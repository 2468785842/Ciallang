/*
 * Copyright (c) 2026/11/1
 *
 * 反编译器 M2 表达式恢复实现（doc/bytecode/decompiler-plan.md §5.2/§5.7）。
 * 见 ExprBuilder.hpp。
 *
 * 管线（CFG 块 + 条件区结构化，M2/M3/M4）：
 *   1. 预检拒绝不可结构化语句（_zf 直写比较族、with、try、
 *      eval、展开实参 → M5）；
 *   2. Pass A：寄存器版本链（LIFO 复用使同一寄存器多版本；参数寄存器预播种）
 *      + 使用点收集；
 *   3. Pass B：按后优化形态表建表达式节点、登记语句 tile（BinaryImm/
 *      CompareTestImm 逆映射、字面量 SetIndex 折叠、具名局部定义转 var 语句、
 *      Push+Call 归并、Load 声明常量 + 安装链折叠为 Decl tile 经宿主装配）；
 *   4. 复合赋值逆融合（M4）：GProp+Op+DProp 模板且中间值单读者（防 getter
 *      计数翻倍的误融合）时合并为 +=/++ 语句；
 *   5. 死代码：无使用的纯定义丢弃，无使用且有副作用的定义降为表达式语句；
 *   6. 物化决策（按 defPc 降序，消费方先决）：单使用且 def→落点干净区间才内联；
 *      干净区间 = 之间无存储/调用/getter，除非中间定义同落点内联（同树序不变）
 *      或本身是恒可内联的字面量；多使用在函数体内物化为临时变量，顶层纯读复制、
 *      否则拒绝（避免复制出 getter/调用次数变化或物化可观察全局状态）；
 *   7. 按 pc 序发射语句（块结构走 M3 区域归约），按 TJS2 优先级加括号。
 */

#include "ExprBuilder.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <map>
#include <ranges>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include <fmt/format.h>

#include "gen/ControlFlowGraph.hpp"
#include "gen/Instruction.hpp"
#include "runtime/Runtime.hpp"
#include "types/Array.hpp"
#include "types/Dictionary.hpp"
#include "types/Octet.hpp"

namespace cial::decompile {
    namespace {
        using inter::Instruction;
        using inter::Label;
        using inter::Register;
        using inter::TacOpCode;

        constexpr u32 INVALID = 0xFFFFFFFFu;

        std::string atomText(Runtime &rt, const Atom atom) {
            if(atom == ATOM_INVALID)
                return {};
            if(const auto *entry = rt.atomTable.get(atom); entry && entry->str)
                return entry->str->toStdStr();
            return {};
        }

        // 打印为 obj.name 形态的成员名：合法标识符且非保留字；否则 ["name"]
        bool isPlainMemberName(const std::string &text) {
            if(text.empty())
                return false;
            const auto isHead = [](const char c) {
                return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
            };
            if(!isHead(text[0]))
                return false;
            for(const char c : text) {
                const bool tail = isHead(c) || (c >= '0' && c <= '9');
                if(!tail)
                    return false;
            }
            static const std::set<std::string> keywords = {
                "function", "class",  "var",    "const",   "if",         "else",       "return",  "while",  "for",
                "do",       "switch", "case",   "default", "break",      "continue",   "throw",   "try",    "catch",
                "finally",  "new",    "delete", "typeof",  "int",        "real",       "string",  "octet",  "void",
                "null",     "true",   "false",  "in",      "instanceof", "with",       "global",  "this",   "super",
                "property", "getter", "setter", "extends", "static",     "invalidate", "isvalid", "export", "import"
            };
            return !keywords.contains(text);
        }

        std::string escapeString(const std::string &text) {
            std::string out = "\"";
            for(const char c : text) {
                switch(c) {
                    case '"':
                        out += "\\\"";
                        break;
                    case '\\':
                        out += "\\\\";
                        break;
                    case '\n':
                        out += "\\n";
                        break;
                    case '\r':
                        out += "\\r";
                        break;
                    case '\t':
                        out += "\\t";
                        break;
                    default:
                        if(static_cast<unsigned char>(c) < 0x20)
                            out += fmt::format("\\x{:02x}", static_cast<unsigned>(c));
                        else
                            out += c;
                }
            }
            return out + "\"";
        }

        // 实数字面量必须可被再解析为 real：整值补 .0（fmt 对 2.0 输出 "2"）；
        // inf/nan 无字面量拼写，拒绝
        bool renderReal(const Real &value, std::string &out) {
            if(!value.isFinite())
                return false;
            out += fmt::format("{}", value.value());
            if(out.find('.') == std::string::npos && out.find('e') == std::string::npos &&
               out.find('E') == std::string::npos)
                out += ".0";
            return true;
        }

        // TJS2 优先级（tjs.y，低 → 高）
        enum class Prec : u8 {
            Assign = 1,
            Ternary = 2,
            Lor = 3,
            Land = 4,
            BitOr = 5,
            BitXor = 6,
            BitAnd = 7,
            Eq = 8,
            Rel = 9,
            Shift = 10,
            Add = 11,
            Mul = 12,
            Unary = 13,
            Postfix = 14,
            Primary = 15,
        };

        struct BinaryOpInfo {
            const char *text;
            Prec prec;
        };

        bool binaryOpInfo(const TacOpCode op, BinaryOpInfo &info) {
            switch(op) {
                case TacOpCode::LOr:
                    info = { "||", Prec::Lor };
                    return true;
                case TacOpCode::LAnd:
                    info = { "&&", Prec::Land };
                    return true;
                case TacOpCode::BOr:
                    info = { "|", Prec::BitOr };
                    return true;
                case TacOpCode::BXor:
                    info = { "^", Prec::BitXor };
                    return true;
                case TacOpCode::BAnd:
                    info = { "&", Prec::BitAnd };
                    return true;
                case TacOpCode::EQ:
                    info = { "==", Prec::Eq };
                    return true;
                case TacOpCode::NEQ:
                    info = { "!=", Prec::Eq };
                    return true;
                case TacOpCode::AbsEQ:
                    info = { "===", Prec::Eq };
                    return true;
                case TacOpCode::AbsNEQ:
                    info = { "!==", Prec::Eq };
                    return true;
                case TacOpCode::LT:
                    info = { "<", Prec::Rel };
                    return true;
                case TacOpCode::LE:
                    info = { "<=", Prec::Rel };
                    return true;
                case TacOpCode::GT:
                    info = { ">", Prec::Rel };
                    return true;
                case TacOpCode::GE:
                    info = { ">=", Prec::Rel };
                    return true;
                case TacOpCode::BlShift:
                    info = { "<<", Prec::Shift };
                    return true;
                case TacOpCode::BrShift:
                    info = { ">>", Prec::Shift };
                    return true;
                case TacOpCode::BurShift:
                    info = { ">>>", Prec::Shift };
                    return true;
                case TacOpCode::Add:
                    info = { "+", Prec::Add };
                    return true;
                case TacOpCode::Sub:
                    info = { "-", Prec::Add };
                    return true;
                case TacOpCode::Mul:
                    info = { "*", Prec::Mul };
                    return true;
                case TacOpCode::Div:
                    info = { "/", Prec::Mul };
                    return true;
                case TacOpCode::Idiv:
                    info = { "\\", Prec::Mul };
                    return true;
                case TacOpCode::Mod:
                    info = { "%", Prec::Mul };
                    return true;
                default:
                    return false;
            }
        }

        const char *binaryText(const TacOpCode op) {
            BinaryOpInfo info{};
            return binaryOpInfo(op, info) ? info.text : "?";
        }

        // 渲染产物：文本 + 表达式优先级（父级据此加括号）
        struct Rendered {
            std::string text;
            Prec prec{ Prec::Primary };
        };

        struct Expr {
            enum class Kind : u8 {
                Void,
                Null,
                Int,
                Real,
                Str,
                OctetLit, // <% hex %>; 字面量（text 预渲染）
                ArrayLit,
                DictLit,
                RegExp,
                LocalName, // 具名局部（含参数）或全局名
                ThisVal,
                GlobalObj,
                SuperVal, // super（Super 指令定义，仅作成员访问/调用接收者）
                Member, // obj.name / obj["name"] / obj[expr]
                Index, // obj[expr]
                Call, // callee(args)
                Transmit, // f(...)
                Unary, // 前缀（- ! ~ + typeof int real string octet # $ isvalid & * delete）
                IncDec, // Inc/Dec 单指令（仅语句形态直接回写时可恢复）
                Binary,
                Ternary, // cond ? a : b（跨块折叠，M3；integer 复用为 else 节点下标）
                Copy, // DLocal/GLocal 匿名拷贝（透明引用源版本）
                FuncLit, // 匿名函数字面量（实参/赋值右值；integer 复用为常量下标）
            };
            Kind kind{};
            Integer integer{};
            Real real{};
            std::string text; // Str 转义文本 / 名字 / 操作符 / 正则模式
            Atom atom{}; // LocalName/Member 的原始 Atom
            u32 a{ INVALID }, b{ INVALID }; // 子表达式节点（Member: a=obj, b=动态名节点）
            u32 srcVersion{ INVALID }; // Copy 的源版本（(reg<<16)|version）
            Vec<u32> items; // ArrayLit 元素 / DictLit 键值对（键,值 交替）
            Vec<u32> args; // Call/Transmit 实参（args[0] = callee）
            bool dotForm{}; // Member 以 .name 打印
            bool globalName{}; // LocalName 为全局名/自由名（非具名局部槽）
        };

        // 语句 tile 寄存器槽：(寄存器下标 << 16) | 版本
        u32 packSlot(const Register reg, const u32 version) { return (static_cast<u32>(reg.index()) << 16) | version; }

        struct Stmt {
            enum class Kind : u8 {
                StoreGlobal, // (var) name = SRC
                StoreMember, // (var) OBJ.M = SRC / OBJ[expr] = SRC（raw 时左值前缀 &）
                StoreIndex, // OBJ[IDX] = SRC
                StoreDeref, // *P = SRC
                StoreLocal, // (var) 具名局部 = SRC（DLocal 目标或具名寄存器直定）
                CompoundAssign, // TARGET OP= RHS / TARGET++（M4 逆融合，单读者守卫）
                Decl, // 声明装配（Load 声明常量 + 安装链 → 宿主装配文本）
                DeleteMember, // delete OBJ.M / OBJ[expr]（dst 无使用）
                ExprStmt, // EXPR;（死副作用定义降级：调用/getter/转换/delete）
                Return, // return SRC / return（Void 且单元末尾时省略）
                ThrowStmt,
                Invalidate,
                DebuggerStmt,
            };
            Kind kind{};
            u32 pc{};
            u32 tacEnd{}; // >0 = 多指令 tile 的显式区间尾（Decl 安装链）
            u32 obj{ INVALID }, idx{ INVALID }, src{ INVALID }; // 寄存器槽
            u32 exprNode{ INVALID }; // StoreLocal 直定 / ExprStmt 的表达式节点
            u32 rhsNode{ INVALID }; // CompoundAssign 的右操作数节点
            u32 constIdx{}; // Decl 的常量下标
            Atom atom{};
            std::string name; // StoreLocal 变量名 / Decl 安装名
            std::string opText; // CompoundAssign 运算符（"++"/"--" 或 "+=" 族）
            bool raw{};
            bool declare{}; // 输出 var 前缀（首次声明）
            bool contextInstall{}; // Decl 安装链含 ChgThis（FunctionDecl 形态）
            u8 declInstall{}; // Decl 安装形态：0=无(零拷贝具名槽) 1=全局 2=成员 3=局部
        };

        struct DefVersion {
            u32 node{ INVALID };
            u32 defPc{};
            Vec<u32> usePcs;
            u32 landing{ INVALID }; // 单使用版本的文本落点（消费语句/定义的 pc）
            bool hidden{}; // VM 内部写入（GMethodC receiver）：不产节点
            bool fused{}; // 逆融合消费（成员读/二目结果）：不物化不复制，landing 保留
            bool materialized{}; // 物化为临时变量（函数体）
            bool duplicated{}; // 顶层纯读复制
            std::string tempName;
        };
    } // namespace

    class ExprBuilder {
    public:
        ExprBuilder(Runtime &rt, vm::Chunk &chunk, const FuncMeta *meta, const bool functionLike, DeclHost *host,
                    bool stateMachine = false) :
            _rt(rt), _chunk(chunk), _meta(meta), _functionLike(functionLike), _host(host), _stateMachine(stateMachine) {
        }

        ExprRecovery run() {
            if(_stateMachine)
                return runStateMachine();
            ExprRecovery recovery;
            if(!precheck(recovery.unsupported))
                return recovery;

            // 声明安装链预扫描（M4）：先于版本链扫描折叠，链上 pc 不计使用/定义，
            // 链内寄存器的链内多次读取（如 global→chg_this→d_prop_raw_c 共用全局
            // 对象寄存器）不进入物化决策
            scanDeclarations();
            buildBlocksAndCondZones();
            scanVersions();
            buildNodesAndStmts();
            if(finishUnsupported(recovery))
                return recovery;

            fuseCompoundAssign();
            foldTernaries();
            collectDeadStatements();
            decideVersions();
            assignTempNames();
            if(finishUnsupported(recovery))
                return recovery;

            if(_blocks.size() <= 1 && _tryHeads.empty())
                emitStatements(recovery.statements);
            else
                emitStructured(recovery.statements);
            finishUnsupported(recovery);
            return recovery;
        }

    private:
        ExprRecovery runStateMachine() {
            ExprRecovery result;
            if(!_functionLike || !_meta || !precheck(result.unsupported)) {
                if(result.unsupported.empty())
                    result.unsupported = "CFG 状态机需要函数局部作用域";
                return result;
            }
            if(_meta->hasCollapse) {
                result.unsupported = "CFG 状态机不支持变长参数保留区";
                return result;
            }
            const auto &code = _chunk.getInstVec();
            size_t registers = std::max<size_t>(_chunk.getRegCount(), _meta->arity);
            for(const auto &inst : code) {
                switch(inst->opcode()) {
                    case TacOpCode::NOP:
                    case TacOpCode::LoadImm:
                    case TacOpCode::This:
                    case TacOpCode::Global:
                    case TacOpCode::DLocal:
                    case TacOpCode::GLocal:
                    case TacOpCode::GThis:
                    case TacOpCode::GGlobal:
                    case TacOpCode::DThis:
                    case TacOpCode::DGlobal:
                    case TacOpCode::Add:
                    case TacOpCode::Sub:
                    case TacOpCode::Mul:
                    case TacOpCode::Div:
                    case TacOpCode::Idiv:
                    case TacOpCode::Mod:
                    case TacOpCode::EQ:
                    case TacOpCode::NEQ:
                    case TacOpCode::AbsEQ:
                    case TacOpCode::AbsNEQ:
                    case TacOpCode::LT:
                    case TacOpCode::LE:
                    case TacOpCode::GT:
                    case TacOpCode::GE:
                    case TacOpCode::BAnd:
                    case TacOpCode::BOr:
                    case TacOpCode::BXor:
                    case TacOpCode::BlShift:
                    case TacOpCode::BrShift:
                    case TacOpCode::BurShift:
                    case TacOpCode::LAnd:
                    case TacOpCode::LOr:
                    case TacOpCode::BNot:
                    case TacOpCode::LNot:
                    case TacOpCode::ChgSign:
                    case TacOpCode::ToInt:
                    case TacOpCode::ToReal:
                    case TacOpCode::Inc:
                    case TacOpCode::Dec:
                    case TacOpCode::TypeOf:
                    case TacOpCode::BinaryImm:
                    case TacOpCode::CompareTest:
                    case TacOpCode::CompareTestImm:
                    case TacOpCode::CmpEQ:
                    case TacOpCode::CmpDEQ:
                    case TacOpCode::CmpLT:
                    case TacOpCode::CmpGT:
                    case TacOpCode::Test:
                    case TacOpCode::Jmp:
                    case TacOpCode::JmpNE:
                    case TacOpCode::JmpE:
                    case TacOpCode::Ret:
                    case TacOpCode::Throw:
                        break;
                    case TacOpCode::Load: {
                        const auto kind = _chunk.getConstants()[inst->getOp2().value<ConstIdx>().index()].type();
                        if(kind == ConstantType::None || kind == ConstantType::Real || kind == ConstantType::Atom)
                            break;
                    }
                        [[fallthrough]];
                    default:
                        result.unsupported = "CFG 状态机尚不支持该指令的生命周期/接收者协议";
                        return result;
                }
                forEachUse(*inst, [&](Register r) { registers = std::max(registers, size_t(r.index()) + 1); });
                forEachDef(*inst, [&](Register r, bool) { registers = std::max(registers, size_t(r.index()) + 1); });
                for(const auto &operand : { inst->getOp1(), inst->getOp2(), inst->getOp3() })
                    if(operand.type() == inter::Operand::Type::Atom)
                        _usedNames.insert(atomText(_rt, operand.value<Atom>()));
            }
            for(const auto &handler : _chunk.getThrowHandlers()) {
                if(handler.tryStart >= handler.tryEnd || handler.tryEnd >= code.size()) {
                    result.unsupported = "CFG 异常区域或 catch 入口越界";
                    return result;
                }
                if(handler.exValueReg != vm::ThrowHandler::exValueRegNone)
                    registers = std::max(registers, size_t(handler.exValueReg) + 1);
            }
            if(registers >= vm::ThrowHandler::exValueRegNone) {
                result.unsupported = "CFG 状态机寄存器数量越界";
                return result;
            }
            Vec<std::string> parameters;
            for(u16 i = 0; i < _meta->arity; ++i) {
                auto name = localNameAt(Register{ i }, 0);
                if(name.empty())
                    name = fmt::format("a{}", i);
                _usedNames.insert(name);
                parameters.push_back(name);
            }
            std::set<std::string> uniqueParameters(parameters.begin(), parameters.end());
            if(uniqueParameters.size() != parameters.size()) {
                result.unsupported = "CFG 状态机参数名存在冲突";
                return result;
            }
            for(const auto &inst : code) {
                const auto op = inst->opcode();
                if(op == TacOpCode::GThis || op == TacOpCode::GGlobal || op == TacOpCode::DThis ||
                   op == TacOpCode::DGlobal) {
                    if(uniqueParameters.contains(atomText(_rt, inst->getOp1().value<Atom>()))) {
                        result.unsupported = "CFG 状态机参数与动态名字查找冲突";
                        return result;
                    }
                }
            }
            for(const auto &local : _meta->localVars)
                _usedNames.insert(atomText(_rt, local.identifier));
            for(size_t i = 0; i < registers; ++i)
                _machineNames.push_back(nextTempName());
            const auto state = nextTempName();
            const auto flag = nextTempName();
            const auto exception = nextTempName();
            buildBlocksAndCondZones();
            scanVersions();
            buildNodesAndStmts();
            if(finishUnsupported(result))
                return result;
            auto &out = result.statements;
            for(size_t i = 0; i < registers; ++i)
                out.push_back({ .pc = 0,
                                .text = fmt::format("var {} = {};", _machineNames[i],
                                                    i < parameters.size() ? parameters[i] : "void"),
                                .synthetic = true });
            // 参数备份到物理寄存器后立即清除入口参数，避免旧对象被额外保活。
            for(const auto &parameter : parameters)
                out.push_back({ .pc = 0, .text = fmt::format("{} = void;", parameter), .synthetic = true });
            out.push_back({ .pc = 0, .text = fmt::format("var {} = 0;", state), .synthetic = true });
            out.push_back({ .pc = 0, .text = fmt::format("var {} = 0;", flag), .synthetic = true });
            out.push_back({ .pc = 0, .text = "while (true) {", .synthetic = true });
            out.push_back({ .pc = 0, .text = fmt::format("    switch ({}) {{", state), .synthetic = true });
            std::stable_sort(_stmts.begin(), _stmts.end(), [](const Stmt &a, const Stmt &b) { return a.pc < b.pc; });
            _emitCursor = 0;
            for(size_t bi = 0; bi < _blocks.size(); ++bi) {
                const auto &block = _blocks[bi];
                out.push_back({ .pc = static_cast<u32>(block.start),
                                .text = fmt::format("        case {}:", block.start),
                                .synthetic = true });
                for(u32 pc = static_cast<u32>(block.start); pc < block.end; ++pc) {
                    const auto handler = _chunk.findThrowHandler(pc);
                    const u32 indent = handler ? 4 : 3;
                    if(handler)
                        out.push_back({ .pc = pc, .text = "            try {", .synthetic = true });
                    emitAt(pc, indent, out);
                    // 在原比较/Test 指令处求值；不能移到跳转处再求值，否则
                    // 异常的所属 try 区域、部分寄存器写入和副作用都会漂移。
                    const auto condition = _condNodeAtPc.find(pc);
                    if(condition != _condNodeAtPc.end())
                        out.push_back({ .pc = pc,
                                        .text = fmt::format("{}{} = !(!({}));", indentText(indent), flag,
                                                            renderNode(condition->second).text) });
                    if(handler) {
                        out.push_back({ .pc = pc,
                                        .text = fmt::format("            }} catch ({}) {{", exception),
                                        .synthetic = true });
                        if(handler->exValueReg != vm::ThrowHandler::exValueRegNone)
                            out.push_back({ .pc = pc,
                                            .text = fmt::format("                {} = {};",
                                                                _machineNames[handler->exValueReg], exception),
                                            .synthetic = true });
                        out.push_back({ .pc = pc,
                                        .text = fmt::format("                {} = void;", exception),
                                        .synthetic = true });
                        out.push_back({ .pc = pc,
                                        .text = fmt::format("                {} = {};", state, handler->tryEnd),
                                        .synthetic = true });
                        out.push_back({ .pc = pc, .text = "                continue;", .synthetic = true });
                        out.push_back({ .pc = pc, .text = "            }", .synthetic = true });
                    }
                }
                const auto last = static_cast<u32>(block.end - 1);
                const auto op = code[last]->opcode();
                if(op == TacOpCode::Ret || op == TacOpCode::Throw)
                    continue;
                u32 target = static_cast<u32>(block.end);
                if(op == TacOpCode::Jmp || op == TacOpCode::JmpNE || op == TacOpCode::JmpE) {
                    target = static_cast<u32>(code[last]->getOp1().value<Label>().address());
                    if(target > code.size()) {
                        fail("CFG 状态机跳转目标越界");
                        break;
                    }
                }
                std::string next = std::to_string(target);
                if(op == TacOpCode::JmpNE || op == TacOpCode::JmpE) {
                    const auto condition = zoneNodeFor(bi);
                    if(condition == INVALID) {
                        fail("CFG 状态机条件缺失");
                        break;
                    }
                    next = fmt::format("({}) ? {} : {}", flag,
                                       op == TacOpCode::JmpE ? target : static_cast<u32>(block.end),
                                       op == TacOpCode::JmpE ? static_cast<u32>(block.end) : target);
                }
                out.push_back({ .pc = last,
                                .text = fmt::format("            {} = {};", state, next),
                                .synthetic = op != TacOpCode::Jmp && op != TacOpCode::JmpNE && op != TacOpCode::JmpE });
                out.push_back({ .pc = last, .text = "            continue;", .synthetic = true });
            }
            out.push_back(
                { .pc = static_cast<u32>(code.size()), .text = "        default: return;", .synthetic = true });
            out.push_back({ .pc = static_cast<u32>(code.size()), .text = "    }", .synthetic = true });
            out.push_back({ .pc = static_cast<u32>(code.size()), .text = "}", .synthetic = true });
            finishUnsupported(result);
            return result;
        }

        bool finishUnsupported(ExprRecovery &recovery) const {
            if(_unsupported.empty())
                return false;
            recovery.unsupported = _unsupported;
            recovery.statements.clear();
            return true;
        }

        // ── 预检：不可结构化语句（M5）如实拒绝 ──
        bool precheck(std::string &reason) {
            // 函数内 try/catch 交由 precomputeTryRegions 校验（失败落回状态机）；
            // 顶层无函数局部作用域，维持批次拒绝
            if(!_stateMachine && !_functionLike && !_chunk.getThrowHandlers().empty()) {
                reason = "try/catch 顶层结构化属 M5（throw handler 表非空）";
                return false;
            }
            auto &instructions = _chunk.getInstVec();
            for(size_t pc = 0; pc < instructions.size(); ++pc) {
                const auto op = instructions[pc]->opcode();
                switch(op) {
                    // 跳转/Test/TestVoid 交由块条件区识别（M3 结构化）；JmpE 由
                    // switch 派发链认领（precomputeSwitches，未认领即拒）；
                    // 直写 _zf 仅接纳相邻假跳；其它 flag 生命周期保守拒绝
                    case TacOpCode::CmpEQ:
                    case TacOpCode::CmpDEQ:
                    case TacOpCode::CmpLT:
                    case TacOpCode::CmpGT:
                        if(pc + 1 >= instructions.size() ||
                           (instructions[pc + 1]->opcode() != TacOpCode::JmpNE &&
                            !(instructions[pc + 1]->opcode() == TacOpCode::JmpE &&
                              (_stateMachine || instructions[pc + 1]->getOp1().value<Label>().address() < pc)))) {
                            reason = "_zf 直写比较需要相邻 JmpNE（其它 flag 形态属 M5）";
                            return false;
                        }
                        break;
                    case TacOpCode::WithPush:
                    case TacOpCode::WithPop:
                    case TacOpCode::GetWith:
                        reason = "with 语句结构化属 M3";
                        return false;
                    case TacOpCode::EvalStr:
                        reason = "动态求值（后缀 !）可按名字访问局部，恢复保守拒绝";
                        return false;
                    case TacOpCode::CallExpand:
                    case TacOpCode::CollapseArgs:
                        reason = "展开实参/收拢参数恢复属后续批次";
                        return false;
                    default:
                        break;
                }
            }
            return true;
        }

        // ── M3：CFG 块与条件区 ──
        // 条件块尾部形态（优化器 CompareTestFusion 保证相邻）：TestVoid? Test?
        // CompareTest(+Imm)? JmpNE ——TestVoid/Test/比较的 pc 从语句流剔除，节点
        // 交由块条件；do-while 尾块为 [... 比较, JmpNE exit, Jmp header]
        struct CondZone {
            u32 condNode{ INVALID }; // build 期填充
            u32 firstPc{ INVALID }; // 条件区起始 pc（此前 = 块前缀语句）
        };

        void buildBlocksAndCondZones() {
            inter::ControlFlowGraph cfg;
            _blocks = cfg.buildCFG(_chunk);
            _dominators = cfg.getDominators();
            _condZones.assign(_blocks.size(), CondZone{});
            for(size_t bi = 0; bi < _blocks.size(); ++bi) {
                const auto &block = _blocks[bi];
                auto &code = _chunk.getInstVec();
                const size_t lastPc = block.end - 1;
                const auto lastOp = code[lastPc]->opcode();
                // do-while 尾：JmpNE 后跟 Jmp 回边，两者同块
                if(lastOp == TacOpCode::Jmp) {
                    const auto target = code[lastPc]->getOp1().value<Label>().address();
                    if(target < block.start && lastPc > block.start && code[lastPc - 1]->opcode() == TacOpCode::JmpNE) {
                        const size_t condJumpPc = lastPc - 1;
                        markCondCompare(block, condJumpPc, bi);
                    }
                    continue;
                }
                if(lastOp != TacOpCode::JmpNE &&
                   !(lastOp == TacOpCode::JmpE &&
                     (_stateMachine || code[lastPc]->getOp1().value<Label>().address() <= block.start)))
                    continue;
                markCondCompare(block, lastPc, bi);
            }
            if(!_stateMachine) {
                precomputeLoops();
                precomputeSwitches();
                precomputeTryRegions();
                precomputeLoopStorage();
            }
        }

        // 从 condJumpPc 向前收集条件区（TestVoid? Test? CompareTest(+Imm)? JmpNE）
        void markCondCompare(const inter::BasicBlock &block, const size_t condJumpPc, const size_t blockIdx) {
            auto &code = _chunk.getInstVec();
            size_t first = condJumpPc;
            // JmpNE 前可跟 Test（布尔值判真）或 TestVoid+Test（默认参数守卫）
            if(first > block.start) {
                const auto prevOp = code[first - 1]->opcode();
                if(prevOp == TacOpCode::Test || prevOp == TacOpCode::TestVoid) {
                    --first;
                    // TestVoid + Test 链
                    if(first > block.start && code[first - 1]->opcode() == TacOpCode::TestVoid &&
                       code[first]->opcode() == TacOpCode::Test)
                        --first;
                }
            }
            // 再往前是 CompareTest(+Imm)（比较结果仅供 _zf/死寄存器）
            if(first > block.start) {
                const auto prevOp = code[first - 1]->opcode();
                if(prevOp == TacOpCode::CompareTest || prevOp == TacOpCode::CompareTestImm ||
                   prevOp == TacOpCode::CmpEQ || prevOp == TacOpCode::CmpDEQ || prevOp == TacOpCode::CmpLT ||
                   prevOp == TacOpCode::CmpGT)
                    --first;
            }
            auto &zone = _condZones[blockIdx];
            zone.firstPc = static_cast<u32>(first);
            for(size_t pc = first; pc <= condJumpPc; ++pc)
                _condPcs.insert(static_cast<u32>(pc));
        }

        // ── Pass A：寄存器版本链 + 使用点收集 ──
        void scanVersions() {
            _current.assign(std::max<size_t>(_chunk.getRegCount() + 8, 64), INVALID);
            if(_stateMachine) {
                _instPc = 0;
                for(u16 i = 0; i < _machineNames.size(); ++i) {
                    pushDef(Register{ i }, false);
                    _defs[i].back().node = makeNamed(Expr::Kind::LocalName, _machineNames[i], false);
                }
            }
            // 参数寄存器 0..arity-1 由调用方装入（帧约定），预播种具名版本
            if(_meta && !_stateMachine) {
                _instPc = 0;
                for(u16 i = 0; i < static_cast<u16>(_meta->arity); ++i) {
                    const auto reg = Register{ i };
                    auto name = localNameAt(reg, 0);
                    if(name.empty())
                        name = fmt::format("a{}", i);
                    pushDef(reg, false);
                    _defs[i].back().node = makeNamed(Expr::Kind::LocalName, name, false);
                    _declaredLocals.emplace(i, name);
                }
            }
            auto &instructions = _chunk.getInstVec();
            for(size_t pc = 0; pc < instructions.size(); ++pc) {
                _instPc = static_cast<u32>(pc);
                // catch 入口：异常槽由 VM 在 unwind 时写入（异常发生时寄存器处于
                // 指令中间状态，方案 §5.3），从此 pc 起重绑定为 catch 变量名节点；
                // 体区内的同号版本不受影响。版本下标记录供 Pass B 恢复游标
                for(auto &[tryStart, region] : _tryHeads) {
                    if(region.tryEnd != _instPc || region.exValueReg == vm::ThrowHandler::exValueRegNone)
                        continue;
                    pushDef(Register{ region.exValueReg }, false);
                    region.exVersion = static_cast<u32>(_defs[region.exValueReg].size() - 1);
                    _defs[region.exValueReg].back().node = makeNamed(Expr::Kind::LocalName, region.catchName, false);
                }
                // 声明安装链：整体跳过（链内读写在 Decl tile 内消费）
                if(_foldedPcs.contains(_instPc))
                    continue;
                const Instruction &inst = *instructions[pc];
                // 同寄存器自拷贝（优化器残留）透明：不计使用，只推进版本
                const bool selfCopy = (inst.opcode() == TacOpCode::DLocal || inst.opcode() == TacOpCode::GLocal) &&
                    inst.getOp1().value<Register>() == inst.getOp2().value<Register>();
                // 先使用后定义：Inc/Dec、ChkIns 的操作数寄存器读旧写新
                if(!selfCopy)
                    forEachUse(inst, [&](const Register reg) { countUse(reg); });
                forEachDef(inst, [&](const Register reg, const bool hidden) { pushDef(reg, hidden); });
            }
        }

        void countUse(const Register reg) {
            const auto version = currentVersion(reg);
            if(version == INVALID) {
                fail(fmt::format("pc {} 使用未定义寄存器 %{}", _instPc, reg.index()));
                return;
            }
            _defs[reg.index()][version].usePcs.push_back(_instPc);
        }

        void pushDef(const Register reg, const bool hidden) {
            const auto index = reg.index();
            if(index >= _defs.size())
                _defs.resize(index + 1);
            _defs[index].push_back(DefVersion{ .defPc = _instPc, .hidden = hidden });
            _current[index] = static_cast<u32>(_defs[index].size() - 1);
        }

        [[nodiscard]] u32 currentVersion(const Register reg) const {
            const auto index = reg.index();
            if(index >= _current.size())
                return INVALID;
            return _current[index];
        }

        [[nodiscard]] const DefVersion &versionOf(const u32 slot) const { return _defs[slot >> 16][slot & 0xFFFF]; }

        // 指令的寄存器使用槽（语义读；dst 与隐藏定义不计）
        void forEachUse(const Instruction &inst, auto &&callback) const {
            switch(inst.opcode()) {
                case TacOpCode::ToInt:
                case TacOpCode::ToReal:
                case TacOpCode::ToString:
                case TacOpCode::TypeOf:
                case TacOpCode::Asc:
                case TacOpCode::Chr:
                case TacOpCode::LNot:
                case TacOpCode::ChgSign:
                case TacOpCode::ChkInv:
                case TacOpCode::BNot:
                case TacOpCode::ToNumber:
                case TacOpCode::ToOctet:
                case TacOpCode::Inv:
                case TacOpCode::Throw:
                case TacOpCode::Ret:
                case TacOpCode::Push:
                case TacOpCode::Inc:
                case TacOpCode::Dec:
                case TacOpCode::DerefGet:
                case TacOpCode::DLocal:
                case TacOpCode::GLocal:
                    callback(inst.getOp1().value<Register>());
                    break;
                case TacOpCode::CmpEQ:
                case TacOpCode::CmpDEQ:
                case TacOpCode::CmpLT:
                case TacOpCode::CmpGT:
                case TacOpCode::ChkIns:
                case TacOpCode::DerefSet:
                    callback(inst.getOp1().value<Register>());
                    callback(inst.getOp2().value<Register>());
                    break;
                case TacOpCode::ChgThis:
                case TacOpCode::DGlobal:
                case TacOpCode::DThis:
                    callback(inst.getOp2().value<Register>());
                    break;
                case TacOpCode::Add:
                case TacOpCode::Sub:
                case TacOpCode::Mul:
                case TacOpCode::Div:
                case TacOpCode::Idiv:
                case TacOpCode::Mod:
                case TacOpCode::BXor:
                case TacOpCode::BOr:
                case TacOpCode::BAnd:
                case TacOpCode::BlShift:
                case TacOpCode::BrShift:
                case TacOpCode::BurShift:
                case TacOpCode::EQ:
                case TacOpCode::NEQ:
                case TacOpCode::AbsEQ:
                case TacOpCode::AbsNEQ:
                case TacOpCode::LT:
                case TacOpCode::LE:
                case TacOpCode::GT:
                case TacOpCode::GE:
                case TacOpCode::LAnd:
                case TacOpCode::LOr:
                case TacOpCode::GProp:
                case TacOpCode::GPropRaw:
                case TacOpCode::DelProp:
                case TacOpCode::GetIndex:
                case TacOpCode::CompareTest:
                case TacOpCode::BinaryImm:
                case TacOpCode::CompareTestImm:
                case TacOpCode::GPropC:
                case TacOpCode::GPropRawC:
                case TacOpCode::DelPropC:
                case TacOpCode::GMethodC:
                    callback(inst.getOp1().value<Register>());
                    if(isRegisterOperand2(inst.opcode()))
                        callback(inst.getOp2().value<Register>());
                    break;
                case TacOpCode::DProp:
                case TacOpCode::DPropRaw:
                case TacOpCode::SetIndex:
                    callback(inst.getOp1().value<Register>());
                    callback(inst.getOp2().value<Register>());
                    callback(inst.getOp3().value<Register>());
                    break;
                case TacOpCode::DPropC:
                case TacOpCode::DPropRawC:
                    callback(inst.getOp1().value<Register>());
                    callback(inst.getOp3().value<Register>());
                    break;
                case TacOpCode::Call:
                    callback(inst.getOp2().value<Register>());
                    break; // 实参由 Push 序列供给
                case TacOpCode::CallOmit:
                    // 手写子类：实参由 dst/func 字段承载，_ops 为空
                    callback(static_cast<const inter::CallOmit &>(inst).func);
                    break;
                case TacOpCode::CallV:
                case TacOpCode::CallMethodV: {
                    const auto &call = static_cast<const inter::CallV &>(inst);
                    callback(call.func);
                    for(const auto arg : call.args)
                        callback(arg);
                    break;
                }
                default:
                    break;
            }
        }

        // op2 为寄存器（而非 Atom/Integer）的三槽指令族
        [[nodiscard]] static bool isRegisterOperand2(const TacOpCode op) {
            switch(op) {
                case TacOpCode::Add:
                case TacOpCode::Sub:
                case TacOpCode::Mul:
                case TacOpCode::Div:
                case TacOpCode::Idiv:
                case TacOpCode::Mod:
                case TacOpCode::BXor:
                case TacOpCode::BOr:
                case TacOpCode::BAnd:
                case TacOpCode::BlShift:
                case TacOpCode::BrShift:
                case TacOpCode::BurShift:
                case TacOpCode::EQ:
                case TacOpCode::NEQ:
                case TacOpCode::AbsEQ:
                case TacOpCode::AbsNEQ:
                case TacOpCode::LT:
                case TacOpCode::LE:
                case TacOpCode::GT:
                case TacOpCode::GE:
                case TacOpCode::LAnd:
                case TacOpCode::LOr:
                case TacOpCode::GProp:
                case TacOpCode::GPropRaw:
                case TacOpCode::DelProp:
                case TacOpCode::GetIndex:
                case TacOpCode::CompareTest:
                    return true;
                case TacOpCode::BinaryImm:
                case TacOpCode::CompareTestImm:
                case TacOpCode::GPropC:
                case TacOpCode::GPropRawC:
                case TacOpCode::DelPropC:
                case TacOpCode::GMethodC:
                    return false;
                default:
                    return false;
            }
        }

        // 指令的寄存器定义槽（hidden = VM 内部写入，不产打印节点）
        void forEachDef(const Instruction &inst, auto &&callback) const {
            switch(inst.opcode()) {
                case TacOpCode::Global:
                case TacOpCode::This:
                case TacOpCode::Load:
                case TacOpCode::LoadImm:
                case TacOpCode::NewArray:
                case TacOpCode::NewDict:
                case TacOpCode::NewRegExp:
                    callback(inst.getOp1().value<Register>(), false);
                    break;
                case TacOpCode::ToInt:
                case TacOpCode::ToReal:
                case TacOpCode::ToString:
                case TacOpCode::TypeOf:
                case TacOpCode::Asc:
                case TacOpCode::Chr:
                case TacOpCode::LNot:
                case TacOpCode::ChgSign:
                case TacOpCode::ChkInv:
                case TacOpCode::BNot:
                case TacOpCode::ToNumber:
                case TacOpCode::ToOctet:
                case TacOpCode::TestVoid:
                case TacOpCode::DLocal:
                case TacOpCode::GLocal:
                case TacOpCode::GRefRaw:
                case TacOpCode::DerefGet:
                case TacOpCode::GGlobal:
                case TacOpCode::GThis:
                    callback(inst.getOp2().value<Register>(), false);
                    break;
                case TacOpCode::ChkIns:
                case TacOpCode::Inc:
                case TacOpCode::Dec:
                    callback(inst.getOp1().value<Register>(), false);
                    break;
                case TacOpCode::ChgThis:
                    callback(inst.getOp1().value<Register>(), true);
                    break;
                case TacOpCode::Super:
                    callback(inst.getOp1().value<Register>(), false);
                    break;
                case TacOpCode::Add:
                case TacOpCode::Sub:
                case TacOpCode::Mul:
                case TacOpCode::Div:
                case TacOpCode::Idiv:
                case TacOpCode::Mod:
                case TacOpCode::BXor:
                case TacOpCode::BOr:
                case TacOpCode::BAnd:
                case TacOpCode::BlShift:
                case TacOpCode::BrShift:
                case TacOpCode::BurShift:
                case TacOpCode::EQ:
                case TacOpCode::NEQ:
                case TacOpCode::AbsEQ:
                case TacOpCode::AbsNEQ:
                case TacOpCode::LT:
                case TacOpCode::LE:
                case TacOpCode::GT:
                case TacOpCode::GE:
                case TacOpCode::LAnd:
                case TacOpCode::LOr:
                case TacOpCode::GProp:
                case TacOpCode::GPropRaw:
                case TacOpCode::DelProp:
                case TacOpCode::GetIndex:
                case TacOpCode::CompareTest:
                case TacOpCode::CompareTestImm:
                case TacOpCode::BinaryImm:
                case TacOpCode::GPropC:
                case TacOpCode::GPropRawC:
                case TacOpCode::DelPropC:
                case TacOpCode::GMethodC:
                    callback(inst.getOp3().value<Register>(), false);
                    break;
                case TacOpCode::CallV:
                case TacOpCode::CallMethodV:
                    callback(static_cast<const inter::CallV &>(inst).dst, false);
                    break;
                case TacOpCode::Call:
                    callback(inst.getOp1().value<Register>(), false);
                    break;
                case TacOpCode::CallOmit:
                    callback(static_cast<const inter::CallOmit &>(inst).dst, false);
                    break;
                default:
                    break;
            }
        }

        // ── Pass B：构建表达式节点与语句 tile ──
        // 版本游标按 pc 回放：操作数读取"本指令之前"的版本，定义落到 Pass A 在
        // 本 pc 登记的版本（definedVersion），指令处理完后提交推进
        void buildNodesAndStmts() {
            auto &instructions = _chunk.getInstVec();
            _impureOrStore.assign(instructions.size(), false);
            std::fill(_current.begin(), _current.end(), INVALID);
            if(_stateMachine) {
                for(size_t i = 0; i < _machineNames.size(); ++i)
                    _current[i] = 0;
            } else if(_meta)
                for(u16 i = 0; i < static_cast<u16>(_meta->arity); ++i)
                    _current[i] = 0;
            for(size_t pc = 0; pc < instructions.size(); ++pc) {
                _instPc = static_cast<u32>(pc);
                // catch 入口：恢复 Pass A 登记的异常槽版本（游标重置后参数之外
                // 的版本不延续，catch 体首条指令可能读取异常值）
                for(const auto &[tryStart, region] : _tryHeads) {
                    if(region.tryEnd != _instPc || region.exVersion == INVALID)
                        continue;
                    _current[region.exValueReg] = region.exVersion;
                }
                const Instruction &inst = *instructions[pc];
                // 声明安装链：随 Decl tile 折叠，不产节点/语句，也不视为存储
                if(_foldedPcs.contains(_instPc)) {
                    _impureOrStore[pc] = false;
                    continue;
                }
                _impureOrStore[pc] = isImpureOrStore(inst);
                // 跳转是求值时机屏障：跨分支内联会把调用/getter 移入/移出分支，
                // 改变可观察的执行次数
                if(inst.opcode() == TacOpCode::Jmp || inst.opcode() == TacOpCode::JmpNE)
                    _impureOrStore[pc] = true;
                buildInstruction(inst);
                if(_stateMachine)
                    forEachDef(inst, [&](Register reg, bool hidden) {
                        if(hidden)
                            return;
                        const auto v = definedVersion(reg);
                        if(v != INVALID && _defs[reg.index()][v].node != INVALID) {
                            const auto &node = _nodes[_defs[reg.index()][v].node];
                            if(node.kind != Expr::Kind::LocalName || node.text != _machineNames[reg.index()])
                                noteNamedDef(reg);
                        }
                    });
                forEachDef(inst, [&](const Register reg, bool) {
                    const auto version = definedVersion(reg);
                    if(version != INVALID)
                        _current[reg.index()] = version;
                });
            }
        }

        // Pass A 在当前 pc 为 reg 登记的版本（同 pc 同寄存器至多一条真实定义；
        // 参数预播种 defPc 也为 0，取最后一条即真实定义）
        [[nodiscard]] u32 definedVersion(const Register reg) const {
            const auto index = reg.index();
            if(index >= _defs.size())
                return INVALID;
            const auto &versions = _defs[index];
            for(size_t v = versions.size(); v > 0; --v)
                if(versions[v - 1].defPc == _instPc)
                    return static_cast<u32>(v - 1);
            return INVALID;
        }

        // 是否影响别名/求值次数/求值顺序（内联干净区间判定的基础）
        [[nodiscard]] static bool isImpureOrStore(const Instruction &inst) {
            switch(inst.opcode()) {
                case TacOpCode::DGlobal:
                case TacOpCode::DThis:
                case TacOpCode::DProp:
                case TacOpCode::DPropC:
                case TacOpCode::DPropRaw:
                case TacOpCode::DPropRawC:
                case TacOpCode::SetIndex:
                case TacOpCode::DerefSet:
                case TacOpCode::Inc:
                case TacOpCode::Dec:
                case TacOpCode::Throw:
                case TacOpCode::Ret:
                case TacOpCode::Inv:
                case TacOpCode::Debugger:
                case TacOpCode::GProp:
                case TacOpCode::GPropC:
                case TacOpCode::GPropRaw:
                case TacOpCode::GPropRawC:
                case TacOpCode::DerefGet:
                case TacOpCode::DelProp:
                case TacOpCode::DelPropC:
                case TacOpCode::Call:
                case TacOpCode::CallV:
                case TacOpCode::CallMethodV:
                case TacOpCode::CallOmit:
                case TacOpCode::ToInt:
                case TacOpCode::ToReal:
                case TacOpCode::ToString:
                case TacOpCode::ToNumber:
                case TacOpCode::ToOctet:
                case TacOpCode::Asc:
                case TacOpCode::Chr:
                    return true;
                default:
                    return false;
            }
        }

        // 语句 tile 的寄存器槽登记
        [[nodiscard]] u32 slotOf(const Register reg) {
            const auto version = currentVersion(reg);
            if(version == INVALID) {
                fail(fmt::format("pc {} 使用未定义寄存器 %{}", _instPc, reg.index()));
                return INVALID;
            }
            return packSlot(reg, version);
        }

        // 表达式操作数解析到节点
        [[nodiscard]] u32 operandNode(const Register reg) {
            const auto version = currentVersion(reg);
            if(version == INVALID) {
                fail(fmt::format("pc {} 使用未定义寄存器 %{}", _instPc, reg.index()));
                return INVALID;
            }
            return _defs[reg.index()][version].node;
        }

        void buildInstruction(const Instruction &inst) {
            switch(inst.opcode()) {
                case TacOpCode::NOP:
                case TacOpCode::PopN:
                case TacOpCode::ChgThis:
                    break;
                case TacOpCode::Push:
                    // 转译路径：实参寄存器挂起，等 Call 归并（透明化该 pc）
                    _impureOrStore[_instPc] = false;
                    _pending.push_back({ _instPc, operandNode(inst.getOp1().value<Register>()) });
                    break;
                case TacOpCode::Test:
                case TacOpCode::TestVoid:
                    if(!_condPcs.contains(_instPc)) {
                        fail("条件上下文出现在非块尾位置");
                        break;
                    }
                    if(inst.opcode() == TacOpCode::TestVoid) {
                        // 守卫谓词：src 是否为 void（默认参数守卫，语句层暂不展开）
                        const auto voidLeaf = makeSimple(Expr::Kind::Void);
                        _condNodeAtPc[_instPc] =
                            makeBinary(TacOpCode::EQ, operandNode(inst.getOp1().value<Register>()), voidLeaf);
                    } else {
                        _condNodeAtPc[_instPc] = operandNode(inst.getOp1().value<Register>());
                    }
                    break;
                case TacOpCode::Jmp:
                case TacOpCode::JmpNE:
                case TacOpCode::JmpE:
                    break; // 结构跳转：条件节点由比较/Test pc 捕获（JmpE 属 switch 派发链）
                case TacOpCode::Debugger:
                    pushStmt(Stmt{ .kind = Stmt::Kind::DebuggerStmt, .pc = _instPc });
                    break;
                case TacOpCode::Load: {
                    const auto dst = inst.getOp1().value<Register>();
                    const auto constIdx = inst.getOp2().value<ConstIdx>();
                    if(const auto it = _declSpecs.find(_instPc); it != _declSpecs.end()) {
                        emitDeclaration(it->second);
                        break;
                    }
                    const auto &constant = _chunk.getConstants()[constIdx.index()];
                    if(constant.type() == ConstantType::FuncMeta) {
                        // 无安装链的函数常量 = 表达式位置字面量（实参/赋值右值）。
                        // 恒按匿名渲染（具名函数表达式的名字绑定差异未验证）；
                        // 类/property 无表达式字面量语法，如实拒绝
                        if(!atomText(_rt, constant.value<FuncMeta *>()->name).empty() &&
                           (!_host || !_host->isAnonymousExpression(*constant.value<FuncMeta *>()))) {
                            fail("具名函数表达式恢复属后续批次");
                            break;
                        }
                        if(!_host) {
                            fail("匿名函数字面量缺少装配宿主");
                            break;
                        }
                        const auto node = newNode(Expr::Kind::FuncLit);
                        _nodes[node].integer = static_cast<Integer>(constIdx.index());
                        defineNode(dst, node);
                        noteNamedDef(dst);
                        break;
                    }
                    if(constant.type() == ConstantType::ClassMeta || constant.type() == ConstantType::PropMeta) {
                        fail("类/property 无表达式位置字面量语法");
                        break;
                    }
                    defineNode(dst, loadConstNode(constIdx));
                    noteNamedDef(dst);
                    break;
                }
                case TacOpCode::LoadImm:
                    defineNode(inst.getOp1().value<Register>(),
                               makeLeaf(Expr::Kind::Int, inst.getOp2().value<Integer>()));
                    noteNamedDef(inst.getOp1().value<Register>());
                    break;
                case TacOpCode::Global:
                    defineNode(inst.getOp1().value<Register>(), makeSimple(Expr::Kind::GlobalObj));
                    break;
                case TacOpCode::This:
                    defineNode(inst.getOp1().value<Register>(), makeSimple(Expr::Kind::ThisVal));
                    break;
                case TacOpCode::Super:
                    // super（M4 类壳）：superclass 引用，仅作成员访问/调用接收者
                    defineNode(inst.getOp1().value<Register>(), makeSimple(Expr::Kind::SuperVal));
                    break;
                case TacOpCode::GGlobal:
                case TacOpCode::GThis: {
                    const auto atom = inst.getOp1().value<Atom>();
                    defineNode(inst.getOp2().value<Register>(),
                               makeNamed(Expr::Kind::LocalName, atomText(_rt, atom), true, atom));
                    break;
                }
                case TacOpCode::DGlobal:
                case TacOpCode::DThis: {
                    Stmt stmt{ .kind = Stmt::Kind::StoreGlobal,
                               .pc = _instPc,
                               .src = slotOf(inst.getOp2().value<Register>()),
                               .atom = inst.getOp1().value<Atom>() };
                    const auto name = atomText(_rt, stmt.atom);
                    if(!_functionLike && inst.opcode() == TacOpCode::DGlobal && !_declaredGlobals.contains(name)) {
                        stmt.declare = true;
                        _declaredGlobals.insert(name);
                    }
                    pushStmt(std::move(stmt));
                    break;
                }
                case TacOpCode::DLocal:
                case TacOpCode::GLocal: {
                    const auto dst = inst.getOp2().value<Register>();
                    const auto src = inst.getOp1().value<Register>();
                    const auto name = localNameAt(dst, _instPc + 1); // 同 noteNamedDef：定义后的名字
                    if(!name.empty()) {
                        // 具名局部赋值（IRGen 的 var 零拷贝路径由 noteNamedDef 处理，
                        // 这里是"作用域已有 → DLocal"的重赋值/拷入路径）
                        Stmt stmt{ .kind = Stmt::Kind::StoreLocal, .pc = _instPc, .src = slotOf(src), .name = name };
                        stmt.declare = declareLocal(dst.index(), name);
                        pushStmt(std::move(stmt));
                        defineNode(dst, makeNamed(Expr::Kind::LocalName, name, false));
                    } else {
                        // 匿名拷贝：透明节点
                        const auto node = newNode(Expr::Kind::Copy);
                        const auto version = currentVersion(src);
                        _nodes[node].srcVersion = version == INVALID ? INVALID : packSlot(src, version);
                        defineNode(dst, node);
                    }
                    break;
                }
                case TacOpCode::Add:
                case TacOpCode::Sub:
                case TacOpCode::Mul:
                case TacOpCode::Div:
                case TacOpCode::Idiv:
                case TacOpCode::Mod:
                case TacOpCode::BXor:
                case TacOpCode::BOr:
                case TacOpCode::BAnd:
                case TacOpCode::BlShift:
                case TacOpCode::BrShift:
                case TacOpCode::BurShift:
                case TacOpCode::EQ:
                case TacOpCode::NEQ:
                case TacOpCode::AbsEQ:
                case TacOpCode::AbsNEQ:
                case TacOpCode::LT:
                case TacOpCode::LE:
                case TacOpCode::GT:
                case TacOpCode::GE:
                case TacOpCode::LAnd:
                case TacOpCode::LOr:
                    defineNode(inst.getOp3().value<Register>(),
                               makeBinary(inst.opcode(), operandNode(inst.getOp1().value<Register>()),
                                          operandNode(inst.getOp2().value<Register>())));
                    noteNamedDef(inst.getOp3().value<Register>());
                    break;
                case TacOpCode::BinaryImm: {
                    // 逆映射：LoadImm K imm; Op src K dst → src op imm
                    const auto &fused = static_cast<const inter::BinaryImm &>(inst);
                    const auto imm = makeLeaf(Expr::Kind::Int, fused.getOp2().value<Integer>());
                    defineNode(fused.getOp3().value<Register>(),
                               makeBinary(fused.arithmetic, operandNode(fused.getOp1().value<Register>()), imm));
                    noteNamedDef(fused.getOp3().value<Register>());
                    break;
                }
                case TacOpCode::CmpEQ:
                case TacOpCode::CmpDEQ:
                case TacOpCode::CmpLT:
                case TacOpCode::CmpGT: {
                    if(!_condPcs.contains(_instPc)) {
                        fail("_zf 比较不属于已识别的条件区（M5）");
                        break;
                    }
                    const auto op = inst.opcode() == TacOpCode::CmpEQ ? TacOpCode::EQ
                        : inst.opcode() == TacOpCode::CmpDEQ          ? TacOpCode::AbsEQ
                        : inst.opcode() == TacOpCode::CmpLT           ? TacOpCode::LT
                                                                      : TacOpCode::GT;
                    _condNodeAtPc[_instPc] = makeBinary(op, operandNode(inst.getOp1().value<Register>()),
                                                        operandNode(inst.getOp2().value<Register>()));
                    break;
                }
                case TacOpCode::CompareTest:
                    defineNode(inst.getOp3().value<Register>(),
                               makeBinary(static_cast<const inter::CompareTest &>(inst).comparison,
                                          operandNode(inst.getOp1().value<Register>()),
                                          operandNode(inst.getOp2().value<Register>())));
                    noteCondCompare(inst.getOp3().value<Register>());
                    break;
                case TacOpCode::CompareTestImm: {
                    const auto &cmp = static_cast<const inter::CompareTestImm &>(inst);
                    const auto imm = makeLeaf(Expr::Kind::Int, cmp.getOp2().value<Integer>());
                    defineNode(cmp.getOp3().value<Register>(),
                               makeBinary(cmp.comparison, operandNode(cmp.getOp1().value<Register>()), imm));
                    noteCondCompare(cmp.getOp3().value<Register>());
                    break;
                }
                case TacOpCode::GPropC:
                case TacOpCode::GMethodC:
                    defineNode(
                        inst.getOp3().value<Register>(),
                        memberNode(operandNode(inst.getOp1().value<Register>()), inst.getOp2().value<Atom>(), INVALID));
                    noteNamedDef(inst.getOp3().value<Register>());
                    break;
                case TacOpCode::GPropRawC:
                    // &o.x（Atom 直引）：property 引用，"&" 前缀形态
                    defineNode(inst.getOp3().value<Register>(),
                               makeUnary("&",
                                         memberNode(operandNode(inst.getOp1().value<Register>()),
                                                    inst.getOp2().value<Atom>(), INVALID)));
                    noteNamedDef(inst.getOp3().value<Register>());
                    break;
                case TacOpCode::GProp:
                case TacOpCode::GPropRaw: {
                    const bool raw = inst.opcode() == TacOpCode::GPropRaw;
                    const u32 nameNode = raw
                        ? operandNode(inst.getOp2().value<Register>())
                        : (inst.opcode() == TacOpCode::GProp ? operandNode(inst.getOp2().value<Register>()) : INVALID);
                    const auto atom =
                        inst.opcode() == TacOpCode::GPropRawC ? inst.getOp2().value<Atom>() : ATOM_INVALID;
                    const auto inner = memberNode(operandNode(inst.getOp1().value<Register>()), atom, nameNode);
                    defineNode(inst.getOp3().value<Register>(), raw ? makeUnary("&", inner) : inner);
                    noteNamedDef(inst.getOp3().value<Register>());
                    break;
                }
                case TacOpCode::GRefRaw:
                    defineNode(inst.getOp2().value<Register>(),
                               makeUnary("&",
                                         makeNamed(Expr::Kind::LocalName, atomText(_rt, inst.getOp1().value<Atom>()),
                                                   true, inst.getOp1().value<Atom>())));
                    break;
                case TacOpCode::DerefGet: {
                    const auto node = makeUnary("*", operandNode(inst.getOp1().value<Register>()));
                    defineNode(inst.getOp2().value<Register>(), node);
                    noteNamedDef(inst.getOp2().value<Register>());
                    break;
                }
                case TacOpCode::DPropC:
                case TacOpCode::DProp:
                case TacOpCode::DPropRawC:
                case TacOpCode::DPropRaw: {
                    const bool raw = inst.opcode() == TacOpCode::DPropRawC || inst.opcode() == TacOpCode::DPropRaw;
                    Stmt stmt{ .kind = Stmt::Kind::StoreMember,
                               .pc = _instPc,
                               .obj = slotOf(inst.getOp1().value<Register>()),
                               .src = slotOf(inst.getOp3().value<Register>()),
                               .raw = raw };
                    if(inst.opcode() == TacOpCode::DPropC || inst.opcode() == TacOpCode::DPropRawC)
                        stmt.atom = inst.getOp2().value<Atom>();
                    else
                        stmt.idx = slotOf(inst.getOp2().value<Register>());
                    pushStmt(std::move(stmt));
                    break;
                }
                case TacOpCode::SetIndex: {
                    const auto obj = inst.getOp1().value<Register>();
                    const auto idxNode = operandNode(inst.getOp2().value<Register>());
                    // 字面量构造折叠：obj 当前定义为 ArrayLit/DictLit 节点时并入
                    if(foldLiteralElement(obj, idxNode, inst.getOp3().value<Register>()))
                        break;
                    Stmt stmt{ .kind = Stmt::Kind::StoreIndex,
                               .pc = _instPc,
                               .obj = slotOf(obj),
                               .idx = slotOf(inst.getOp2().value<Register>()),
                               .src = slotOf(inst.getOp3().value<Register>()) };
                    pushStmt(std::move(stmt));
                    break;
                }
                case TacOpCode::DerefSet:
                    pushStmt(Stmt{ .kind = Stmt::Kind::StoreDeref,
                                   .pc = _instPc,
                                   .obj = slotOf(inst.getOp1().value<Register>()),
                                   .src = slotOf(inst.getOp2().value<Register>()) });
                    break;
                case TacOpCode::DelPropC:
                case TacOpCode::DelProp: {
                    const auto dst = inst.getOp3().value<Register>();
                    const auto version = definedVersion(dst);
                    const bool used = version != INVALID && !_defs[dst.index()][version].usePcs.empty();
                    if(used) {
                        const u32 nameNode = inst.opcode() == TacOpCode::DelProp
                            ? operandNode(inst.getOp2().value<Register>())
                            : INVALID;
                        const auto atom =
                            inst.opcode() == TacOpCode::DelPropC ? inst.getOp2().value<Atom>() : ATOM_INVALID;
                        const auto inner = memberNode(operandNode(inst.getOp1().value<Register>()), atom, nameNode);
                        defineNode(dst, makeUnary("delete", inner));
                    } else {
                        Stmt stmt{ .kind = Stmt::Kind::DeleteMember,
                                   .pc = _instPc,
                                   .obj = slotOf(inst.getOp1().value<Register>()) };
                        if(inst.opcode() == TacOpCode::DelPropC)
                            stmt.atom = inst.getOp2().value<Atom>();
                        else
                            stmt.idx = slotOf(inst.getOp2().value<Register>());
                        pushStmt(std::move(stmt));
                    }
                    break;
                }
                case TacOpCode::GetIndex: {
                    const auto node = newNode(Expr::Kind::Index);
                    _nodes[node].a = operandNode(inst.getOp1().value<Register>());
                    _nodes[node].b = operandNode(inst.getOp2().value<Register>());
                    defineNode(inst.getOp3().value<Register>(), node);
                    noteNamedDef(inst.getOp3().value<Register>());
                    break;
                }
                case TacOpCode::NewArray: {
                    const auto node = newNode(Expr::Kind::ArrayLit);
                    defineNode(inst.getOp1().value<Register>(), node);
                    noteNamedDef(inst.getOp1().value<Register>());
                    break;
                }
                case TacOpCode::NewDict: {
                    const auto node = newNode(Expr::Kind::DictLit);
                    defineNode(inst.getOp1().value<Register>(), node);
                    noteNamedDef(inst.getOp1().value<Register>());
                    break;
                }
                case TacOpCode::NewRegExp: {
                    const auto &constant = _chunk.getConstants()[inst.getOp2().value<ConstIdx>().index()];
                    if(constant.type() != ConstantType::Atom) {
                        fail("正则字面量模式常量缺失");
                        break;
                    }
                    const auto node = newNode(Expr::Kind::RegExp);
                    _nodes[node].text = atomText(_rt, constant.value<Atom>());
                    defineNode(inst.getOp1().value<Register>(), node);
                    noteNamedDef(inst.getOp1().value<Register>());
                    break;
                }
                case TacOpCode::CallV:
                case TacOpCode::CallMethodV: {
                    const auto &call = static_cast<const inter::CallV &>(inst);
                    const auto node = newNode(Expr::Kind::Call);
                    _nodes[node].args.push_back(operandNode(call.func));
                    for(const auto arg : call.args)
                        _nodes[node].args.push_back(operandNode(arg));
                    defineNode(call.dst, node);
                    noteNamedDef(call.dst);
                    break;
                }
                case TacOpCode::Call: {
                    // 转译路径遗留形态：N×Push + Call dst func argc
                    const auto argc = inst.getOp3().value<Integer>();
                    const auto dst = inst.getOp1().value<Register>();
                    const auto node = newNode(Expr::Kind::Call);
                    _nodes[node].args.push_back(operandNode(inst.getOp2().value<Register>()));
                    if(_pending.size() < static_cast<size_t>(argc)) {
                        fail("Push 序列与 Call 实参数不匹配");
                        break;
                    }
                    const auto dstVersion = definedVersion(dst);
                    for(auto it = _pending.end() - static_cast<ptrdiff_t>(argc); it != _pending.end(); ++it) {
                        _nodes[node].args.push_back(it->node);
                        if(dstVersion != INVALID)
                            _redirect[it->pc] = packSlot(dst, dstVersion);
                    }
                    _pending.resize(_pending.size() - static_cast<size_t>(argc));
                    defineNode(dst, node);
                    noteNamedDef(dst);
                    break;
                }
                case TacOpCode::CallOmit: {
                    const auto &omit = static_cast<const inter::CallOmit &>(inst);
                    const auto node = newNode(Expr::Kind::Transmit);
                    _nodes[node].args.push_back(operandNode(omit.func));
                    defineNode(omit.dst, node);
                    noteNamedDef(omit.dst);
                    break;
                }
                case TacOpCode::ToInt:
                case TacOpCode::ToReal:
                case TacOpCode::ToString:
                case TacOpCode::TypeOf:
                case TacOpCode::Asc:
                case TacOpCode::Chr:
                case TacOpCode::LNot:
                case TacOpCode::ChgSign:
                case TacOpCode::ChkInv:
                case TacOpCode::BNot:
                case TacOpCode::ToNumber:
                case TacOpCode::ToOctet: {
                    const auto node = makeUnary(unaryText(inst.opcode()), operandNode(inst.getOp1().value<Register>()));
                    defineNode(inst.getOp2().value<Register>(), node);
                    noteNamedDef(inst.getOp2().value<Register>());
                    break;
                }
                case TacOpCode::Inc:
                case TacOpCode::Dec: {
                    // 单指令自增：旧版本为操作数、新版本承载结果。仅"结果直接回写
                    // 同名目标"的语句形态可恢复（IRGen 不发射；来自 TJS2 转译）。
                    // 具名/合成存储经 noteNamedDef 落为 `name++;` 语句并改绑新版
                    // 本，避免后续读取内联出 `name++` 表达式（后缀值语义错误）
                    const auto reg = inst.getOp1().value<Register>();
                    const auto node = newNode(Expr::Kind::IncDec);
                    _nodes[node].text = inst.opcode() == TacOpCode::Inc ? "++" : "--";
                    _nodes[node].a = operandNode(reg);
                    defineNode(reg, node);
                    noteNamedDef(reg);
                    break;
                }
                case TacOpCode::ChkIns: {
                    const auto node = newNode(Expr::Kind::Binary);
                    _nodes[node].text = "instanceof";
                    _nodes[node].a = operandNode(inst.getOp1().value<Register>()); // 前一版本
                    _nodes[node].b = operandNode(inst.getOp2().value<Register>());
                    defineNode(inst.getOp1().value<Register>(), node);
                    noteNamedDef(inst.getOp1().value<Register>());
                    break;
                }
                case TacOpCode::Inv:
                    pushStmt(Stmt{ .kind = Stmt::Kind::Invalidate,
                                   .pc = _instPc,
                                   .src = slotOf(inst.getOp1().value<Register>()) });
                    break;
                case TacOpCode::Ret:
                    pushStmt(Stmt{
                        .kind = Stmt::Kind::Return, .pc = _instPc, .src = slotOf(inst.getOp1().value<Register>()) });
                    break;
                case TacOpCode::Throw:
                    pushStmt(Stmt{
                        .kind = Stmt::Kind::ThrowStmt, .pc = _instPc, .src = slotOf(inst.getOp1().value<Register>()) });
                    break;
                default:
                    fail(fmt::format("预检未覆盖的指令（opcode {}）", static_cast<int>(inst.opcode())));
                    break;
            }
        }

        // 具名局部寄存器的直接定义 = var 声明（IRGen 零拷贝：表达式 dst 直接成为
        // 变量槽）。转为 StoreLocal 语句，版本改绑为变量名
        void noteNamedDef(const Register reg) {
            // 仅供派发比较的标签常量是该定义版本的值，不是跨 case 存储。
            // 物理寄存器复用不应将其改绑为合成名字或留下无法发射的赋值。
            if(_switchLabelPcs.contains(_instPc) && _switchLocalNames.contains(reg.index())) {
                const auto v = currentVersion(reg);
                if(v != INVALID && std::ranges::all_of(_defs[reg.index()][v].usePcs, [&](u32 pc) {
                       return _switchCmpPcs.contains(pc);
                   }))
                    return;
                fail("参考 switch 标签常量具有派发之外的使用，尚不能局部物化（M5）");
                return;
            }
            // 声明标签在表达式发射之后取（startPC = 定义 pc + 1）：查"本指令
            // 执行后"该寄存器的名字
            const auto name = localNameAt(reg, _instPc + 1);
            if(name.empty())
                return;
            const auto version = currentVersion(reg);
            if(version == INVALID)
                return;
            auto &def = _defs[reg.index()][version];
            // var x; 的 void 初始化也走此路径（Load *0 到具名寄存器）
            Stmt stmt{ .kind = Stmt::Kind::StoreLocal, .pc = _instPc, .exprNode = def.node, .name = name };
            stmt.declare = declareLocal(reg.index(), name);
            pushStmt(std::move(stmt));
            const auto nameNode = makeNamed(Expr::Kind::LocalName, name, false);
            def.node = nameNode;
        }

        [[nodiscard]] bool declareLocal(const u16 regIndex, const std::string &name) {
            if(_stateMachine)
                return false;
            const auto synthetic = _switchLocalNames.find(regIndex);
            if(synthetic != _switchLocalNames.end() && synthetic->second.first == name)
                return false; // 合成存储在函数体入口统一声明，赋值仍保留原 pc。
            if(_declaredLocals.contains({ regIndex, name }))
                return false;
            _declaredLocals.emplace(regIndex, name);
            return true;
        }

        // reg 在 pc 之前的最新版本（声明安装链回填槽位用：折叠时 Pass B 尚未
        // 推进到安装指令，_current 不能反映安装点版本）
        [[nodiscard]] u32 versionBefore(const Register reg, const u32 pc) const {
            const auto index = reg.index();
            if(index >= _defs.size())
                return INVALID;
            u32 found = INVALID;
            for(u32 v = 0; v < _defs[index].size(); ++v)
                if(_defs[index][v].defPc < pc)
                    found = v; // 同寄存器定义按 pc 单调，取最后一个
            return found;
        }

        // ── M4 声明装配：Load 声明常量 + 紧随安装链折叠为 Decl tile ──
        // 安装链 = Load 后连续的 Global（链内定义临时）/ ChgThis / DGlobal / DThis /
        // DProp 族 / DLocal，且每条必须读链内定义的寄存器（否则是无关语句，折叠
        // 终止）。安装名取自安装指令（Atom / DProp 名寄存器 / 具名局部）；无安装名
        // 的表达式位置字面量（如实参函数）如实拒绝。模板形态与 IRGen/转译器一一对
        // 应：Global+ChgThis+DGlobal = 顶层 function 声明、Load 直落具名局部 = 嵌套
        // function 声明（零拷贝）、DGlobal 裸写 = var 赋值/类安装、ChgThis+DPropRawC
        // = property 安装（自产与 bc 同构）
        // 声明安装链预扫描：Load 声明常量 + 紧随安装链折叠。链 pc 记入 _foldedPcs
        // （版本链扫描与语句构建整体跳过），Decl tile 规格存入 _declSpecs，待
        // Pass B 处理到 Load pc 时（版本链已齐）解析槽位并产语句。
        // 模板形态严格有序：Global* → ChgThis? → 安装指令（终点）。寄存器复用
        // 使"读链值"判定不充分（安装后的无关语句可能恰读链内寄存器），故安装
        // 即终止，不向后贪婪
        struct DeclSpec {
            u32 constIdx{};
            u32 installPc{};
            u32 foldEnd{};
            u8 install{}; // Install 枚举
            std::string name;
            Register objReg{};
            Register nameReg{};
            bool hasObj{};
            bool hasNameReg{};
            bool raw{};
            bool declare{};
            bool contextInstall{};
        };

        void scanDeclarations() {
            auto &code = _chunk.getInstVec();
            for(u32 pc = 0; pc < code.size(); ++pc) {
                const Instruction &inst = *code[pc];
                if(inst.opcode() != TacOpCode::Load)
                    continue;
                const auto constIdx = inst.getOp2().value<ConstIdx>();
                const auto &constant = _chunk.getConstants()[constIdx.index()];
                if(constant.type() != ConstantType::FuncMeta && constant.type() != ConstantType::ClassMeta &&
                   constant.type() != ConstantType::PropMeta)
                    continue;
                // ctExprFunction 的变量存储是赋值，不是具名声明安装链。
                if(constant.type() == ConstantType::FuncMeta && _host &&
                   _host->isAnonymousExpression(*constant.value<FuncMeta *>()))
                    continue;
                const auto dst = inst.getOp1().value<Register>();
                enum class Install : u8 { None, Global, Member, Local } install{ Install::None };
                DeclSpec spec{};
                spec.constIdx = constIdx.index();
                bool contextInstall = false;
                u32 foldEnd = pc;
                for(u32 chain = pc + 1; chain < code.size(); ++chain) {
                    const Instruction &cand = *code[chain];
                    const auto op = cand.opcode();
                    if(op == TacOpCode::Global && install == Install::None && !contextInstall) {
                        _foldedPcs.insert(chain);
                        foldEnd = chain;
                        continue;
                    }
                    if(op == TacOpCode::ChgThis && install == Install::None) {
                        _foldedPcs.insert(chain);
                        foldEnd = chain;
                        contextInstall = true;
                        continue;
                    }
                    // 仅安装指令族可作为链终点；其余指令（调用、拷贝、控制流…）属于
                    // 后续语句——不折叠、不终止于它（此前 default 分支误把紧随语句
                    // 折进安装链，吞掉如"零拷贝局部声明后紧跟的调用"，读被吞定义的
                    // 寄存器误报未定义）
                    bool matched = false;
                    switch(op) {
                        case TacOpCode::DGlobal:
                            spec.name = atomText(_rt, cand.getOp1().value<Atom>());
                            install = Install::Global;
                            if(!_functionLike && !_declaredGlobals.contains(spec.name)) {
                                spec.declare = true;
                                _declaredGlobals.insert(spec.name);
                            }
                            matched = true;
                            break;
                        case TacOpCode::DThis:
                            spec.name = atomText(_rt, cand.getOp1().value<Atom>());
                            install = Install::Global; // 上下文成员声明：同裸声明形态
                            contextInstall = true;
                            matched = true;
                            break;
                        case TacOpCode::DPropC:
                        case TacOpCode::DPropRawC:
                            spec.name = atomText(_rt, cand.getOp2().value<Atom>());
                            install = Install::Member;
                            spec.raw = op == TacOpCode::DPropRawC;
                            spec.objReg = cand.getOp1().value<Register>();
                            spec.hasObj = true;
                            // 顶层 property 安装（Global+ChgThis+DPropRawC 模板）占据
                            // 全局名：后续同名 DGlobal 是赋值，不再补 var（property
                            // 声明注册的是属性对象本体，名字已被占据）
                            if(contextInstall && !_functionLike && !_declaredGlobals.contains(spec.name))
                                _declaredGlobals.insert(spec.name);
                            matched = true;
                            break;
                        case TacOpCode::DProp:
                        case TacOpCode::DPropRaw:
                            install = Install::Member;
                            spec.raw = op == TacOpCode::DPropRaw;
                            spec.objReg = cand.getOp1().value<Register>();
                            spec.hasObj = true;
                            spec.nameReg = cand.getOp2().value<Register>();
                            spec.hasNameReg = true;
                            matched = true;
                            break;
                        case TacOpCode::DLocal: {
                            const auto ldst = cand.getOp2().value<Register>();
                            spec.name = localNameAt(ldst, chain + 1);
                            if(!spec.name.empty()) {
                                install = Install::Local;
                                spec.declare = declareLocal(ldst.index(), spec.name);
                                matched = true;
                            }
                            // 匿名拷入（dst 无名）：值进入表达式流，不是安装
                            break;
                        }
                        default:
                            break;
                    }
                    if(!matched)
                        break; // 非安装指令：安装链不存在，留待语句/表达式处理
                    _foldedPcs.insert(chain);
                    foldEnd = chain;
                    spec.installPc = chain;
                    break; // 安装即终点
                }
                if(install == Install::None) {
                    // Load 直落具名局部（嵌套 function 声明的零拷贝形态）；无名 =
                    // 表达式位置字面量（实参/赋值右值），不登记 spec，由 Pass B 的
                    // Load case 生成 FuncLit 节点或按常量种类拒绝
                    spec.name = localNameAt(dst, pc + 1);
                    if(spec.name.empty())
                        continue;
                }
                spec.foldEnd = foldEnd;
                spec.contextInstall = contextInstall;
                spec.install = static_cast<u8>(install);
                _declSpecs.emplace(pc, std::move(spec));
            }
        }

        // Pass B 到达 Load pc：按预扫描规格解析槽位（versionBefore 需完整版本链）
        void emitDeclaration(const DeclSpec &spec) {
            Stmt stmt{ .kind = Stmt::Kind::Decl, .pc = _instPc, .tacEnd = spec.foldEnd + 1, .constIdx = spec.constIdx };
            if(spec.hasObj)
                stmt.obj = packSlot(spec.objReg, versionBefore(spec.objReg, spec.installPc));
            if(spec.hasNameReg) {
                const auto nameVersion = versionBefore(spec.nameReg, spec.installPc);
                if(nameVersion != INVALID)
                    stmt.idx = packSlot(spec.nameReg, nameVersion);
            }
            stmt.name = spec.name;
            stmt.raw = spec.raw;
            stmt.declare = spec.declare;
            stmt.contextInstall = spec.contextInstall;
            stmt.declInstall = spec.install;
            // 零拷贝局部声明（无安装指令，Load 直落变量存储寄存器）：后续读按
            // 局部名渲染（调用/传参读的就是该寄存器）。install == 0 = 无安装指令
            if(spec.install == 0 && !stmt.name.empty())
                defineNode(_chunk.getInstVec()[_instPc]->getOp1().value<Register>(),
                           makeNamed(Expr::Kind::LocalName, stmt.name, false));
            pushStmt(std::move(stmt));
        }

        // ── M4 复合赋值逆融合（方案 §5.6）：模板命中 + 单读者双守卫 ──
        // GProp/DerefGet 读 → 二目运算 → DProp/SetIndex/DerefSet 写，且中间值各
        // 只有一个读者时合并为 TARGET OP= RHS / TARGET++。单读者守卫防止误融合
        // 改变 getter 调用次数（读值另有读者时融合会复制/丢失读取）；对象一致性
        // 按"同一源表达式的两次求值"判定（bc 模板同槽，自产路径比较定义指令
        // 签名或名字节点），防止 o.x = p.x + 1 类误融为 o.x += 1。Sub(1, X)、
        // 成员非直接回写等非模板形态不融合。
        void fuseCompoundAssign() {
            for(auto &stmt : _stmts) {
                if(stmt.kind != Stmt::Kind::StoreMember && stmt.kind != Stmt::Kind::StoreIndex &&
                   stmt.kind != Stmt::Kind::StoreDeref && stmt.kind != Stmt::Kind::StoreLocal &&
                   stmt.kind != Stmt::Kind::StoreGlobal)
                    continue;
                if(stmt.src == INVALID)
                    continue;
                const auto &srcDef = versionOf(stmt.src);
                if(srcDef.usePcs.size() != 1 || srcDef.usePcs.front() != stmt.pc)
                    continue; // 结果另有读者：融合会改变求值次数
                if(srcDef.defPc == INVALID || srcDef.defPc >= _chunk.getInstVec().size())
                    continue;
                if(srcDef.node == INVALID || _nodes[srcDef.node].kind != Expr::Kind::Binary)
                    continue;
                const auto &bin = _nodes[srcDef.node];
                // ++ 形态：Add(字面量 1, X)（自产后缀模板）；bc INCPD 展开为成员在
                // 左的 Add(t, 1)，落入 += 分支按 o.x += 1 恢复（行为等价）
                const bool incForm = bin.a != INVALID && bin.a < _nodes.size() &&
                    _nodes[bin.a].kind == Expr::Kind::Int && _nodes[bin.a].integer == 1 && bin.text == "+";
                std::string opText;
                u32 memberNodeIdx = INVALID;
                u32 rhsNodeIdx = INVALID;
                if(incForm) {
                    opText = "++";
                    memberNodeIdx = bin.b;
                } else if(isCompoundOpText(bin.text)) {
                    opText = bin.text + "=";
                    memberNodeIdx = bin.a;
                    rhsNodeIdx = bin.b;
                } else {
                    continue;
                }
                // 成员形态归一：&o.x / *p 的裸访问解一层
                u32 inner = memberNodeIdx;
                u32 derefOperand = INVALID;
                bool rawAccess = false;
                if(inner != INVALID && inner < _nodes.size() && _nodes[inner].kind == Expr::Kind::Unary) {
                    const auto &un = _nodes[inner];
                    if(un.text == "&") {
                        rawAccess = true;
                        derefOperand = un.a;
                        inner = un.a;
                    } else if(un.text == "*") {
                        derefOperand = un.a;
                        inner = INVALID; // 解引用形态无内层成员
                    } else {
                        continue;
                    }
                }
                Stmt fused{ .kind = Stmt::Kind::CompoundAssign, .pc = stmt.pc, .opText = std::move(opText) };
                bool matched = false;
                if(inner != INVALID && inner < _nodes.size() &&
                   (_nodes[inner].kind == Expr::Kind::Member || _nodes[inner].kind == Expr::Kind::Index)) {
                    const auto &member = _nodes[inner];
                    const bool isIndexKind = member.kind == Expr::Kind::Index;
                    if(stmt.kind == Stmt::Kind::StoreMember) {
                        if(stmt.atom != ATOM_INVALID) {
                            matched =
                                !isIndexKind && member.atom == stmt.atom && sameObjectIdentity(member.a, stmt.obj);
                        } else {
                            matched = !isIndexKind && member.atom == ATOM_INVALID &&
                                sameObjectIdentity(member.a, stmt.obj) && sameObjectIdentity(member.b, stmt.idx);
                        }
                        if(matched && rawAccess != stmt.raw)
                            matched = false;
                    } else if(stmt.kind == Stmt::Kind::StoreIndex) {
                        matched = isIndexKind && sameObjectIdentity(member.a, stmt.obj) &&
                            sameObjectIdentity(member.b, stmt.idx);
                    }
                    if(matched) {
                        fused.obj = stmt.obj;
                        fused.idx = stmt.idx;
                        fused.atom = stmt.atom;
                        fused.raw = stmt.raw;
                    }
                } else if(derefOperand != INVALID && stmt.kind == Stmt::Kind::StoreDeref) {
                    // *p 形态：读侧解引用操作数与写侧引用一致
                    matched = sameObjectIdentity(derefOperand, stmt.obj);
                    if(matched) {
                        fused.obj = stmt.obj;
                        fused.raw = stmt.raw;
                    }
                } else if(stmt.kind == Stmt::Kind::StoreLocal) {
                    // 具名局部：x++（Add(1, x)）/ x OP= rhs（成员在左）；名字节点
                    // 直读（非成员/解引用形态）
                    const u32 nameNode = incForm ? bin.b : bin.a;
                    if(memberNodeIdx == nameNode && nameNode != INVALID && nameNode < _nodes.size() &&
                       _nodes[nameNode].kind == Expr::Kind::LocalName && !_nodes[nameNode].globalName &&
                       _nodes[nameNode].text == stmt.name) {
                        matched = true;
                    }
                } else if(stmt.kind == Stmt::Kind::StoreGlobal) {
                    // 全局：g OP= rhs（GGlobal 名节点在左）/ g++（Add(1, g)，bc
                    // proxy 模板成员在左）；读侧为 globalName 节点，写侧按 Atom 名
                    const u32 nameNode = incForm ? bin.b : bin.a;
                    if(memberNodeIdx == nameNode && nameNode != INVALID && nameNode < _nodes.size() &&
                       _nodes[nameNode].kind == Expr::Kind::LocalName && _nodes[nameNode].globalName &&
                       _nodes[nameNode].text == atomText(_rt, stmt.atom) && isPlainMemberName(_nodes[nameNode].text)) {
                        matched = true;
                    }
                }
                if(!matched)
                    continue;
                // 成员读取版本：唯一读者须是本二目指令（中间值单读者守卫）。
                // 具名局部按名字直读（无 getter），无需该守卫
                if(stmt.kind != Stmt::Kind::StoreLocal) {
                    const u32 memberSlot = slotByNodeAndUse(memberNodeIdx, srcDef.defPc);
                    if(memberSlot == INVALID)
                        continue;
                    _defs[memberSlot >> 16][memberSlot & 0xFFFF].fused = true;
                }
                _defs[stmt.src >> 16][stmt.src & 0xFFFF].fused = true;
                fused.rhsNode = rhsNodeIdx;
                if(stmt.kind == Stmt::Kind::StoreLocal)
                    fused.name = stmt.name;
                else if(stmt.kind == Stmt::Kind::StoreGlobal)
                    fused.name = _nodes[incForm ? bin.b : bin.a].text;
                stmt = std::move(fused);
            }
        }

        // 可逆融合为 OP= 的二目运算符（与 Ciallang AssignToken 集一致）
        [[nodiscard]] static bool isCompoundOpText(const std::string &text) {
            static const std::set<std::string> ops = { "+", "-", "*",  "/",  "\\",  "%",  "|",
                                                       "^", "&", "<<", ">>", ">>>", "&&", "||" };
            return ops.contains(text);
        }

        // 匿名拷贝透明解析：Copy 节点沿 srcVersion 追到源头节点（拷贝不改变值，
        // 两侧各自拷贝同一源版本时是同一值）
        [[nodiscard]] u32 resolveCopyNode(u32 nodeIdx) const {
            for(auto guard = 0; guard < 16; ++guard) {
                if(nodeIdx == INVALID || nodeIdx >= _nodes.size())
                    break;
                const auto &node = _nodes[nodeIdx];
                if(node.kind != Expr::Kind::Copy || node.srcVersion == INVALID)
                    break;
                const auto &srcDef = versionOf(node.srcVersion);
                if(srcDef.node == nodeIdx)
                    break; // 防自环
                nodeIdx = srcDef.node;
            }
            return nodeIdx;
        }

        // 读侧对象节点与写侧对象槽位的一致性：同节点/同名字读即同值；匿名拷贝
        // 按源头解析（如 *p 模板读写两侧各自 g_local 拷贝同一引用）；否则比较
        // 定义指令签名（同一源表达式的两次求值）。调用族定义保守拒绝（CallV
        // 实参不在操作数中，签名无法区分不同调用）
        [[nodiscard]] bool sameObjectIdentity(const u32 readNode, const u32 writeSlot) {
            if(readNode == INVALID || writeSlot == INVALID || readNode >= _nodes.size())
                return false;
            const auto &wd = versionOf(writeSlot);
            if(wd.node == readNode)
                return true;
            if(wd.node == INVALID || wd.node >= _nodes.size() || wd.defPc >= _chunk.getInstVec().size())
                return false;
            const u32 rnIdx = resolveCopyNode(readNode);
            const u32 wnIdx = resolveCopyNode(wd.node);
            if(rnIdx != INVALID && rnIdx == wnIdx)
                return true;
            if(rnIdx == INVALID || wnIdx == INVALID || rnIdx >= _nodes.size() || wnIdx >= _nodes.size())
                return false;
            const auto &rn = _nodes[rnIdx];
            const auto &wn = _nodes[wnIdx];
            if(rn.kind == Expr::Kind::LocalName && wn.kind == Expr::Kind::LocalName)
                return rn.text == wn.text && rn.globalName == wn.globalName;
            auto &code = _chunk.getInstVec();
            for(size_t reg = 0; reg < _defs.size(); ++reg) {
                for(size_t v = 0; v < _defs[reg].size(); ++v) {
                    const auto &rd = _defs[reg][v];
                    if(rd.node != rnIdx || rd.defPc >= code.size())
                        continue;
                    const auto &ri = code[rd.defPc];
                    const auto &wi = code[wd.defPc];
                    if(ri->opcode() != wi->opcode())
                        return false;
                    switch(ri->opcode()) {
                        case TacOpCode::CallV:
                        case TacOpCode::CallMethodV:
                        case TacOpCode::Call:
                        case TacOpCode::CallOmit:
                        case TacOpCode::EvalStr:
                            return false;
                        default:
                            break;
                    }
                    if(ri->opcode() == TacOpCode::BinaryImm &&
                       static_cast<const inter::BinaryImm &>(*ri).arithmetic !=
                           static_cast<const inter::BinaryImm &>(*wi).arithmetic)
                        return false;
                    return operandEquals(ri->getOp1(), wi->getOp1()) && operandEquals(ri->getOp2(), wi->getOp2()) &&
                        operandEquals(ri->getOp3(), wi->getOp3());
                }
            }
            return false;
        }

        // 版本链中节点恰为 nodeIdx 且唯一读者为 usePc 的定义槽位
        [[nodiscard]] u32 slotByNodeAndUse(const u32 nodeIdx, const u32 usePc) const {
            if(nodeIdx == INVALID)
                return INVALID;
            for(size_t reg = 0; reg < _defs.size(); ++reg)
                for(size_t v = 0; v < _defs[reg].size(); ++v) {
                    const auto &def = _defs[reg][v];
                    if(def.node == nodeIdx && def.usePcs.size() == 1 && def.usePcs.front() == usePc)
                        return packSlot(Register{ static_cast<u16>(reg) }, static_cast<u32>(v));
                }
            return INVALID;
        }

        static bool operandEquals(const inter::Operand &a, const inter::Operand &b) {
            if(a.type() != b.type())
                return false;
            switch(a.type()) {
                case inter::Operand::Type::Register:
                    return a.value<Register>() == b.value<Register>();
                case inter::Operand::Type::Label:
                    return a.value<Label>().address() == b.value<Label>().address();
                case inter::Operand::Type::ConstIndex:
                    return a.value<ConstIdx>().index() == b.value<ConstIdx>().index();
                case inter::Operand::Type::Number:
                    return a.value<Integer>() == b.value<Integer>();
                case inter::Operand::Type::Atom:
                    return a.value<Atom>() == b.value<Atom>();
                default:
                    return true;
            }
        }

        // 字面量构造折叠：SetIndex 到 ArrayLit/DictLit 节点时并入元素。
        // 返回 true 表示该 pc 已消费（透明化：不产语句、不影响干净区间）
        [[nodiscard]] bool foldLiteralElement(const Register obj, const u32 idxNode, const Register src) {
            const auto version = currentVersion(obj);
            if(version == INVALID)
                return false;
            const auto node = _defs[obj.index()][version].node;
            if(node == INVALID)
                return false;
            const auto kind = _nodes[node].kind;
            if(kind != Expr::Kind::ArrayLit && kind != Expr::Kind::DictLit)
                return false;
            _impureOrStore[_instPc] = false;
            if(kind == Expr::Kind::ArrayLit) {
                // 元素下标必须来自顺序 LoadImm（IRGen 逐元素发射）
                if(idxNode == INVALID || _nodes[idxNode].kind != Expr::Kind::Int ||
                   _nodes[idxNode].integer != static_cast<Integer>(_nodes[node].items.size())) {
                    fail("数组字面量下标序列不连续");
                    return true;
                }
                _nodes[node].items.push_back(operandNode(src));
            } else {
                // 字典：键为常量字符串或任意表达式节点（%[k => v] 动态键按序求值）
                if(idxNode == INVALID) {
                    fail("字典字面量键节点缺失");
                    return true;
                }
                _nodes[node].items.push_back(idxNode);
                _nodes[node].items.push_back(operandNode(src));
            }
            // 该 pc 从 obj 版本的使用点中剔除（并入构造，非读取）
            auto &usePcs = _defs[obj.index()][version].usePcs;
            std::erase(usePcs, _instPc);
            return true;
        }

        [[nodiscard]] u32 memberNode(const u32 objNode, const Atom atom, const u32 nameNode) {
            const auto node = newNode(Expr::Kind::Member);
            _nodes[node].a = objNode;
            if(atom != ATOM_INVALID) {
                _nodes[node].atom = atom;
                _nodes[node].text = atomText(_rt, atom);
                _nodes[node].dotForm = isPlainMemberName(_nodes[node].text);
            } else if(nameNode != INVALID && _nodes[nameNode].kind == Expr::Kind::Str) {
                // 编译期字符串常量成员名折叠为 .name / ["name"]
                _nodes[node].text = unescapeForName(_nodes[nameNode].text);
                _nodes[node].dotForm = isPlainMemberName(_nodes[node].text);
            } else {
                _nodes[node].b = nameNode; // 动态名 → obj[expr]
            }
            return node;
        }

        // Str 节点文本（含引号）→ 裸文本（成员名场景，引号由渲染层补）
        [[nodiscard]] std::string unescapeForName(const std::string &escaped) const {
            if(escaped.size() >= 2 && escaped.front() == '"' && escaped.back() == '"')
                return escaped.substr(1, escaped.size() - 2);
            return escaped;
        }

        [[nodiscard]] u32 loadConstNode(const ConstIdx idx) {
            const auto &constant = _chunk.getConstants()[idx.index()];
            switch(constant.type()) {
                case ConstantType::None:
                    return makeSimple(Expr::Kind::Void);
                case ConstantType::Real: {
                    const auto node = newNode(Expr::Kind::Real);
                    _nodes[node].real = constant.value<Real>();
                    return node;
                }
                case ConstantType::Atom: {
                    const auto node = newNode(Expr::Kind::Str);
                    _nodes[node].atom = constant.value<Atom>();
                    _nodes[node].text = escapeString(atomText(_rt, _nodes[node].atom));
                    return node;
                }
                case ConstantType::Object: {
                    if(constant.value<Object *>() == nullptr)
                        return makeSimple(Expr::Kind::Null);
                    return containerNode(constant.value<Object *>());
                }
                case ConstantType::Octet: {
                    const auto *payload = constant.value<SharedPayload<Octet> *>();
                    if(payload == nullptr || payload->data == nullptr || payload->data->getSize() == 0) {
                        fail("octet 常量不可读");
                        return INVALID;
                    }
                    // <% hex hex %> 字面量（语法文档：Octet 十六进制字节流）
                    const auto node = newNode(Expr::Kind::OctetLit);
                    std::string out = "<% ";
                    char buf[4];
                    const auto *data = payload->data->getData();
                    const auto size = payload->data->getSize();
                    for(u32 i = 0; i < size; ++i) {
                        std::snprintf(buf, sizeof(buf), "%02x ", data[i]);
                        out += buf;
                    }
                    out += "%>";
                    _nodes[node].text = std::move(out);
                    return node;
                }
                case ConstantType::FuncMeta:
                case ConstantType::ClassMeta:
                case ConstantType::PropMeta:
                    fail("声明常量缺少安装链（匿名声明字面量属后续批次）");
                    return INVALID;
                default:
                    fail("未知常量类型");
                    return INVALID;
            }
        }

        // 编译期折叠容器：Object 常量重建字面量（嵌套递归；字典为无序映射，
        // 按键名排序输出保证确定性，成员集合与值保真）
        [[nodiscard]] u32 containerNode(const Object *object) {
            if(const auto *array = dynamic_cast<const Array *>(object)) {
                const auto node = newNode(Expr::Kind::ArrayLit);
                for(const auto &element : array->elements)
                    _nodes[node].items.push_back(valueNode(element));
                return node;
            }
            if(const auto *dict = dynamic_cast<const Dictionary *>(object)) {
                const auto node = newNode(Expr::Kind::DictLit);
                Vec<std::pair<std::string, u32>> pairs;
                for(const auto &[key, value] : dict->members)
                    pairs.emplace_back(atomText(_rt, key), valueNode(value));
                std::ranges::sort(pairs, {}, &std::pair<std::string, u32>::first);
                for(auto &[key, valueNodeIdx] : pairs) {
                    const auto keyNode = newNode(Expr::Kind::Str);
                    _nodes[keyNode].text = escapeString(key);
                    _nodes[node].items.push_back(keyNode);
                    _nodes[node].items.push_back(valueNodeIdx);
                }
                return node;
            }
            fail("非常量容器类型无可打印字面量");
            return INVALID;
        }

        // 静态 Value → 字面量节点（折叠容器元素；字符串借用仅在本表达式内使用）
        [[nodiscard]] u32 valueNode(const Value &value) {
            switch(value.type()) {
                case ValueType::Void:
                    return makeSimple(Expr::Kind::Void);
                case ValueType::Integer: {
                    const auto node = newNode(Expr::Kind::Int);
                    _nodes[node].integer = value.asIntegerUnsafe();
                    return node;
                }
                case ValueType::Real: {
                    const auto real = value.asReal();
                    if(real.isFailed()) {
                        fail("实数常量不可读");
                        return INVALID;
                    }
                    const auto node = newNode(Expr::Kind::Real);
                    _nodes[node].real = real.value();
                    return node;
                }
                case ValueType::String: {
                    const auto text = value.asString();
                    if(text.isFailed()) {
                        fail("字符串常量不可读");
                        return INVALID;
                    }
                    const auto node = newNode(Expr::Kind::Str);
                    _nodes[node].text = escapeString(std::string{ text.value()->toStdStr() });
                    return node;
                }
                case ValueType::Object:
                    if(value.isNull())
                        return makeSimple(Expr::Kind::Null);
                    return containerNode(value.requireObject());
                default:
                    fail("容器常量含不可打印值");
                    return INVALID;
            }
        }

        void pushStmt(Stmt stmt) {
            _stmtPcs.insert(stmt.pc);
            _stmts.push_back(std::move(stmt));
        }

        [[nodiscard]] u32 newNode(const Expr::Kind kind) {
            _nodes.push_back(Expr{ .kind = kind });
            return static_cast<u32>(_nodes.size() - 1);
        }

        [[nodiscard]] u32 makeSimple(const Expr::Kind kind) { return newNode(kind); }

        [[nodiscard]] u32 makeLeaf(const Expr::Kind kind, const Integer integer) {
            const auto node = newNode(kind);
            _nodes[node].integer = integer;
            return node;
        }

        [[nodiscard]] u32 makeNamed(const Expr::Kind kind, std::string text, const bool globalName,
                                    const Atom atom = ATOM_INVALID) {
            const auto node = newNode(kind);
            _nodes[node].text = std::move(text);
            _nodes[node].globalName = globalName;
            _nodes[node].atom = atom;
            return node;
        }

        [[nodiscard]] u32 makeBinary(const TacOpCode op, const u32 a, const u32 b) {
            // IRGen 降糖逆映射：`~x` 发射为 `x ^ -1`（BNot 不由 IRGen 发射）
            if(op == TacOpCode::BXor && b != INVALID && _nodes[b].kind == Expr::Kind::Int && _nodes[b].integer == -1)
                return makeUnary("~", a);
            const auto node = newNode(Expr::Kind::Binary);
            _nodes[node].text = binaryText(op);
            _nodes[node].a = a;
            _nodes[node].b = b;
            return node;
        }

        [[nodiscard]] u32 makeUnary(std::string text, const u32 a) {
            const auto node = newNode(Expr::Kind::Unary);
            _nodes[node].text = std::move(text);
            _nodes[node].a = a;
            return node;
        }

        void defineNode(const Register reg, const u32 node) {
            const auto version = definedVersion(reg);
            if(version == INVALID) {
                fail(fmt::format("pc {} 定义未登记寄存器 %{}", _instPc, reg.index()));
                return;
            }
            _defs[reg.index()][version].node = node;
            _current[reg.index()] = version; // 后续同指令内的 noteNamedDef 等读新版本
        }

        // pc 处的局部名：取 reg 匹配、startPC ≤ pc < endPC 的最新声明。endPC 由
        // endScope 填写（随压缩重映射）；0 = 未出作用域（参数/函数级变量）。寄存器
        // 跨作用域复用时，旧名不得外溢到复用者（如循环变量寄存器被三元临时复用）
        [[nodiscard]] std::string localNameAt(const Register reg, const u32 pc) const {
            if(_stateMachine && reg.index() < _machineNames.size())
                return _machineNames[reg.index()];
            if(!_meta)
                return {};
            std::string best;
            u32 bestStart = 0;
            bool found = false;
            for(const auto &local : _meta->localVars) {
                if(local.reg != reg || local.startPC.address() > pc)
                    continue;
                const auto end = local.endPC.address();
                // 查名 pc 为"本指令执行后"语义（pc+1）：与 endPC 相等仍在作用域内
                if(end != 0 && pc > end)
                    continue; // 作用域已结束
                if(!found || local.startPC.address() >= bestStart) {
                    bestStart = static_cast<u32>(local.startPC.address());
                    best = atomText(_rt, local.identifier);
                    found = true;
                }
            }
            if(best.empty()) {
                const auto recovered = _switchLocalNames.find(reg.index());
                if(recovered != _switchLocalNames.end() && pc >= recovered->second.second)
                    return recovered->second.first;
                const auto promoted = _promotedStorage.find(reg.index());
                if(promoted != _promotedStorage.end() && pc >= promoted->second.second)
                    return promoted->second.first;
            }
            return best;
        }

        static const char *unaryText(const TacOpCode op) {
            switch(op) {
                case TacOpCode::ChgSign:
                    return "-";
                case TacOpCode::LNot:
                    return "!";
                case TacOpCode::BNot:
                    return "~";
                case TacOpCode::ToNumber:
                    return "+";
                case TacOpCode::TypeOf:
                    return "typeof";
                case TacOpCode::ToInt:
                    return "int";
                case TacOpCode::ToReal:
                    return "real";
                case TacOpCode::ToString:
                    return "string";
                case TacOpCode::ToOctet:
                    return "octet";
                case TacOpCode::Asc:
                    return "#";
                case TacOpCode::Chr:
                    return "$";
                case TacOpCode::ChkInv:
                    return "isvalid";
                default:
                    return "?";
            }
        }

        // ── 死代码：无使用的纯定义丢弃，无使用且有副作用的定义降为表达式语句 ──
        void collectDeadStatements() {
            for(size_t reg = 0; reg < _defs.size(); ++reg) {
                for(auto &def : _defs[reg]) {
                    if(def.hidden || !def.usePcs.empty() || def.node == INVALID)
                        continue;
                    if(_condPcs.contains(def.defPc))
                        continue; // 条件区定义：节点为块条件，不降级为语句
                    if(_stmtPcs.contains(def.defPc))
                        continue; // 已是语句（StoreLocal 直定等）
                    if(_foldedPcs.contains(def.defPc))
                        continue; // 声明安装链/三元折叠：随 tile 消费
                    if(!hasEffect(def.node))
                        continue;
                    Stmt stmt{ .kind = Stmt::Kind::ExprStmt, .pc = def.defPc, .exprNode = def.node };
                    pushStmt(std::move(stmt));
                }
            }
        }

        // 节点或其子树是否有可观察副作用（调用/getter/存储型转换/自增）
        [[nodiscard]] bool hasEffect(const u32 node) const {
            if(node == INVALID)
                return false;
            const auto &expr = _nodes[node];
            switch(expr.kind) {
                case Expr::Kind::Call:
                case Expr::Kind::Transmit:
                case Expr::Kind::Member: // getter 语义
                case Expr::Kind::Index: // 任意对象下标读取保守视作可观察
                case Expr::Kind::IncDec:
                    return true;
                case Expr::Kind::Unary:
                    if(expr.text == "delete" || expr.text == "*" || expr.text == "int" || expr.text == "real" ||
                       expr.text == "string" || expr.text == "octet" || expr.text == "+" || expr.text == "#" ||
                       expr.text == "$")
                        return true;
                    return hasEffect(expr.a);
                case Expr::Kind::Copy:
                    return expr.srcVersion != INVALID && hasEffect(versionOf(expr.srcVersion).node);
                case Expr::Kind::Binary:
                    return hasEffect(expr.a) || hasEffect(expr.b);
                case Expr::Kind::ArrayLit:
                case Expr::Kind::DictLit:
                    break;
                default:
                    return false;
            }
            for(const auto item : expr.items)
                if(hasEffect(item))
                    return true;
            return false;
        }

        // ── 物化决策（按 defPc 降序：消费方先决，落点沿消费链传播）──
        void decideVersions() {
            // pc → 该 pc 上的非隐藏定义（每 pc 至多一条定义指令）
            std::unordered_map<u32, u32> defSlotAtPc;
            for(size_t reg = 0; reg < _defs.size(); ++reg)
                for(size_t v = 0; v < _defs[reg].size(); ++v)
                    if(!_defs[reg][v].hidden)
                        defSlotAtPc[_defs[reg][v].defPc] =
                            packSlot(Register{ static_cast<u16>(reg) }, static_cast<u32>(v));

            Vec<u32> slots;
            for(auto &[pc, slot] : defSlotAtPc)
                slots.push_back(slot);
            std::sort(slots.begin(), slots.end(),
                      [&](const u32 a, const u32 b) { return versionOf(a).defPc > versionOf(b).defPc; });

            for(const auto slot : slots) {
                auto &def = _defs[slot >> 16][slot & 0xFFFF];
                if(def.usePcs.empty())
                    continue;
                // switch 判别值/匹配值：全部使用点都是派发比较——判别值只求值
                // 一次（TAC 单定义），重编译由 IRGen 再生单次求值；绕过物化/复制
                // 决策，直接按节点渲染
                if(std::ranges::all_of(def.usePcs, [&](const u32 usePc) { return _switchCmpPcs.contains(usePc); }))
                    continue;
                if(def.fused) {
                    // 逆融合消费的定义：不物化/复制（融合语句按目标文本直读），
                    // 但保留 landing 供跨区干净判定引用
                    if(def.usePcs.size() == 1)
                        def.landing = landingFor(def.usePcs.front(), slot, defSlotAtPc);
                    continue;
                }
                // 具名局部槽本身就是存储（参数/var 变量）：读它就是读变量名，
                // 任何重赋值都会产生新版本，无需物化也不受干净区间约束
                if(def.node != INVALID && _nodes[def.node].kind == Expr::Kind::LocalName &&
                   !_nodes[def.node].globalName)
                    continue;
                if(def.usePcs.size() == 1) {
                    def.landing = landingFor(def.usePcs.front(), slot, defSlotAtPc);
                    if(!spanClean(def, defSlotAtPc)) {
                        if(_functionLike)
                            def.materialized = true;
                        else
                            fail(fmt::format("pc {} 的临时值跨脏区间且顶层无局部作用域可物化", def.defPc));
                    }
                    continue;
                }
                // 多使用
                if(_functionLike) {
                    def.materialized = true;
                    continue;
                }
                if(canDuplicate(def, defSlotAtPc))
                    def.duplicated = true;
                else
                    fail(fmt::format("pc {} 的临时值多使用且顶层复制会改变可观察行为", def.defPc));
            }
        }

        // 使用点 pc 的消费方落点：语句直接落；折叠构造（字面量/Push+Call）沿
        // redirect 找到构造结果的定义；否则是同 pc 的另一条定义（链式内联）
        [[nodiscard]] u32 landingFor(const u32 usePc, const u32 selfSlot,
                                     const std::unordered_map<u32, u32> &defSlotAtPc) const {
            if(_stmtPcs.contains(usePc))
                return usePc;
            if(const auto it = _redirect.find(usePc); it != _redirect.end() && it->second != selfSlot) {
                const auto &consumer = versionOf(it->second);
                return consumer.materialized || consumer.usePcs.size() != 1
                    ? consumer.defPc
                    : landingFor(consumer.usePcs.front(), it->second, defSlotAtPc);
            }
            if(const auto it = defSlotAtPc.find(usePc); it != defSlotAtPc.end() && it->second != selfSlot) {
                const auto &consumer = versionOf(it->second);
                if(consumer.materialized || consumer.usePcs.size() != 1)
                    return consumer.defPc;
                return landingFor(consumer.usePcs.front(), it->second, defSlotAtPc);
            }
            return usePc; // ChkIns/Inc 自引用等：落点即本 pc
        }

        // 恒可内联/复制：不读可变状态的字面量与上下文对象
        [[nodiscard]] bool alwaysInlinable(const u32 node) const {
            if(node == INVALID)
                return false;
            switch(_nodes[node].kind) {
                case Expr::Kind::Void:
                case Expr::Kind::Null:
                case Expr::Kind::Int:
                case Expr::Kind::Real:
                case Expr::Kind::Str:
                case Expr::Kind::ThisVal:
                case Expr::Kind::GlobalObj:
                    return true;
                default:
                    return false;
            }
        }

        // def → landing 开区间干净：无存储/调用/getter，除非中间定义同落点内联
        // （同一表达式树，求值序不变）或本身恒可内联。提升存储的重定义任何内联
        // 都不可跨越（Copy.srcVersion 渲染期实时解析，会读到重定义前/后的值）
        [[nodiscard]] bool spanClean(const DefVersion &def, const std::unordered_map<u32, u32> &defSlotAtPc) const {
            if(alwaysInlinable(def.node))
                return true;
            if(def.landing == INVALID || def.landing <= def.defPc)
                return true; // 自引用/同 pc
            for(u32 pc = def.defPc + 1; pc < def.landing; ++pc) {
                const auto it = defSlotAtPc.find(pc);
                if(it != defSlotAtPc.end() && _promotedStorage.contains(static_cast<u16>(it->second >> 16)))
                    return false; // 跨越提升存储重定义
                if(!_impureOrStore[pc])
                    continue;
                if(it != defSlotAtPc.end()) {
                    const auto &mid = _defs[it->second >> 16][it->second & 0xFFFF];
                    if(mid.landing == def.landing && !mid.materialized)
                        continue; // 同树内联，求值序不变
                }
                return false;
            }
            return true;
        }

        // 顶层多使用复制：恒可内联字面量，或（名字读）全部使用区间干净
        [[nodiscard]] bool canDuplicate(const DefVersion &def, const std::unordered_map<u32, u32> &defSlotAtPc) const {
            if(alwaysInlinable(def.node))
                return true;
            if(def.node == INVALID || _nodes[def.node].kind != Expr::Kind::LocalName)
                return false;
            return std::ranges::all_of(def.usePcs, [&](const u32 usePc) {
                auto copy = def;
                copy.landing = usePc;
                return spanClean(copy, defSlotAtPc);
            });
        }

        void assignTempNames() {
            // 候选名避开单元内的具名局部与全局/成员名
            for(const auto &local : _meta ? _meta->localVars : Vec<inter::LocalVariable>{})
                if(local.identifier != ATOM_INVALID)
                    _usedNames.insert(atomText(_rt, local.identifier));
            for(const auto &stmt : _stmts) {
                if(!stmt.name.empty())
                    _usedNames.insert(stmt.name);
                if(stmt.kind == Stmt::Kind::StoreGlobal || stmt.kind == Stmt::Kind::StoreMember) {
                    const auto text = atomText(_rt, stmt.atom);
                    if(!text.empty())
                        _usedNames.insert(text);
                }
            }
            Vec<std::pair<u32, u32>> order;
            for(size_t reg = 0; reg < _defs.size(); ++reg)
                for(size_t v = 0; v < _defs[reg].size(); ++v)
                    if(_defs[reg][v].materialized)
                        order.emplace_back(_defs[reg][v].defPc,
                                           packSlot(Register{ static_cast<u16>(reg) }, static_cast<u32>(v)));
            std::ranges::sort(order);
            for(const auto &[pc, slot] : order)
                _defs[slot >> 16][slot & 0xFFFF].tempName = nextTempName();
        }

        [[nodiscard]] std::string nextTempName() {
            std::string name;
            do {
                name = fmt::format("v{}", _tempCounter++);
            } while(_usedNames.contains(name));
            _usedNames.insert(name);
            return name;
        }

        // ── 渲染 ──
        [[nodiscard]] std::string renderVersion(const u32 slot) {
            if(slot == INVALID)
                return "<invalid>";
            const auto &def = versionOf(slot);
            if(def.materialized)
                return def.tempName;
            if(def.hidden || def.node == INVALID)
                return "<invalid>";
            return renderNode(def.node).text;
        }

        [[nodiscard]] Rendered renderNode(const u32 node) {
            if(node == INVALID)
                return { "<invalid>", Prec::Primary };
            const auto &expr = _nodes[node];
            switch(expr.kind) {
                case Expr::Kind::Void:
                    return { "void", Prec::Primary };
                case Expr::Kind::Null:
                    return { "null", Prec::Primary };
                case Expr::Kind::Int: {
                    auto text = fmt::format("{}", expr.integer);
                    return { std::move(text), expr.integer < 0 ? Prec::Unary : Prec::Primary };
                }
                case Expr::Kind::Real: {
                    std::string text;
                    if(!renderReal(expr.real, text)) {
                        fail("实数常量不可打印（inf/nan 无字面量拼写）");
                        return { "<real>", Prec::Primary };
                    }
                    return { std::move(text), expr.real.value() < 0 ? Prec::Unary : Prec::Primary };
                }
                case Expr::Kind::Str:
                    return { expr.text, Prec::Primary };
                case Expr::Kind::OctetLit:
                    return { expr.text, Prec::Primary };
                case Expr::Kind::RegExp:
                    return { fmt::format("/{}/", expr.text), Prec::Postfix };
                case Expr::Kind::LocalName:
                    return { expr.text, Prec::Primary };
                case Expr::Kind::ThisVal:
                    return { "this", Prec::Primary };
                case Expr::Kind::GlobalObj:
                    return { "global", Prec::Primary };
                case Expr::Kind::SuperVal:
                    return { "super", Prec::Primary };
                case Expr::Kind::Copy:
                    return { expr.srcVersion == INVALID ? std::string{ "<copy>" } : renderVersion(expr.srcVersion),
                             Prec::Primary };
                case Expr::Kind::FuncLit: {
                    // 匿名函数字面量（实参/赋值右值）：渲染期经宿主装配多行文本。
                    // 相对缩进——首行顶格随所在表达式嵌入，体内层行带装配层级缩进
                    if(!_host) {
                        fail("匿名函数字面量缺少装配宿主");
                        return { "<invalid>", Prec::Primary };
                    }
                    const auto &constant = _chunk.getConstants()[static_cast<size_t>(expr.integer)];
                    std::string reason;
                    auto text = _host->declarationText(constant, DeclForm::Assign, {}, 0, reason);
                    if(!reason.empty()) {
                        fail(reason);
                        return { "<invalid>", Prec::Primary };
                    }
                    return { std::move(text), Prec::Primary };
                }
                case Expr::Kind::Member:
                case Expr::Kind::Index: {
                    const auto obj = renderParen(expr.a, Prec::Postfix, false);
                    if(expr.kind == Expr::Kind::Index)
                        return { fmt::format("{}[{}]", obj.text, renderNode(expr.b).text), Prec::Postfix };
                    if(expr.dotForm)
                        return { fmt::format("{}.{}", obj.text, expr.text), Prec::Postfix };
                    if(expr.b != INVALID)
                        return { fmt::format("{}[{}]", obj.text, renderNode(expr.b).text), Prec::Postfix };
                    return { fmt::format("{}[{}]", obj.text, escapeString(expr.text)), Prec::Postfix };
                }
                case Expr::Kind::ArrayLit: {
                    std::string out = "[";
                    for(size_t i = 0; i < expr.items.size(); ++i)
                        out += fmt::format("{}{}", i ? ", " : "", renderNode(expr.items[i]).text);
                    return { out + "]", Prec::Primary };
                }
                case Expr::Kind::DictLit: {
                    std::string out = "%[";
                    for(size_t i = 0; i + 1 < expr.items.size(); i += 2)
                        out += fmt::format("{}{} => {}", i ? ", " : "", renderNode(expr.items[i]).text,
                                           renderNode(expr.items[i + 1]).text);
                    return { out + "]", Prec::Primary };
                }
                case Expr::Kind::Call:
                case Expr::Kind::Transmit: {
                    auto callee = renderParen(expr.args[0], Prec::Postfix, false);
                    if(expr.kind == Expr::Kind::Transmit)
                        return { callee.text + "(...)", Prec::Postfix };
                    std::string out = std::move(callee.text) + "(";
                    for(size_t i = 1; i < expr.args.size(); ++i)
                        out += fmt::format("{}{}", i > 1 ? ", " : "", renderNode(expr.args[i]).text);
                    return { out + ")", Prec::Postfix };
                }
                case Expr::Kind::Unary: {
                    const auto operand = renderNode(expr.a);
                    const bool word =
                        std::isalnum(static_cast<unsigned char>(expr.text.empty() ? 0 : expr.text[0])) != 0 ||
                        expr.text == "isvalid" || expr.text == "delete";
                    // 符号一元对一元操作数加括号（防 --x 词形冲突）；词形一元不要求
                    const auto floor = word ? static_cast<u8>(Prec::Unary) : static_cast<u8>(Prec::Unary) + 1;
                    std::string inner = operand.text;
                    if(static_cast<u8>(operand.prec) < floor ||
                       (!word && static_cast<u8>(operand.prec) <= static_cast<u8>(Prec::Unary)))
                        inner = fmt::format("({})", inner);
                    return { fmt::format("{}{}{}", expr.text, word ? " " : "", inner), Prec::Unary };
                }
                case Expr::Kind::IncDec: {
                    const auto operand = renderParen(expr.a, Prec::Postfix, false);
                    return { operand.text + expr.text, Prec::Postfix };
                }
                case Expr::Kind::Binary: {
                    const auto prec = precedenceOf(expr.text);
                    auto lhs = renderParen(expr.a, prec, false);
                    auto rhs = renderParen(expr.b, prec, true);
                    return { fmt::format("{} {} {}", lhs.text, expr.text, rhs.text), prec };
                }
                case Expr::Kind::Ternary: {
                    // a = 条件，b = then 值，integer 复用为 else 值节点下标
                    const auto cond = renderParen(expr.a, Prec::Ternary, false);
                    const auto thenV = renderParen(expr.b, Prec::Assign, false);
                    const auto elseV =
                        renderParen(expr.integer == INVALID ? INVALID : expr.integer, Prec::Assign, false);
                    return { fmt::format("{} ? {} : {}", cond.text, thenV.text, elseV.text), Prec::Ternary };
                }
            }
            return { "<unknown>", Prec::Primary };
        }

        // 子表达式优先级低于 floor（或等于且为右端）时加括号
        [[nodiscard]] Rendered renderParen(const u32 node, const Prec floor, const bool strictRight) {
            auto rendered = renderNode(node);
            if(static_cast<u8>(rendered.prec) < static_cast<u8>(floor) || (strictRight && rendered.prec == floor))
                rendered.text = fmt::format("({})", rendered.text);
            return rendered;
        }

        [[nodiscard]] static Prec precedenceOf(const std::string &text) {
            static const std::map<std::string, Prec> table = {
                { "||", Prec::Lor },   { "&&", Prec::Land },  { "|", Prec::BitOr },       { "^", Prec::BitXor },
                { "&", Prec::BitAnd }, { "==", Prec::Eq },    { "!=", Prec::Eq },         { "===", Prec::Eq },
                { "!==", Prec::Eq },   { "<", Prec::Rel },    { "<=", Prec::Rel },        { ">", Prec::Rel },
                { ">=", Prec::Rel },   { "<<", Prec::Shift }, { ">>", Prec::Shift },      { ">>>", Prec::Shift },
                { "+", Prec::Add },    { "-", Prec::Add },    { "*", Prec::Mul },         { "/", Prec::Mul },
                { "\\", Prec::Mul },   { "%", Prec::Mul },    { "instanceof", Prec::Rel }
            };
            const auto it = table.find(text);
            return it == table.end() ? Prec::Primary : it->second;
        }

        // ── 发射（按 pc 序；物化临时与语句 tile 交错）──
        void emitStatements(Vec<RecoveredStatement> &out) {
            std::unordered_map<u32, const Stmt *> stmtAtPc;
            for(const auto &stmt : _stmts)
                stmtAtPc[stmt.pc] = &stmt;

            const auto count = _chunk.getInstVec().size();
            for(size_t pc = 0; pc < count; ++pc) {
                // 该 pc 上的物化临时定义（先于消费它的语句）；自体表达式直接
                // 渲染节点（renderVersion 会返回临时名自身）
                for(size_t reg = 0; reg < _defs.size(); ++reg) {
                    for(size_t v = 0; v < _defs[reg].size(); ++v) {
                        auto &def = _defs[reg][v];
                        if(!def.materialized || def.defPc != pc)
                            continue;
                        out.push_back(RecoveredStatement{
                            .pc = static_cast<u32>(pc),
                            .text = fmt::format("var {} = {};", def.tempName, renderNode(def.node).text) });
                    }
                }
                const auto it = stmtAtPc.find(static_cast<u32>(pc));
                if(it == stmtAtPc.end())
                    continue;
                const auto &stmt = *it->second;
                std::string text;
                if(!renderStmt(stmt, text))
                    return;
                if(!text.empty())
                    out.push_back(RecoveredStatement{
                        .pc = static_cast<u32>(pc), .text = std::move(text), .tacEnd = stmt.tacEnd });
            }
        }

        [[nodiscard]] bool renderStmt(const Stmt &stmt, std::string &out, const u32 indent = 0) {
            // slotOf 失败时已记录原因；此处防 INVALID 槽位进入渲染（versionOf 越界）。
            // CompoundAssign/Decl 的槽位在各自 case 内按需校验
            const bool needsObj = stmt.kind == Stmt::Kind::StoreMember || stmt.kind == Stmt::Kind::StoreIndex ||
                stmt.kind == Stmt::Kind::StoreDeref || stmt.kind == Stmt::Kind::DeleteMember;
            const bool needsSrc = stmt.exprNode == INVALID && stmt.kind != Stmt::Kind::DebuggerStmt &&
                stmt.kind != Stmt::Kind::DeleteMember && stmt.kind != Stmt::Kind::ExprStmt &&
                stmt.kind != Stmt::Kind::Decl && stmt.kind != Stmt::Kind::CompoundAssign;
            if((needsObj && stmt.obj == INVALID) || (needsSrc && stmt.src == INVALID) ||
               (stmt.kind == Stmt::Kind::StoreIndex && stmt.idx == INVALID)) {
                fail("无效寄存器槽位");
                return false;
            }
            switch(stmt.kind) {
                case Stmt::Kind::StoreGlobal: {
                    auto name = atomText(_rt, stmt.atom);
                    if(!isPlainMemberName(name)) {
                        fail("全局名非标识符，无法打印");
                        return false;
                    }
                    out = fmt::format("{}{} = {};", stmt.declare ? "var " : "", name, renderVersion(stmt.src));
                    return true;
                }
                case Stmt::Kind::StoreLocal: {
                    std::string value;
                    if(stmt.exprNode != INVALID)
                        value = renderNode(stmt.exprNode).text;
                    else
                        value = renderVersion(stmt.src);
                    // 自赋值 no-op（TJS2 转译 prologue 的参数搬运）跳过
                    if(!stmt.declare && value == stmt.name)
                        return true;
                    if(stmt.declare && stmt.exprNode != INVALID && _nodes[stmt.exprNode].kind == Expr::Kind::Void) {
                        out = fmt::format("var {};", stmt.name);
                        return true;
                    }
                    // 自增结果直接回写同名目标 → x++ 语句形态
                    const u32 srcNode = stmt.exprNode != INVALID
                        ? stmt.exprNode
                        : (stmt.src == INVALID ? INVALID : versionOf(stmt.src).node);
                    if(srcNode != INVALID && _nodes[srcNode].kind == Expr::Kind::IncDec) {
                        const auto target = renderNode(_nodes[srcNode].a).text;
                        if(target == stmt.name) {
                            out = fmt::format("{}{};", stmt.name, _nodes[srcNode].text);
                            return true;
                        }
                        fail("自增结果非直接回写同名目标");
                        return false;
                    }
                    out = fmt::format("{}{} = {};", stmt.declare ? "var " : "", stmt.name, value);
                    return true;
                }
                case Stmt::Kind::StoreMember: {
                    const auto obj = renderVersion(stmt.obj);
                    std::string target;
                    if(stmt.idx != INVALID)
                        target = fmt::format("{}[{}]", obj, renderVersion(stmt.idx));
                    else {
                        const auto name = atomText(_rt, stmt.atom);
                        target = isPlainMemberName(name) ? fmt::format("{}.{}", obj, name)
                                                         : fmt::format("{}[{}]", obj, escapeString(name));
                    }
                    // 自增回写：OBJ.M 的 IncDec 语句形态（目标文本一致才可恢复）
                    const auto &srcDef = versionOf(stmt.src);
                    if(srcDef.node != INVALID && _nodes[srcDef.node].kind == Expr::Kind::IncDec) {
                        const auto &inc = _nodes[srcDef.node];
                        if(renderNode(inc.a).text == target && !stmt.raw) {
                            out = fmt::format("{}{};", target, inc.text);
                            return true;
                        }
                        fail("自增结果非直接回写同一目标");
                        return false;
                    }
                    out = fmt::format("{}{} = {};", stmt.raw ? "&" : "", target, renderVersion(stmt.src));
                    return true;
                }
                case Stmt::Kind::StoreIndex: {
                    const auto target = fmt::format("{}[{}]", renderVersion(stmt.obj), renderVersion(stmt.idx));
                    const auto &srcDef = versionOf(stmt.src);
                    if(srcDef.node != INVALID && _nodes[srcDef.node].kind == Expr::Kind::IncDec) {
                        const auto &inc = _nodes[srcDef.node];
                        if(renderNode(inc.a).text == target) {
                            out = fmt::format("{}{};", target, inc.text);
                            return true;
                        }
                        fail("自增结果非直接回写同一目标");
                        return false;
                    }
                    out = fmt::format("{} = {};", target, renderVersion(stmt.src));
                    return true;
                }
                case Stmt::Kind::StoreDeref:
                    out = fmt::format("*{} = {};", renderVersion(stmt.obj), renderVersion(stmt.src));
                    return true;
                case Stmt::Kind::CompoundAssign: {
                    // TARGET OP= RHS / TARGET++（M4 逆融合；目标文本一致性已由
                    // 融合守卫验证）
                    std::string target;
                    if(stmt.atom != ATOM_INVALID || stmt.idx != INVALID) {
                        const auto obj = renderVersion(stmt.obj);
                        if(stmt.atom != ATOM_INVALID) {
                            const auto name = atomText(_rt, stmt.atom);
                            target = isPlainMemberName(name) ? fmt::format("{}.{}", obj, name)
                                                             : fmt::format("{}[{}]", obj, escapeString(name));
                        } else {
                            target = fmt::format("{}[{}]", obj, renderVersion(stmt.idx));
                        }
                        if(stmt.raw)
                            target = fmt::format("&{}", target);
                    } else if(stmt.obj != INVALID) {
                        target = fmt::format("*{}", renderVersion(stmt.obj));
                    } else {
                        target = stmt.name;
                    }
                    if(stmt.rhsNode == INVALID && (stmt.opText == "++" || stmt.opText == "--")) {
                        out = fmt::format("{}{};", target, stmt.opText);
                        return true;
                    }
                    if(stmt.rhsNode == INVALID) {
                        fail("复合赋值缺少右操作数");
                        return false;
                    }
                    out = fmt::format("{} {} {};", target, stmt.opText, renderNode(stmt.rhsNode).text);
                    return true;
                }
                case Stmt::Kind::Decl: {
                    // 声明装配（M4 壳组装）：宿主装配多行声明块
                    if(!_host) {
                        fail("声明装配缺少宿主上下文");
                        return false;
                    }
                    const auto &constant = _chunk.getConstants()[stmt.constIdx];
                    std::string reason;
                    DeclForm form = DeclForm::Bare;
                    std::string literalName = stmt.name;
                    if(constant.type() == ConstantType::FuncMeta) {
                        const auto ownName = atomText(_rt, constant.value<FuncMeta *>()->name);
                        // 裸声明：FunctionDecl 形态（ChgThis / DThis / 零拷贝具名槽），
                        // 或全局安装且与函数自身名一致；否则按匿名字面量赋值恢复
                        const bool bare = stmt.contextInstall || stmt.declInstall == 0 ||
                            (stmt.declInstall == 1 && !ownName.empty() && (stmt.name.empty() || stmt.name == ownName));
                        if(!bare) {
                            form = DeclForm::Assign;
                            literalName = {};
                        } else {
                            literalName = !ownName.empty() ? ownName : stmt.name;
                        }
                    } else if(constant.type() == ConstantType::ClassMeta) {
                        const auto className = atomText(_rt, constant.value<ClassMeta *>()->className);
                        if(stmt.name.empty() || (!className.empty() && stmt.name != className)) {
                            fail(fmt::format("类名与安装名不一致（{} vs {}）", className, stmt.name));
                            return false;
                        }
                        literalName = className;
                    } else if(constant.type() == ConstantType::PropMeta) {
                        if(stmt.name.empty()) {
                            fail("property 安装名缺失");
                            return false;
                        }
                    } else {
                        fail("声明常量类型未知");
                        return false;
                    }
                    out = _host->declarationText(constant, form, literalName, indent, reason);
                    if(!reason.empty()) {
                        fail(reason);
                        return false;
                    }
                    // 赋值形态包一层：var 前缀/成员目标 + 分号（字面量首行并入赋值行）
                    if(form == DeclForm::Assign) {
                        std::string prefix;
                        if(stmt.obj != INVALID) {
                            const auto obj = renderVersion(stmt.obj);
                            const auto name = stmt.atom != ATOM_INVALID ? atomText(_rt, stmt.atom) : stmt.name;
                            prefix = stmt.atom != ATOM_INVALID && isPlainMemberName(name)
                                ? fmt::format("{}.{}", obj, name)
                                : fmt::format("{}[{}]", obj, escapeString(name));
                            if(stmt.raw)
                                prefix = fmt::format("&{}", prefix);
                        } else {
                            prefix = fmt::format("{}{}", stmt.declare ? "var " : "", stmt.name);
                        }
                        const auto nl = out.find('\n');
                        if(nl == std::string::npos) {
                            out = prefix + " = " + out + ";";
                        } else {
                            auto rest = out.substr(nl);
                            rest += ';';
                            out = prefix + " = " + out.substr(0, nl) + rest;
                        }
                    }
                    return true;
                }
                case Stmt::Kind::DeleteMember: {
                    const auto obj = renderVersion(stmt.obj);
                    if(stmt.idx != INVALID)
                        out = fmt::format("delete {}[{}];", obj, renderVersion(stmt.idx));
                    else {
                        const auto name = atomText(_rt, stmt.atom);
                        out = isPlainMemberName(name) ? fmt::format("delete {}.{};", obj, name)
                                                      : fmt::format("delete {}[{}];", obj, escapeString(name));
                    }
                    return true;
                }
                case Stmt::Kind::ExprStmt:
                    out = renderNode(stmt.exprNode).text + ";";
                    return true;
                case Stmt::Kind::Return: {
                    const auto &def = versionOf(stmt.src);
                    if(def.node != INVALID && _nodes[def.node].kind == Expr::Kind::Void) {
                        // 函数末尾的隐式 void return 省略；其余位置输出 return;
                        if(_functionLike && stmt.pc + 1 == _chunk.getInstVec().size())
                            return true;
                        out = "return;";
                        return true;
                    }
                    out = fmt::format("return {};", renderVersion(stmt.src));
                    return true;
                }
                case Stmt::Kind::ThrowStmt:
                    out = fmt::format("throw {};", renderVersion(stmt.src));
                    return true;
                case Stmt::Kind::Invalidate:
                    out = fmt::format("invalidate {};", renderVersion(stmt.src));
                    return true;
                case Stmt::Kind::DebuggerStmt:
                    out = "debugger;";
                    return true;
            }
            return true;
        }

        // ── M3 语句结构化 ──

        struct WhileLoop {
            u32 exitPc{}; // JmpNE 出口（break 目标）；INVALID = while (true)
            u32 latchJmpPc{}; // 回边 Jmp 的 pc（体区结束）
            u32 updatePc{ INVALID }; // for 更新块入口（由锁存块的前向入边证明）
            u32 continuePc{}; // continue 目标（头块条件区起始）
            bool hasCond{}; // false = while (true)
        };

        struct DoLoop {
            u32 bodyStart{};
            u32 condStart{}; // 条件比较起始（体区结束）
            u32 condJumpPc{}; // JmpNE pc
            u32 exitPc{}; // JmpNE 目标
            u32 backTarget{}; // Jmp 目标（体首）
        };

        struct LoopCtx {
            u32 breakPc{ INVALID }; // 循环出口（break 目标）
            u32 continuePc{ INVALID }; // 循环条件（while 的 continue 目标）
            u32 updatePc{ INVALID }; // continue 前执行的 for 更新区
            u32 latchPc{ INVALID }; // 锁存 Jmp 的 pc（for 形态 continue 经锁存块中转）
        };

        // try/catch 区域（M5）：[tryStart, tryEnd) 体区 + [tryEnd, joinPc) catch 体。
        // handler 表内层优先（IRGen 与转译器共同保证）；出口取体区末尾跳过 catch
        // 的 Jmp 目标（IRGen 模板恒有；优化器不折叠不可达 Jmp，转译器要求 catch
        // 目标落在指令边界）
        struct TryRegion {
            u32 tryStart{};
            u32 tryEnd{};
            u32 joinPc{};
            u16 exValueReg{}; // vm::ThrowHandler::exValueRegNone = 无绑定
            std::string catchName; // catch 变量名（具名局部或合成临时）
            u32 exVersion{ INVALID }; // Pass A 在 tryEnd 登记的异常槽版本
        };

        // 寄存器是否已是合成存储（switch 合流 / 既有提升）：读取直接走变量语义
        [[nodiscard]] bool storageReady(const u16 regIndex) const {
            return _promotedStorage.contains(regIndex) || _switchLocalNames.contains(regIndex);
        }

        // 将无名寄存器提升为函数局部合成存储（catch 数据流 / 无名循环携带共用）。
        // 存储名从最早定义之后可见（对齐 localNameAt 的"定义后"契约）；入口声明
        // 是唯一的 var，后续命名定义经 _declaredLocals 去掉 declare 前缀；参数槽
        // 以入口拷贝初始化。返回 false = 单元内无定义点（调用方拒绝）
        bool promoteRegister(const u16 regIndex) {
            if(storageReady(regIndex))
                return true;
            auto &code = _chunk.getInstVec();
            u32 firstDef = INVALID;
            for(u32 pc = 0; pc < code.size() && firstDef == INVALID; ++pc) {
                forEachDef(*code[pc], [&](const Register r, const bool) {
                    if(r.index() == regIndex && firstDef == INVALID)
                        firstDef = pc;
                });
            }
            if(firstDef == INVALID)
                return false;
            const auto storageName = nextTempName();
            _promotedStorage.emplace(regIndex, std::pair{ storageName, firstDef + 1 });
            _declaredLocals.emplace(regIndex, storageName);
            if(_meta && regIndex < _meta->arity) {
                // 参数槽在体内重定义：入口拷贝兜住"重定义前"的读取（与状态机
                // var vN = aN 同语义）
                const auto src = localNameAt(Register{ regIndex }, 0);
                _promotedParamCopies.emplace(regIndex, src.empty() ? fmt::format("a{}", regIndex) : src);
            }
            return true;
        }

        // 无名循环携带变量提升：循环条件读取、体内（含更新区）重定义、无名字
        // 绑定的寄存器提升为合成存储——条件渲染变量名保住迭代语义，自增经
        // IncDec 语句形态渲染，跨迭代读取即变量当前值。具名/参数读取本就是
        // 变量语义，不提升
        void precomputeLoopStorage() {
            if(!_meta || !_functionLike)
                return;
            auto &code = _chunk.getInstVec();
            auto scanCond = [&](const u32 condBegin, const u32 condEnd, const u32 bodyBegin, const u32 bodyEndIncl) {
                std::unordered_set<u16> reads;
                for(u32 pc = condBegin; pc < condEnd; ++pc) {
                    if(_foldedPcs.contains(pc))
                        continue;
                    forEachUse(*code[pc], [&](const Register r) { reads.insert(r.index()); });
                }
                for(const auto regIndex : reads) {
                    if(storageReady(regIndex) || _promotedStorage.contains(regIndex))
                        continue;
                    if(!localNameAt(Register{ regIndex }, condEnd).empty())
                        continue; // 具名/参数：读变量名即运行时值
                    bool redefined = false;
                    for(u32 pc = bodyBegin; pc <= bodyEndIncl && !redefined; ++pc) {
                        forEachDef(*code[pc], [&](const Register r, const bool) {
                            if(r.index() == regIndex)
                                redefined = true;
                        });
                    }
                    if(!redefined)
                        continue; // 体内无重定义：线性链绑定条件外定义，渲染正确
                    if(!promoteRegister(regIndex))
                        fail(fmt::format("循环条件读取的寄存器 %{} 缺少定义点", regIndex));
                }
            };
            for(const auto &[blockIdx, loop] : _whileLoops) {
                const auto &zone = _condZones[blockIdx];
                if(!loop.hasCond || zone.firstPc == INVALID)
                    continue;
                scanCond(zone.firstPc, static_cast<u32>(_blocks[blockIdx].end), static_cast<u32>(_blocks[blockIdx].end),
                         loop.latchJmpPc);
            }
            for(const auto &[bodyStart, loop] : _doLoops) {
                const auto latchBlock = blockAtPc(loop.condJumpPc);
                if(latchBlock >= _blocks.size())
                    continue;
                const auto &zone = _condZones[latchBlock];
                if(zone.firstPc == INVALID)
                    continue;
                scanCond(loop.condStart, loop.condJumpPc + 1, loop.bodyStart,
                         loop.condStart > 0 ? loop.condStart - 1 : 0);
            }
        }

        void precomputeTryRegions() {
            const auto &handlers = _chunk.getThrowHandlers();
            if(handlers.empty())
                return;
            auto &code = _chunk.getInstVec();
            for(const auto &handler : handlers) {
                if(handler.tryStart >= handler.tryEnd || handler.tryEnd >= code.size()) {
                    fail("CFG 异常区域或 catch 入口越界");
                    return;
                }
            }
            // 区域两两真嵌套或不相交；完全重叠 = 同一区域重复登记，归属不明
            for(size_t i = 0; i < handlers.size(); ++i) {
                for(size_t j = i + 1; j < handlers.size(); ++j) {
                    const auto &a = handlers[i];
                    const auto &b = handlers[j];
                    if(a.tryStart == b.tryStart && a.tryEnd == b.tryEnd) {
                        fail("异常区域完全重叠，无法结构化（M5）");
                        return;
                    }
                    const bool disjoint = b.tryStart >= a.tryEnd || a.tryStart >= b.tryEnd;
                    const bool nested = (b.tryStart >= a.tryStart && b.tryEnd <= a.tryEnd) ||
                        (a.tryStart >= b.tryStart && a.tryEnd <= b.tryEnd);
                    if(!disjoint && !nested) {
                        fail("异常区域部分重叠，无法结构化（M5）");
                        return;
                    }
                }
            }
            // 名字避让与既有合成存储一致：参数、局部与 Atom（catch 变量名之后
            // 供临时物化分配器共用）
            for(u16 p = 0; p < (_meta ? _meta->arity : u16{ 0 }); ++p)
                _usedNames.insert(fmt::format("a{}", p));
            for(const auto &local : _meta ? _meta->localVars : Vec<inter::LocalVariable>{})
                if(local.identifier != ATOM_INVALID)
                    _usedNames.insert(atomText(_rt, local.identifier));
            for(const auto &inst : code) {
                for(const auto &operand : { inst->getOp1(), inst->getOp2(), inst->getOp3() }) {
                    if(operand.type() == inter::Operand::Type::Atom)
                        _usedNames.insert(atomText(_rt, operand.value<Atom>()));
                }
            }
            for(const auto &handler : handlers) {
                // 出口证明：体区末块尾必须是跳过 catch 体的 Jmp。体区以 Ret/Throw
                // 收尾（Jmp 不可达仍保留于自产路径；缺失即形态未证明）→ 拒绝
                if(code[handler.tryEnd - 1]->opcode() != TacOpCode::Jmp) {
                    fail("try 体缺少跳过 catch 的出口跳转，结构化恢复属后续批次（M5）");
                    return;
                }
                const auto join = static_cast<u32>(code[handler.tryEnd - 1]->getOp1().value<Label>().address());
                if(join < handler.tryEnd || join > code.size()) {
                    fail("try 出口越界（M5）");
                    return;
                }
                // join 必须是块边界（Jmp 目标即 leader；函数尾除外）
                if(join < code.size() && blockAtPc(join) < _blocks.size() && _blocks[blockAtPc(join)].start != join) {
                    fail("try 出口未落在块边界（M5）");
                    return;
                }
                // catch 体区域唯一入口证明：区域外块的普通后继不得落入
                // [tryEnd, join)（异常只能经 unwind 进入 catch；区域内的 try 体
                // 异常边由 CFG 已建模）
                for(size_t bi = 0; bi < _blocks.size(); ++bi) {
                    const auto &block = _blocks[bi];
                    if(block.start >= handler.tryStart && block.start < join)
                        continue;
                    for(const auto next : block.successors) {
                        const auto &succ = _blocks[next];
                        if(succ.start >= handler.tryEnd && succ.start < join) {
                            fail("catch 体区域存在区域外的第二入口（M5）");
                            return;
                        }
                    }
                }
                // 数据流守卫：catch 体读取 try 体内定义的寄存器时，SSA 版本在异
                // 常时点不明确（异常可发生在定义前的任意 pc）。无名寄存器提升为
                // 函数局部合成存储（全部定义经 noteNamedDef 落为同名赋值，catch
                // 读取即抛出点值；spanClean 把跨越提升存储重定义的内联判脏），
                // 与 switch 合流存储同机制。具名局部不提升：优化器删除体外死初
                // 始化后幸存定义内移，`var` 声明落点随之漂移，块作用域下 catch
                // 不可见（声明落点守卫保持拒绝）。读取之前 catch 体内已有同号
                // 寄存器定义（复用）的线性链绑定 catch 局部定义，无需提升。
                // exValueReg 由 VM 写入，经 Pass A/B 重绑定放行
                std::unordered_set<u16> bodyDefs;
                for(u32 pc = handler.tryStart; pc < handler.tryEnd; ++pc)
                    forEachDef(*code[pc], [&](const Register reg, const bool) { bodyDefs.insert(reg.index()); });
                std::unordered_set<u16> contested;
                for(u32 pc = handler.tryEnd; pc < join; ++pc) {
                    forEachUse(*code[pc], [&](const Register reg) {
                        if(reg.index() == handler.exValueReg || !bodyDefs.contains(reg.index()))
                            return;
                        for(u32 q = handler.tryEnd; q < pc; ++q) {
                            bool redefined = false;
                            forEachDef(*code[q], [&](const Register r, const bool) {
                                if(r.index() == reg.index())
                                    redefined = true;
                            });
                            if(redefined)
                                return; // 更近的 catch 局部定义
                        }
                        contested.insert(reg.index());
                    });
                }
                for(const auto regIndex : contested) {
                    if(storageReady(regIndex))
                        continue; // 已是合成存储（本 handler 提升 / switch 合流）
                    if(!localNameAt(Register{ regIndex }, handler.tryEnd).empty()) {
                        fail(fmt::format("catch 体读取 try 体内定义的具名寄存器 %{}，声明落点未证明（M5）", regIndex));
                        return;
                    }
                    if(!promoteRegister(regIndex)) {
                        fail(fmt::format("catch 体读取的寄存器 %{} 缺少定义点", regIndex));
                        return;
                    }
                }
                // 声明落点守卫：具名局部最早的命名定义点若落在 try 体内且体外
                // （catch 体 / try 后代码 / 体前）存在同名引用，`var` 声明会随
                // 幸存定义内移（优化器删除体外死初始化时不重映射 localVars），
                // 块作用域下 catch 与 try 后代码不可见——保守拒绝
                if(_meta) {
                    for(const auto &local : _meta->localVars) {
                        if(local.identifier == ATOM_INVALID)
                            continue;
                        const auto regIndex = local.reg.index();
                        const auto name = atomText(_rt, local.identifier);
                        bool defInBody = false;
                        for(u32 pc = handler.tryStart; pc < handler.tryEnd && !defInBody; ++pc) {
                            forEachDef(*code[pc], [&](const Register r, const bool) {
                                if(r.index() == regIndex && localNameAt(r, pc + 1) == name)
                                    defInBody = true;
                            });
                        }
                        if(!defInBody)
                            continue;
                        bool defBefore = false;
                        for(u32 pc = 0; pc < handler.tryStart && !defBefore; ++pc) {
                            forEachDef(*code[pc], [&](const Register r, const bool) {
                                if(r.index() == regIndex && localNameAt(r, pc + 1) == name)
                                    defBefore = true;
                            });
                        }
                        if(defBefore)
                            continue;
                        bool referencedOutside = false;
                        for(u32 pc = 0; pc < code.size() && !referencedOutside; ++pc) {
                            if(pc >= handler.tryStart && pc < handler.tryEnd)
                                continue;
                            forEachUse(*code[pc], [&](const Register r) {
                                if(r.index() == regIndex && localNameAt(r, pc) == name)
                                    referencedOutside = true;
                            });
                            forEachDef(*code[pc], [&](const Register r, const bool) {
                                if(r.index() == regIndex && localNameAt(r, pc + 1) == name)
                                    referencedOutside = true;
                            });
                        }
                        if(referencedOutside) {
                            fail(fmt::format("局部 {} 的声明落点随幸存定义内移进 try 体，块作用域可见性未证明（M5）",
                                             name));
                            return;
                        }
                    }
                }
                std::string catchName;
                if(handler.exValueReg != vm::ThrowHandler::exValueRegNone)
                    catchName = localNameAt(Register{ handler.exValueReg }, handler.tryEnd);
                if(catchName.empty())
                    catchName = nextTempName();
                _usedNames.insert(catchName);
                _tryHeads.emplace(static_cast<u32>(handler.tryStart),
                                  TryRegion{ .tryStart = static_cast<u32>(handler.tryStart),
                                             .tryEnd = static_cast<u32>(handler.tryEnd),
                                             .joinPc = join,
                                             .exValueReg = handler.exValueReg,
                                             .catchName = std::move(catchName) });
            }
        }

        [[nodiscard]] size_t blockAtPc(const u32 pc) const {
            for(size_t i = 0; i < _blocks.size(); ++i)
                if(_blocks[i].start <= pc && pc < _blocks[i].end)
                    return i;
            return _blocks.size();
        }

        [[nodiscard]] u32 loopConditionStart(size_t bi) const {
            u32 pc = _condZones[bi].firstPc;
            const auto &code = _chunk.getInstVec();
            while(pc > _blocks[bi].start) {
                const auto op = code[pc - 1]->opcode();
                if(op != TacOpCode::Load && op != TacOpCode::LoadImm && op != TacOpCode::GThis &&
                   op != TacOpCode::GGlobal && op != TacOpCode::GLocal)
                    break;
                --pc;
            }
            return pc;
        }

        void precomputeLoops() {
            auto &code = _chunk.getInstVec();
            Vec<bool> reachable(_blocks.size(), false);
            Vec<size_t> pending;
            if(!_blocks.empty())
                pending.push_back(0);
            while(!pending.empty()) {
                const auto bi = pending.back();
                pending.pop_back();
                if(reachable[bi])
                    continue;
                reachable[bi] = true;
                for(const auto next : _blocks[bi].successors)
                    pending.push_back(next);
            }
            for(size_t bi = 0; bi < _blocks.size(); ++bi) {
                if(!reachable[bi])
                    continue;
                const auto &block = _blocks[bi];
                const size_t lastPc = block.end - 1;
                if(code[lastPc]->opcode() == TacOpCode::JmpE &&
                   code[lastPc]->getOp1().value<Label>().address() <= block.start &&
                   _condZones[bi].firstPc != INVALID) {
                    const auto body = static_cast<u32>(code[lastPc]->getOp1().value<Label>().address());
                    _doLoops.emplace(body,
                                     DoLoop{ .bodyStart = body,
                                             .condStart = loopConditionStart(bi),
                                             .condJumpPc = static_cast<u32>(lastPc),
                                             .exitPc = static_cast<u32>(block.end),
                                             .backTarget = body });
                    continue;
                }
                if(code[lastPc]->opcode() != TacOpCode::Jmp)
                    continue;
                const auto target = code[lastPc]->getOp1().value<Label>().address();
                if(target >= block.start)
                    continue; // 前向跳转非回边
                // 同块 do-while：[比较…, JmpNE→exit, Jmp→bodyStart]
                if(lastPc > block.start && code[lastPc - 1]->opcode() == TacOpCode::JmpNE) {
                    const auto exitTarget = code[lastPc - 1]->getOp1().value<Label>().address();
                    if(_condZones[bi].firstPc != INVALID && exitTarget == block.end &&
                       target < _condZones[bi].firstPc) {
                        DoLoop loop{ .bodyStart = static_cast<u32>(target),
                                     .condStart = static_cast<u32>(_condZones[bi].firstPc),
                                     .condJumpPc = static_cast<u32>(lastPc - 1),
                                     .exitPc = static_cast<u32>(exitTarget),
                                     .backTarget = static_cast<u32>(target) };
                        _doLoops.emplace(loop.bodyStart, loop);
                    }
                    continue;
                }
                // 两块 do-while：前块以 JmpNE→本块末尾结束（条件区），本块仅 Jmp→体首
                if(bi > 0 && _blocks[bi - 1].end == block.start) {
                    const auto &cond = _blocks[bi - 1];
                    const size_t condLast = cond.end - 1;
                    const auto exitTarget = code[condLast]->opcode() == TacOpCode::JmpNE
                        ? code[condLast]->getOp1().value<Label>().address()
                        : INVALID;
                    // 体严格在条件块之前（while 的回边目标 == 头块 start，据此区分）
                    if(code[condLast]->opcode() == TacOpCode::JmpNE && exitTarget == block.end &&
                       _condZones[bi - 1].firstPc != INVALID && target < cond.start) {
                        DoLoop loop{ .bodyStart = static_cast<u32>(target),
                                     .condStart = loopConditionStart(bi - 1),
                                     .condJumpPc = static_cast<u32>(condLast),
                                     .exitPc = static_cast<u32>(exitTarget),
                                     .backTarget = static_cast<u32>(target) };
                        _doLoops.emplace(loop.bodyStart, loop);
                        continue;
                    }
                }
                // while 锁存：Jmp→头块；若锁存目标到条件区之间含存储（do 的体
                // 语句），实为 do-while（体 + 条件同块/跨块）
                const size_t header = blockAtPc(static_cast<u32>(target));
                if(header >= _blocks.size() || header == bi)
                    continue;
                const auto &zone = _condZones[header];
                WhileLoop loop{ .exitPc = INVALID,
                                .latchJmpPc = static_cast<u32>(lastPc),
                                .continuePc = static_cast<u32>(target),
                                .hasCond = false };
                if(zone.firstPc != INVALID && code[_blocks[header].end - 1]->opcode() == TacOpCode::JmpNE) {
                    const auto exitTarget = code[_blocks[header].end - 1]->getOp1().value<Label>().address();
                    if(exitTarget > _blocks[header].end && exitTarget > loop.latchJmpPc) {
                        loop.exitPc = static_cast<u32>(exitTarget);
                        loop.hasCond = true;
                    }
                }
                if(loop.hasCond && hasBodyStoreBeforeCond(static_cast<u32>(target), zone.firstPc)) {
                    DoLoop doLoop{ .bodyStart = static_cast<u32>(target),
                                   .condStart = static_cast<u32>(zone.firstPc),
                                   .condJumpPc = static_cast<u32>(_blocks[header].end - 1),
                                   .exitPc = loop.exitPc,
                                   .backTarget = static_cast<u32>(target) };
                    _doLoops.emplace(doLoop.bodyStart, doLoop);
                    continue;
                }
                // 前向 continue 入锁存块时，该块前缀为 for 更新区。
                // 仅有回边 Jmp 的空前缀无需额外处理。
                if(block.start < lastPc) {
                    for(size_t q = _blocks[header].end; q < block.start; ++q)
                        if(code[q]->opcode() == TacOpCode::Jmp &&
                           code[q]->getOp1().value<Label>().address() == block.start)
                            loop.updatePc = static_cast<u32>(block.start);
                }
                const auto prior = _whileLoops.find(header);
                if(prior == _whileLoops.end() || prior->second.latchJmpPc < loop.latchJmpPc)
                    _whileLoops.insert_or_assign(header, loop);
            }
            // while 体内的 if (...) continue 会呈现 do 的双块尾模板；
            // 同入口更晚的正常回边证明它仍属于外围 while。
            std::erase_if(_doLoops, [&](const auto &entry) {
                const auto wi = _whileLoops.find(blockAtPc(entry.first));
                return wi != _whileLoops.end() && wi->second.latchJmpPc > entry.second.condJumpPc;
            });
        }

        // 锁存目标 → 条件区之间含存储 → do-while 体；纯读取（条件操作数装入）
        // → while 条件前缀。存储源寄存器若正是条件比较的操作数（如 while (--i > 0)
        // 的前缀自减），归属歧义 → 记 Unsupported
        [[nodiscard]] bool hasBodyStoreBeforeCond(const u32 latchTarget, const u32 zoneFirst) {
            auto &code = _chunk.getInstVec();
            bool hasStore = false;
            for(u32 pc = latchTarget; pc < zoneFirst; ++pc) {
                const Instruction &inst = *code[pc];
                Opt<Register> storeSrc;
                switch(inst.opcode()) {
                    case TacOpCode::DGlobal:
                    case TacOpCode::DThis:
                        storeSrc = inst.getOp2().value<Register>();
                        hasStore = true;
                        break;
                    case TacOpCode::SetIndex:
                        storeSrc = inst.getOp3().value<Register>();
                        hasStore = true;
                        break;
                    case TacOpCode::DPropC:
                    case TacOpCode::DPropRawC:
                        storeSrc = inst.getOp3().value<Register>();
                        hasStore = true;
                        break;
                    case TacOpCode::DLocal: {
                        const auto dst = inst.getOp2().value<Register>();
                        if(!localNameAt(dst, pc + 1).empty()) {
                            storeSrc = inst.getOp1().value<Register>();
                            hasStore = true;
                        }
                        break;
                    }
                    default:
                        break;
                }
                if(!storeSrc.has_value())
                    continue;
                // 存储源寄存器与条件比较操作数同寄存器（前缀自减进条件）：歧义
                for(u32 zpc = zoneFirst; zpc < code.size(); ++zpc) {
                    const Instruction &zinst = *code[zpc];
                    if(zinst.opcode() != TacOpCode::CompareTest && zinst.opcode() != TacOpCode::CompareTestImm &&
                       zinst.opcode() != TacOpCode::Test)
                        continue;
                    bool feeds = false;
                    forEachUse(zinst, [&](const Register operand) { feeds = feeds || operand == *storeSrc; });
                    if(feeds) {
                        fail("循环条件内联存储与 do/while 形态歧义");
                        return true;
                    }
                    break; // 只看条件区第一条比较
                }
            }
            return hasStore;
        }

        // ── M5：switch 派发链恢复 ──
        // 自产模板（IRGenerator::generate(SwitchStmtNode) + CompareTestFusion）：
        // 每个匹配值一对 [compare_test EQ, JmpE→体首]（同一 case 的多匹配值相邻
        // 共块），派发头后随单条 [Jmp→下一派发头]；case 体以 break（Jmp→出口）或
        // 贯穿（prevJmp→下一体，体含 break 时为死代码）结束；default 体在链末。
        struct SwitchMatch {
            u32 cmpPc{};
            bool isImm{};
            Integer imm{};
            Register test{}; // 本次比较的判别值寄存器
            Register rhs{}; // 非立即数匹配值的寄存器（节点发射期解析）
        };
        struct SwitchCase {
            Vec<SwitchMatch> matches; // 同一 case 体的一组匹配值
            u32 bodyPc{}; // 体首（首个 JmpE 目标）
            u32 bodyEnd{}; // 体区域末（下一派发头 / default / 出口）
        };
        struct SwitchInfo {
            Register test{}; // 判别值寄存器（各派发比较的 lhs，一致才成链）
            u32 headStart{}; // 派发段起点（块前缀开始）
            u32 firstPc{}; // 首个 compare pc（此前为块前缀：判别值装入等）
            Vec<SwitchCase> cases;
            bool hasDefault{};
            u32 defaultPc{};
            u32 exitPc{}; // 出口（break 目标，也是区域末）
        };
        std::unordered_map<size_t, SwitchInfo> _switches; // 派发头块 → switch
        std::unordered_set<u32> _switchLabelPcs; // 参考派发的常量装入 pc
        std::unordered_set<u32> _switchCmpPcs; // 已认领的派发比较 pc
        std::unordered_set<u32> _switchPcs; // 已认领的全部派发段 pc

        // 从 startPc 起按指令线性扫描一段派发：[前缀...] (compare_test EQ,
        // JmpE→体)+ 终结于首个 Jmp（→下一派发头 / default / 出口）。块切分点
        // 不固定（JmpE 后、Jmp 前都可能成块边界），故跨块走指令流。
        // 返回匹配对、next 与派发段终点（Jmp 之后）。
        [[nodiscard]] bool scanDispatch(const u32 startPc, SwitchInfo &part, u32 &nextPc, u32 &endPc) const {
            const auto &code = _chunk.getInstVec();
            size_t pos = startPc;
            const auto startBlock = blockAtPc(startPc);
            if(startBlock >= _blocks.size())
                return false;
            const auto prefixEnd = _blocks[startBlock].end;
            // 前缀：首个 compare 之前允许直线语句，不允许跳转；前缀也不得跨入
            // 已认领的派发段 pc（体内块会误吸后续 switch 的比较对）
            while(pos < code.size() && code[pos]->opcode() != TacOpCode::CompareTest &&
                  code[pos]->opcode() != TacOpCode::CompareTestImm) {
                if(pos >= prefixEnd)
                    return false; // 不能吞入相邻块（例如 do-while 的体首）
                const auto op = code[pos]->opcode();
                if(op == TacOpCode::Jmp || op == TacOpCode::JmpNE || op == TacOpCode::JmpE)
                    return false;
                if(_switchPcs.contains(static_cast<u32>(pos)))
                    return false;
                ++pos;
            }
            if(pos >= code.size() || pos >= prefixEnd)
                return false;
            part.firstPc = INVALID;
            while(pos < code.size()) {
                const auto op = code[pos]->opcode();
                if(op == TacOpCode::Jmp) {
                    nextPc = code[pos]->getOp1().value<Label>().address();
                    if(nextPc <= pos || nextPc > code.size())
                        return false;
                    endPc = static_cast<u32>(pos + 1);
                    return part.cases.size() == 1 && part.cases.front().bodyPc == endPc;
                }
                // 常量装入属于下一个 case 标签的表达式，不产生独立语句。
                if(op == TacOpCode::Load || op == TacOpCode::LoadImm) {
                    ++pos;
                    continue;
                }
                if(op != TacOpCode::CompareTest && op != TacOpCode::CompareTestImm)
                    return false;
                if(pos + 1 >= code.size() || code[pos + 1]->opcode() != TacOpCode::JmpE)
                    return false;
                const auto bodyPc = code[pos + 1]->getOp1().value<Label>().address();
                SwitchMatch match{ .cmpPc = static_cast<u32>(pos), .isImm = false };
                if(op == TacOpCode::CompareTest) {
                    const auto *cmp = static_cast<const inter::CompareTest *>(code[pos].get());
                    if(cmp->comparison != TacOpCode::EQ)
                        return false; // 批 1 仅 == 派发
                    if(part.firstPc == INVALID) {
                        part.firstPc = static_cast<u32>(pos);
                        part.test = cmp->getOp1().value<Register>();
                    }
                    match.rhs = cmp->getOp2().value<Register>();
                } else {
                    const auto *cmp = static_cast<const inter::CompareTestImm *>(code[pos].get());
                    if(cmp->comparison != TacOpCode::EQ)
                        return false;
                    if(part.firstPc == INVALID) {
                        part.firstPc = static_cast<u32>(pos);
                        part.test = cmp->getOp1().value<Register>();
                    }
                    match.isImm = true;
                    match.imm = cmp->getOp2Val<Integer>();
                }
                match.test = code[pos]->getOp1().value<Register>();
                if(bodyPc <= pos || bodyPc >= code.size())
                    return false;
                if(!part.cases.empty() && part.cases.back().bodyPc == bodyPc) {
                    part.cases.back().matches.push_back(match); // 多匹配值同体
                } else {
                    part.cases.push_back(
                        SwitchCase{ .matches = { match }, .bodyPc = static_cast<u32>(bodyPc), .bodyEnd = 0 });
                }
                pos += 2;
            }
            return false;
        }

        // 仅跳过完整封闭于 case 区域的已识别循环；嵌套循环拥有自己的
        // break/continue 上下文，不能用外层 switch 的跳转规则校验它。
        [[nodiscard]] bool enclosedLoopJump(u32 pc, u32 begin, u32 end, u32 regionExit = INVALID) const {
            const auto &code = _chunk.getInstVec();
            const auto target = static_cast<u32>(code[pc]->getOp1().value<Label>().address());
            for(const auto &[header, loop] : _whileLoops) {
                const auto start = static_cast<u32>(_blocks[header].start);
                if(begin <= start && start <= pc && pc <= loop.latchJmpPc && loop.latchJmpPc < end &&
                   (loop.exitPc <= end || loop.exitPc == regionExit) && start <= target && target <= loop.exitPc)
                    return true;
            }
            for(const auto &[start, loop] : _doLoops)
                if(begin <= start && start <= pc && pc < loop.exitPc && loop.exitPc <= end && start <= target &&
                   target <= loop.exitPc)
                    return true;
            return false;
        }

        // 参考编译器：常量标签装入、CmpEQ、JmpNE→下一派发头；匹配则落入体。
        // 首批只认单匹配值、前向封闭体。共享判别值在发射时按版本源节点复核。
        void precomputeReferenceSwitches() {
            const auto &code = _chunk.getInstVec();
            Vec<bool> reachable(_blocks.size(), false);
            Vec<size_t> work;
            if(!_blocks.empty())
                work.push_back(0);
            while(!work.empty()) {
                const auto bi = work.back();
                work.pop_back();
                if(reachable[bi])
                    continue;
                reachable[bi] = true;
                for(const auto next : _blocks[bi].successors)
                    work.push_back(next);
            }
            // return 后参考导出仍保留跳转；整个不可达块的跳转都应折叠，
            // 包括未被 switch 认领的 default/尾部区域。
            bool acyclic = true;
            for(size_t bi = 0; bi < _blocks.size(); ++bi) {
                if(!reachable[bi]) {
                    for(u32 pc = static_cast<u32>(_blocks[bi].start); pc < _blocks[bi].end; ++pc) {
                        const auto op = code[pc]->opcode();
                        if(op == TacOpCode::Jmp || op == TacOpCode::JmpNE || op == TacOpCode::JmpE)
                            _foldedPcs.insert(pc);
                    }
                } else {
                    for(const auto next : _blocks[bi].successors)
                        if(next <= bi)
                            acyclic = false;
                }
            }
            auto constantLoad = [&](u32 pc) {
                return code[pc]->opcode() == TacOpCode::Load || code[pc]->opcode() == TacOpCode::LoadImm;
            };
            auto dispatch = [&](u32 start, SwitchMatch &match, u32 &body, u32 &next) {
                const auto bi = blockAtPc(start);
                if(bi >= _blocks.size() || _switchPcs.contains(start))
                    return false;
                const auto end = static_cast<u32>(_blocks[bi].end);
                if(end < start + 3 || code[end - 2]->opcode() != TacOpCode::CmpEQ ||
                   code[end - 1]->opcode() != TacOpCode::JmpNE || !constantLoad(end - 3))
                    return false;
                const auto cmp = end - 2;
                match = SwitchMatch{ .cmpPc = cmp,
                                     .test = code[cmp]->getOp1().value<Register>(),
                                     .rhs = code[cmp]->getOp2().value<Register>() };
                if(!(code[end - 3]->getOp1().value<Register>() == match.rhs))
                    return false;
                body = end;
                next = static_cast<u32>(code[end - 1]->getOp1().value<Label>().address());
                return next > body && next <= code.size();
            };
            for(size_t bi = 0; bi < _blocks.size(); ++bi) {
                if(!reachable[bi] || _switchPcs.contains(static_cast<u32>(_blocks[bi].start)))
                    continue;
                SwitchMatch match;
                u32 body{}, next{};
                const auto start = static_cast<u32>(_blocks[bi].start);
                if(!dispatch(start, match, body, next))
                    continue;
                SwitchInfo info{ .test = match.test, .headStart = start, .firstPc = match.cmpPc };
                std::unordered_set<u32> claimed;
                u32 dispatchStart = start;
                while(true) {
                    for(u32 p = dispatchStart; p < body; ++p)
                        claimed.insert(p);
                    info.cases.push_back(SwitchCase{ .matches = { match }, .bodyPc = body, .bodyEnd = next });
                    SwitchMatch following;
                    u32 followingBody{}, followingNext{};
                    if(!dispatch(next, following, followingBody, followingNext))
                        break;
                    bool constants = true;
                    for(u32 p = next; p < following.cmpPc; ++p)
                        constants &= constantLoad(p);
                    if(!constants || !(following.test == info.test))
                        break;
                    dispatchStart = next;
                    match = following;
                    body = followingBody;
                    next = followingNext;
                }
                // 外围循环上下文：嵌套 if 的 else 探测需识别 continue/锁存跳转
                LoopCtx enclosing;
                u32 enclosingStart = 0;
                for(const auto &[header, loop] : _whileLoops) {
                    const auto lstart = static_cast<u32>(_blocks[header].start);
                    if(lstart <= start && next <= loop.latchJmpPc && lstart >= enclosingStart) {
                        enclosing = LoopCtx{ .continuePc = loop.continuePc,
                                             .updatePc = loop.updatePc,
                                             .latchPc = loop.latchJmpPc };
                        enclosingStart = lstart;
                    }
                }
                for(const auto &[bodyStart, loop] : _doLoops) {
                    if(bodyStart <= start && next <= loop.condStart && bodyStart >= enclosingStart) {
                        enclosing = LoopCtx{ .continuePc = loop.condStart, .latchPc = loop.condJumpPc };
                        enclosingStart = bodyStart;
                    }
                }
                u32 exit = next;
                bool valid = true;
                std::unordered_set<u32> deadJumps;
                for(const auto &c : info.cases) {
                    for(u32 p = c.bodyPc; p < c.bodyEnd; ++p) {
                        const auto op = code[p]->opcode();
                        if(op != TacOpCode::Jmp)
                            continue;
                        if(!reachable[blockAtPc(p)]) {
                            deadJumps.insert(p);
                            continue;
                        }
                        const auto target = static_cast<u32>(code[p]->getOp1().value<Label>().address());
                        if(target > next && target != enclosing.continuePc && target != enclosing.latchPc) {
                            if(exit != next && exit != target && exit != enclosing.updatePc &&
                               target != enclosing.updatePc)
                                valid = false;
                            exit = exit == next ? target : std::min(exit, target);
                        }
                    }
                }
                // 单个等值分支仅在具有尾部 default 区域时尝试 switch 恢复；
                // 它与 if-else 语义等价，仍须通过下方的区域及寄存器合流检查。
                if(info.cases.size() == 1 && (exit == next || deadJumps.empty()))
                    continue;
                if(!valid || exit > code.size())
                    continue;
                info.hasDefault = exit > next;
                info.defaultPc = next;
                info.exitPc = exit;
                // 头块位于已认领 switch 的 case/default 体区域时，本链是嵌套
                // switch 或 else-if 链：出口越出所在体区域则不认领，退回外层
                // 嵌套 if 恢复；否则 emitSwitch 的区域越界守卫会硬拒，或嵌套
                // 发射与外层派发段重入。取包含本头的最小体边界。
                {
                    const auto headPc = static_cast<u32>(_blocks[bi].start);
                    u32 bound = INVALID;
                    for(const auto &[outerBi, outer] : _switches) {
                        u32 regionEnd = INVALID;
                        for(const auto &c : outer.cases)
                            if(c.bodyPc <= headPc && headPc < c.bodyEnd)
                                regionEnd = regionEnd == INVALID ? c.bodyEnd : std::min(regionEnd, c.bodyEnd);
                        if(outer.hasDefault && outer.defaultPc <= headPc && headPc < outer.exitPc)
                            regionEnd = regionEnd == INVALID ? outer.exitPc : std::min(regionEnd, outer.exitPc);
                        if(regionEnd != INVALID)
                            bound = bound == INVALID ? regionEnd : std::min(bound, regionEnd);
                    }
                    if(bound != INVALID && exit > bound)
                        continue;
                }
                const LoopCtx swCtx{ .breakPc = exit,
                                     .continuePc = enclosing.continuePc,
                                     .latchPc = enclosing.latchPc };
                // 无调试名字的寄存器不能按线性版本跨区域合流：某条路径写入
                // 的值不支配其它入口。函数内须恢复为具有共同初始化点的局部存储。
                struct RegionRegs {
                    std::unordered_set<u16> writes;
                    std::unordered_set<u16> liveIn;
                };
                Vec<RegionRegs> regions;
                auto addRegion = [&](u32 begin, u32 end) {
                    RegionRegs regs;
                    for(u32 p = begin; p < end; ++p) {
                        if(!reachable[blockAtPc(p)])
                            continue;
                        forEachUse(*code[p], [&](Register r) {
                            if(!regs.writes.contains(r.index()))
                                regs.liveIn.insert(r.index());
                        });
                        forEachDef(*code[p], [&](Register r, bool) { regs.writes.insert(r.index()); });
                    }
                    regions.push_back(std::move(regs));
                };
                // 体区域（case 或 default）内的活跳转校验：Jmp 目标须封闭在体
                // 区域（fall/出口例外）；活 JmpNE 须为可结构化的嵌套 if
                // （scanBranch 形态封闭），空 then 与越界合并点显式拒绝。
                // 派发链形态的 JmpNE（嵌套 switch / 同判别值 else-if 链）交由
                // 其自身块位的 precompute 认领，此处按保守子区域接纳。
                // fallPc 取 INVALID 表示无贯穿（default 体）。
                auto validateBody = [&](const u32 bodyBegin, const u32 bodyEnd, const u32 fallPc,
                                        const bool isDefault) {
                    for(u32 p = bodyBegin; p < bodyEnd; ++p) {
                        const auto op = code[p]->opcode();
                        if(op != TacOpCode::Jmp && op != TacOpCode::JmpNE && op != TacOpCode::JmpE)
                            continue;
                        // 参考导出保留 break/return 后的不可达跳转；CFG 证明
                        // 不可达的跳转折叠，不参与结构校验
                        if(!reachable[blockAtPc(p)]) {
                            deadJumps.insert(p);
                            continue;
                        }
                        if(enclosedLoopJump(p, bodyBegin, bodyEnd, exit))
                            continue;
                        if(op == TacOpCode::JmpE) {
                            fail("参考 switch 体内 JmpE 消费属 M5 后续批次");
                            return;
                        }
                        if(op == TacOpCode::Jmp) {
                            const auto target = static_cast<u32>(code[p]->getOp1().value<Label>().address());
                            if(target == enclosing.continuePc || target == enclosing.latchPc ||
                               target == enclosing.updatePc)
                                continue;
                            if(target <= p || (target >= bodyEnd && target != fallPc && target != exit)) {
                                if(isDefault)
                                    fail("参考 switch 的 default 控制流尚未恢复（M5）");
                                else
                                    valid = false;
                                return;
                            }
                            continue;
                        }
                        BranchShape shape;
                        if(!scanBranch(blockAtPc(p), swCtx, shape) || shape.mergePc > bodyEnd ||
                           (shape.hasElse && shape.elseEnd > bodyEnd)) {
                            fail("参考 switch 的嵌套分支无法结构化（空 then 或合并点越界，M5）");
                            return;
                        }
                        addRegion(shape.thenBegin, shape.thenContentEnd);
                        if(shape.hasElse)
                            addRegion(shape.elseBegin, shape.elseEnd);
                        addRegion(shape.mergePc, bodyEnd);
                    }
                };
                for(size_t i = 0; i < info.cases.size(); ++i) {
                    const auto &c = info.cases[i];
                    const auto fall = i + 1 < info.cases.size() ? info.cases[i + 1].bodyPc : next;
                    validateBody(c.bodyPc, c.bodyEnd, fall, false);
                }
                if(info.hasDefault)
                    validateBody(next, exit, INVALID, true);
                if(!_unsupported.empty())
                    return;
                if(!valid)
                    continue;
                for(size_t i = 0; i < info.cases.size(); ++i) {
                    const auto begin = i == 0 ? info.firstPc - 1 : info.cases[i - 1].bodyEnd;
                    addRegion(begin, info.cases[i].bodyEnd);
                }
                if(info.hasDefault)
                    addRegion(next, exit);
                const auto bodyRegions = regions.size();
                addRegion(exit, static_cast<u32>(code.size()));
                std::set<u16> mergeRegs;
                for(size_t i = 0; i < bodyRegions; ++i) {
                    for(size_t j = 0; j < regions.size(); ++j) {
                        if(i == j)
                            continue;
                        for(const auto reg : regions[i].writes) {
                            if(regions[j].liveIn.contains(reg)) {
                                mergeRegs.insert(reg);
                            }
                        }
                    }
                }
                // 区域重叠会产生假合流：在无回边 CFG 中，如果每个读取的
                // 线性前驱定义都支配该读取，则无需合成存储。含循环时保留原守卫，
                // 不能仅凭初始定义支配读点就忽略回边带来的其它版本。
                if(acyclic) {
                    std::erase_if(mergeRegs, [&](u16 reg) {
                        u32 lastDef = INVALID;
                        bool safe = true;
                        for(u32 pc = 0; pc < code.size() && safe; ++pc) {
                            const auto block = blockAtPc(pc);
                            if(!reachable[block])
                                continue;
                            forEachUse(*code[pc], [&](Register r) {
                                if(r.index() == reg && (lastDef == INVALID || !_dominators[block][blockAtPc(lastDef)]))
                                    safe = false;
                            });
                            forEachDef(*code[pc], [&](Register r, bool hidden) {
                                if(r.index() != reg)
                                    return;
                                if(hidden)
                                    safe = false; // 隐式 receiver 写入不参与本次消除证明。
                                else
                                    lastDef = pc;
                            });
                        }
                        return safe;
                    });
                }
                std::unordered_map<u16, u32> localStarts;
                for(const auto reg : mergeRegs) {
                    if(_switchLocalNames.contains(reg))
                        continue;
                    u32 initPc = INVALID;
                    for(u32 p = 0; p < info.firstPc; ++p) {
                        if(!reachable[blockAtPc(p)])
                            continue;
                        forEachDef(*code[p], [&](Register r, bool hidden) {
                            if(r.index() == reg && !hidden)
                                initPc = p;
                        });
                    }
                    if(!_functionLike || !_meta || initPc == INVALID ||
                       (blockAtPc(initPc) != bi && blockAtPc(initPc) != 0)) {
                        fail("参考 switch 存在不可局部物化的寄存器合流（M5）");
                        return;
                    }
                    localStarts[reg] = initPc + 1;
                }
                if(!mergeRegs.empty()) {
                    // 避开原单元参数、局部和 Atom 名字；后续临时物化沿用同一分配器。
                    for(u16 p = 0; p < static_cast<u16>(_meta->arity); ++p)
                        _usedNames.insert(fmt::format("a{}", p));
                    for(const auto &local : _meta->localVars)
                        _usedNames.insert(atomText(_rt, local.identifier));
                    for(const auto &inst : code) {
                        for(const auto &operand : { inst->getOp1(), inst->getOp2(), inst->getOp3() }) {
                            if(operand.type() == inter::Operand::Type::Atom)
                                _usedNames.insert(atomText(_rt, operand.value<Atom>()));
                        }
                    }
                    for(const auto reg : mergeRegs) {
                        if(localNameAt(Register{ reg }, info.firstPc).empty())
                            _switchLocalNames.emplace(reg, std::pair{ nextTempName(), localStarts.at(reg) });
                    }
                }
                for(const auto &c : info.cases) {
                    _switchCmpPcs.insert(c.matches.front().cmpPc);
                    _switchLabelPcs.insert(c.matches.front().cmpPc - 1);
                }
                _switchPcs.merge(claimed);
                std::erase_if(_whileLoops,
                              [&](const auto &entry) { return deadJumps.contains(entry.second.latchJmpPc); });
                std::erase_if(_doLoops,
                              [&](const auto &entry) { return deadJumps.contains(entry.second.condJumpPc + 1); });
                _foldedPcs.merge(deadJumps);
                _switches.emplace(bi, std::move(info));
            }
        }

        // 链归约：从派发头沿 [Jmp→next] 走到头（后续前缀仅接纳常量装入），按体尾 break 目标区分 default/出口，bodyEnd
        // 按下一派发头 封界；未被认领的 JmpE 如实拒绝
        void precomputeSwitches() {
            precomputeReferenceSwitches();
            auto &code = _chunk.getInstVec();
            // 后续 case 派发头属于同一条链，不能在反向归约时先独立认领。
            std::unordered_set<u32> continuations;
            for(const auto &block : _blocks) {
                SwitchInfo part, following;
                u32 next{}, end{}, followingNext{}, followingEnd{};
                if(!scanDispatch(static_cast<u32>(block.start), part, next, end))
                    continue;
                for(const auto &inside : _blocks)
                    if(block.start < inside.start && inside.start < end)
                        continuations.insert(static_cast<u32>(inside.start));
                if(!scanDispatch(next, following, followingNext, followingEnd))
                    continue;
                bool constants = true;
                for(u32 pc = next; pc < following.firstPc; ++pc)
                    constants &= code[pc]->opcode() == TacOpCode::Load || code[pc]->opcode() == TacOpCode::LoadImm;
                if(constants)
                    continuations.insert(next);
            }
            for(size_t remaining = _blocks.size(); remaining > 0; --remaining) {
                const auto bi = remaining - 1;
                if(continuations.contains(static_cast<u32>(_blocks[bi].start)))
                    continue;
                SwitchInfo head;
                u32 nextPc = 0;
                u32 endPc = 0;
                const u32 headStart = static_cast<u32>(_blocks[bi].start);
                if(!scanDispatch(headStart, head, nextPc, endPc))
                    continue;
                Vec<SwitchInfo> parts{ head };
                Vec<u32> headStarts{ headStart };
                std::unordered_set<u32> claimed;
                for(u32 p = headStart; p < endPc; ++p)
                    claimed.insert(p);
                while(true) {
                    SwitchInfo npart;
                    u32 nnext = 0;
                    u32 nend = 0;
                    if(!scanDispatch(nextPc, npart, nnext, nend))
                        break;
                    // 后续派发前缀仅接纳常量装入，禁止将默认体副作用移入标签。
                    bool constantPrefix = true;
                    for(u32 p = nextPc; p < npart.firstPc; ++p) {
                        const auto op = code[p]->opcode();
                        constantPrefix &= op == TacOpCode::Load || op == TacOpCode::LoadImm;
                    }
                    if(!constantPrefix)
                        break;
                    parts.push_back(npart);
                    headStarts.push_back(nextPc);
                    for(u32 p = nextPc; p < nend; ++p)
                        claimed.insert(p);
                    nextPc = nnext;
                }
                for(const auto &part : parts)
                    for(const auto &c : part.cases)
                        for(const auto &m : c.matches) {
                            _switchCmpPcs.insert(m.cmpPc);
                            _condPcs.insert(m.cmpPc);
                        }
                const u32 t = nextPc;
                for(u32 p = headStart; p < t; ++p) {
                    if(code[p]->opcode() == TacOpCode::JmpE && !claimed.contains(p) && !_condPcs.contains(p) &&
                       !_switchPcs.contains(p)) {
                        fail("嵌套 switch 派发链尚未支持（M5）");
                        return;
                    }
                }
                // continue 仍属于最内层外围循环，不参与 switch 出口推断。
                LoopCtx enclosing;
                u32 enclosingStart = 0;
                for(const auto &[header, loop] : _whileLoops) {
                    const auto start = static_cast<u32>(_blocks[header].start);
                    if(start <= headStart && t <= loop.latchJmpPc && start >= enclosingStart) {
                        enclosing = LoopCtx{ .continuePc = loop.continuePc,
                                             .updatePc = loop.updatePc,
                                             .latchPc = loop.latchJmpPc };
                        enclosingStart = start;
                    }
                }
                for(const auto &[start, loop] : _doLoops) {
                    if(start <= headStart && t <= loop.condStart && start >= enclosingStart) {
                        enclosing = LoopCtx{ .continuePc = loop.condStart, .latchPc = loop.condJumpPc };
                        enclosingStart = start;
                    }
                }
                // 体尾 break 目标（> t，体块内块尾 Jmp）一致 → 有 default，出口即它；
                // 全部 ≤ t → 无 default，出口 = t
                u32 exitTarget = INVALID;
                for(const auto &blk : _blocks) {
                    if(blk.start < headStart || blk.start >= t)
                        continue;
                    const size_t lp = blk.end - 1;
                    if(code[lp]->opcode() != TacOpCode::Jmp || _condPcs.contains(static_cast<u32>(lp)))
                        continue;
                    const auto target = code[lp]->getOp1().value<Label>().address();
                    if(target <= t || target == enclosing.continuePc || target == enclosing.latchPc)
                        continue;
                    if(exitTarget == INVALID)
                        exitTarget = target;
                    else if(exitTarget != target && exitTarget != enclosing.updatePc && target != enclosing.updatePc) {
                        fail(fmt::format("switch 体尾跳转目标不一致（pc {} → {} 与 {}）", lp, target, exitTarget));
                        return;
                    }
                    exitTarget = std::min(exitTarget, static_cast<u32>(target));
                }
                SwitchInfo chain;
                chain.headStart = headStart;
                chain.test = parts.front().test;
                chain.firstPc = parts.front().firstPc;
                for(size_t i = 0; i < parts.size(); ++i) {
                    const auto bodyEnd = i + 1 < parts.size() ? headStarts[i + 1] : t;
                    for(auto c : parts[i].cases) {
                        c.bodyEnd = bodyEnd;
                        chain.cases.push_back(c);
                    }
                }
                chain.hasDefault = exitTarget != INVALID;
                chain.defaultPc = chain.hasDefault ? t : 0;
                chain.exitPc = chain.hasDefault ? exitTarget : t;
                // 首批只接纳前向、封闭的 case 区域；不能把越界跳转当贯穿省略。
                if(chain.exitPc > code.size() || chain.exitPc <= chain.firstPc) {
                    fail("switch 出口越界（M5）");
                    return;
                }
                for(size_t i = 0; i < chain.cases.size(); ++i) {
                    const auto &c = chain.cases[i];
                    const auto fallPc = i + 1 < chain.cases.size() ? chain.cases[i + 1].bodyPc : t;
                    for(u32 p = c.bodyPc; p < c.bodyEnd; ++p) {
                        const auto op = code[p]->opcode();
                        if(op != TacOpCode::Jmp && op != TacOpCode::JmpNE && op != TacOpCode::JmpE)
                            continue;
                        if(enclosedLoopJump(p, c.bodyPc, c.bodyEnd, chain.exitPc))
                            continue;
                        const auto target = code[p]->getOp1().value<Label>().address();
                        if(target == enclosing.continuePc || target == enclosing.latchPc ||
                           target == enclosing.updatePc)
                            continue;
                        if(target <= p || (target >= c.bodyEnd && target != fallPc && target != chain.exitPc)) {
                            fail("switch 分支跳转超出封闭区域（M5）");
                            return;
                        }
                    }
                }
                _switches.emplace(bi, chain);
                _switchPcs.merge(claimed);
            }
            // 未被派发链认领的 JmpE：如实拒绝
            for(size_t pc = 0; pc < code.size(); ++pc) {
                if(code[pc]->opcode() != TacOpCode::JmpE)
                    continue;
                if(_switchPcs.contains(static_cast<u32>(pc)) ||
                   std::ranges::any_of(_doLoops, [&](const auto &entry) { return entry.second.condJumpPc == pc; }))
                    continue;
                fail("switch 派发链形态不可识别（JmpE 不在可恢复的派发头上，M5）");
                return;
            }
        }

        // 循环条件读取的寄存器若在体内重定义且绑定字面量节点（无名字），按初值
        // 渲染会丢迭代语义 → 拒绝（具名局部/全局名读取不受影响）
        void guardLoopCond(const size_t blockIdx, const u32 bodyEnd) {
            const auto &zone = _condZones[blockIdx];
            if(zone.firstPc == INVALID)
                return;
            auto &code = _chunk.getInstVec();
            for(size_t pc = zone.firstPc; pc < _blocks[blockIdx].end && _unsupported.empty(); ++pc) {
                const Instruction &inst = *code[pc];
                switch(inst.opcode()) {
                    case TacOpCode::CompareTest:
                    case TacOpCode::CompareTestImm:
                    case TacOpCode::CmpEQ:
                    case TacOpCode::CmpDEQ:
                    case TacOpCode::CmpLT:
                    case TacOpCode::CmpGT:
                    case TacOpCode::Test:
                    case TacOpCode::TestVoid:
                        break;
                    default:
                        continue;
                }
                forEachUse(inst, [&](const Register reg) {
                    const auto vRead = liveVersionAt(reg, static_cast<u32>(pc));
                    if(vRead == INVALID)
                        return;
                    const auto &def = _defs[reg.index()][vRead];
                    if(def.node == INVALID)
                        return;
                    // 条件块内的定义每轮都会重新执行；体内复用物理寄存器
                    // 不会改变下一轮比较读取的值，不属于循环携带依赖。
                    if(def.defPc >= _blocks[blockIdx].start)
                        return;
                    // 匿名拷贝链（LoadImm → DLocal）按源头判字面量，否则
                    // Copy 节点绕过下方的 kind 判定，渲染出 `0 < a0` 类文本
                    const auto kind = _nodes[resolveCopyNode(def.node)].kind;
                    if(kind != Expr::Kind::Int && kind != Expr::Kind::Real && kind != Expr::Kind::Str)
                        return;
                    for(const auto &v : _defs[reg.index()]) {
                        if(v.defPc > pc && v.defPc <= bodyEnd) {
                            fail("循环条件读取的寄存器在体内重定义且绑定字面量（循环携带变量渲染不安全）");
                            return;
                        }
                    }
                });
            }
        }

        [[nodiscard]] u32 liveVersionAt(const Register reg, const u32 pc) const {
            const auto index = reg.index();
            if(index >= _defs.size())
                return INVALID;
            const auto &versions = _defs[index];
            for(size_t v = versions.size(); v > 0; --v)
                if(versions[v - 1].defPc <= pc)
                    return static_cast<u32>(v - 1);
            return INVALID;
        }

        // 条件块的条件节点：取条件区内最靠后的捕获节点（比较 > Test > TestVoid）
        [[nodiscard]] u32 zoneNodeFor(const size_t blockIdx) const {
            const auto &zone = _condZones[blockIdx];
            if(zone.firstPc == INVALID)
                return INVALID;
            u32 node = INVALID;
            for(size_t pc = zone.firstPc; pc < _blocks[blockIdx].end; ++pc) {
                const auto it = _condNodeAtPc.find(static_cast<u32>(pc));
                if(it != _condNodeAtPc.end())
                    node = it->second;
            }
            return node;
        }

        struct BranchShape {
            bool hasElse{};
            u32 thenBegin{}, thenContentEnd{}; // 体内容 [thenBegin, thenContentEnd)
            u32 elseBegin{}, elseEnd{}; // [elseBegin, elseEnd)
            u32 mergePc{}; // 汇合 pc（无 else = JmpNE 目标）
            u32 thenJmpPc{ INVALID }; // then 体末尾的汇合 Jmp
        };

        // 头块分支形态扫描：then 体 [end, T)；体末块以 Jmp→M（M > T）结束则
        // 有 else 区 [T, M)。循环回边目标 < 块末不匹配；break/continue 的前向
        // 跳转目标（循环出口/锁存）也不是汇合点
        [[nodiscard]] bool scanBranch(const size_t blockIdx, const LoopCtx &ctx, BranchShape &shape) const {
            const auto &block = _blocks[blockIdx];
            auto &code = _chunk.getInstVec();
            const size_t lastPc = block.end - 1;
            if(code[lastPc]->opcode() != TacOpCode::JmpNE)
                return false;
            const auto t = code[lastPc]->getOp1().value<Label>().address();
            if(t < block.end || t > code.size())
                return false; // t == block.end = 空 then 体
            shape.thenBegin = static_cast<u32>(block.end);
            shape.hasElse = false;
            shape.mergePc = static_cast<u32>(t);
            shape.thenJmpPc = INVALID;
            shape.thenContentEnd = static_cast<u32>(t);
            shape.elseBegin = shape.elseEnd = 0;
            const size_t last = blockAtPc(static_cast<u32>(t - 1));
            if(last >= _blocks.size())
                return true;
            for(size_t bi = blockIdx + 1; bi <= last; ++bi) {
                const auto &b = _blocks[bi];
                if(b.start >= t)
                    break;
                const size_t bLast = b.end - 1;
                if(code[bLast]->opcode() != TacOpCode::Jmp)
                    continue;
                const auto m = code[bLast]->getOp1().value<Label>().address();
                if(m == ctx.breakPc || m == ctx.continuePc || m == ctx.latchPc || m == ctx.updatePc)
                    continue; // break/continue 跳转，非汇合
                if(m >= b.end && m > t) {
                    shape.hasElse = true;
                    shape.thenContentEnd = static_cast<u32>(bLast);
                    shape.elseBegin = static_cast<u32>(t);
                    shape.elseEnd = static_cast<u32>(m);
                    shape.mergePc = static_cast<u32>(m);
                    shape.thenJmpPc = static_cast<u32>(bLast);
                }
            }
            return true;
        }

        [[nodiscard]] u32 definedVersionAt(const Register reg, const u32 pc) const {
            const auto index = reg.index();
            if(index >= _defs.size())
                return INVALID;
            const auto &versions = _defs[index];
            for(size_t v = versions.size(); v > 0; --v)
                if(versions[v - 1].defPc == pc)
                    return static_cast<u32>(v - 1);
            return INVALID;
        }

        [[nodiscard]] bool isLeafForTernary(const u32 node) const {
            switch(_nodes[node].kind) {
                case Expr::Kind::Void:
                case Expr::Kind::Null:
                case Expr::Kind::Int:
                case Expr::Kind::Real:
                case Expr::Kind::Str:
                case Expr::Kind::RegExp:
                case Expr::Kind::LocalName:
                case Expr::Kind::ThisVal:
                case Expr::Kind::GlobalObj:
                    return true;
                default:
                    return false;
            }
        }

        // 三元折叠 v1：两分支各为「单条纯叶定义 + d_local 拷入同一 dst」。
        // 命名 dst 合并为单条赋值语句，匿名 dst 绑定汇合版本节点
        [[nodiscard]] bool tryFoldTernary(const size_t blockIdx, const BranchShape &shape) {
            if(!shape.hasElse)
                return false;
            auto &code = _chunk.getInstVec();
            const size_t thenValPc = shape.thenBegin;
            const size_t thenCopyPc = shape.thenBegin + 1;
            const size_t elseValPc = shape.elseBegin;
            const size_t elseCopyPc = shape.elseBegin + 1;
            // thenContentEnd 为半开区间（thenJmpPc = thenContentEnd）：拷贝在其前一位
            if(thenCopyPc + 1 != shape.thenContentEnd || elseCopyPc + 1 != shape.elseEnd)
                return false;
            // 拷贝指令在 thenCopyPc/elseCopyPc（值定义在其前一位）
            if(code[thenCopyPc]->opcode() != TacOpCode::DLocal || code[elseCopyPc]->opcode() != TacOpCode::DLocal)
                return false;
            const auto *thenCopy = static_cast<const inter::DLocal *>(code[thenCopyPc].get());
            const auto *elseCopy = static_cast<const inter::DLocal *>(code[elseCopyPc].get());
            if(!(thenCopy->getOp2().value<Register>() == elseCopy->getOp2().value<Register>()))
                return false;
            const auto dst = thenCopy->getOp2().value<Register>();
            const auto valueNodeOf = [&](const size_t valPc, const size_t copyPc, const Register src) -> u32 {
                if(valPc + 1 != copyPc)
                    return INVALID;
                const auto version = definedVersionAt(src, static_cast<u32>(valPc));
                if(version == INVALID)
                    return INVALID;
                const auto &def = _defs[src.index()][version];
                if(def.node == INVALID || def.usePcs.size() != 1 || def.usePcs.front() != copyPc)
                    return INVALID;
                if(!isLeafForTernary(def.node))
                    return INVALID;
                return def.node;
            };
            const u32 thenNode = valueNodeOf(thenValPc, thenCopyPc, thenCopy->getOp1().value<Register>());
            const u32 elseNode = valueNodeOf(elseValPc, elseCopyPc, elseCopy->getOp1().value<Register>());
            if(thenNode == INVALID || elseNode == INVALID)
                return false;
            const u32 condNode = zoneNodeFor(blockIdx);
            if(condNode == INVALID)
                return false;
            const auto ternary = newNode(Expr::Kind::Ternary);
            _nodes[ternary].a = condNode;
            _nodes[ternary].b = thenNode;
            _nodes[ternary].integer = static_cast<Integer>(elseNode);

            for(size_t pc = shape.thenBegin; pc <= shape.thenJmpPc; ++pc)
                _foldedPcs.insert(static_cast<u32>(pc));
            for(size_t pc = shape.elseBegin; pc < shape.elseEnd; ++pc)
                _foldedPcs.insert(static_cast<u32>(pc));

            // 命名 dst：两条 StoreLocal 合并为汇合处一条
            const auto name = localNameAt(dst, static_cast<u32>(thenCopyPc) + 1);
            if(!name.empty()) {
                bool declare = false;
                for(const auto &stmt : _stmts) {
                    if(stmt.pc == thenCopyPc && stmt.kind == Stmt::Kind::StoreLocal) {
                        declare = stmt.declare;
                        break;
                    }
                }
                Vec<Stmt> kept;
                kept.reserve(_stmts.size());
                for(auto &stmt : _stmts) {
                    if((stmt.pc == thenCopyPc || stmt.pc == elseCopyPc) && stmt.kind == Stmt::Kind::StoreLocal)
                        continue;
                    kept.push_back(std::move(stmt));
                }
                _stmts = std::move(kept);
                Stmt stmt{ .kind = Stmt::Kind::StoreLocal, .pc = shape.mergePc, .exprNode = ternary, .name = name };
                stmt.declare = declare;
                pushStmt(std::move(stmt));
            } else {
                // 匿名 dst：else 拷贝产生汇合时的活跃版本，节点替换为三元
                const auto version = definedVersionAt(dst, static_cast<u32>(elseCopyPc));
                if(version != INVALID)
                    _defs[dst.index()][version].node = ternary;
            }
            return true;
        }

        // 三元折叠（decideVersions 之前调用）：逐条件头尝试，成功则区域透明化
        void foldTernaries() {
            LoopCtx empty{};
            for(size_t bi = 0; bi < _blocks.size(); ++bi) {
                if(_condZones[bi].firstPc == INVALID || _whileLoops.contains(bi) || _switches.contains(bi) ||
                   _switchCmpPcs.contains(_condZones[bi].firstPc))
                    continue;
                BranchShape shape;
                if(!scanBranch(bi, empty, shape))
                    continue;
                if(tryFoldTernary(bi, shape)) {
                    _foldedHeaders.insert(bi);
                    _foldedHeaderMerge[bi] = shape.mergePc;
                }
            }
        }

        // ── 结构化发射（多块；语句按 pc 排序后由游标消费，区域递归带缩进）──
        void emitStructured(Vec<RecoveredStatement> &out) {
            // case 的词法局部不能供兄弟 case 使用；合成合流存储必须属于函数。
            // try 提升存储同理；参数槽以入口拷贝初始化（异常可先于体内重定义发生）
            std::set<std::string> syntheticNames;
            for(const auto &[reg, storage] : _switchLocalNames)
                syntheticNames.insert(storage.first);
            for(const auto &name : syntheticNames)
                out.push_back(RecoveredStatement{ .pc = 0, .text = fmt::format("var {};", name) });
            for(const auto &[reg, storage] : _promotedStorage) {
                const auto copy = _promotedParamCopies.find(reg);
                if(copy != _promotedParamCopies.end())
                    out.push_back(RecoveredStatement{
                        .pc = 0, .text = fmt::format("var {} = {};", storage.first, copy->second) });
                else
                    out.push_back(RecoveredStatement{ .pc = 0, .text = fmt::format("var {};", storage.first) });
            }
            (void)std::stable_sort(_stmts.begin(), _stmts.end(),
                                   [](const Stmt &a, const Stmt &b) { return a.pc < b.pc; });
            _emitCursor = 0;
            const auto count = static_cast<u32>(_chunk.getInstVec().size());
            emitRegion(0, count, LoopCtx{}, 0, out);
        }

        [[nodiscard]] static std::string indentText(const u32 level) { return std::string(level * 4, ' '); }

        // 单 pc 发射：物化临时定义（先于消费语句）+ 该 pc 的语句（可多条）。
        // 语句文本均为相对缩进（首行由此处补 pad，写入期再叠加体 pad）
        void emitAt(const u32 pc, const u32 indent, Vec<RecoveredStatement> &out) {
            const std::string pad = indentText(indent);
            for(size_t reg = 0; reg < _defs.size(); ++reg) {
                for(size_t v = 0; v < _defs[reg].size(); ++v) {
                    const auto &def = _defs[reg][v];
                    if(!def.materialized || def.defPc != pc)
                        continue;
                    out.push_back(RecoveredStatement{
                        .pc = pc,
                        .text = fmt::format("{}var {} = {};", pad, def.tempName, renderNode(def.node).text) });
                }
            }
            while(_emitCursor < _stmts.size() && _stmts[_emitCursor].pc == pc) {
                const auto &stmt = _stmts[_emitCursor];
                std::string text;
                if(!renderStmt(stmt, text, indent))
                    return;
                if(!text.empty())
                    out.push_back(RecoveredStatement{ .pc = pc, .text = pad + std::move(text), .tacEnd = stmt.tacEnd });
                ++_emitCursor;
            }
        }

        void emitRegion(const u32 begin, const u32 end, const LoopCtx &ctx, const u32 indent,
                        Vec<RecoveredStatement> &out) {
            u32 pc = begin;
            while(pc < end) {
                const auto bi = blockAtPc(pc);
                if(bi >= _blocks.size()) {
                    fail("区域跨越未知块边界");
                    return;
                }
                const auto &block = _blocks[bi];
                auto &code = _chunk.getInstVec();
                // 区域截断：块可能延伸到区域之外（如循环体区域截在锁存 Jmp 前）
                const u32 regionEnd = std::min<u32>(block.end, end);
                const u32 zoneFirst = _condZones[bi].firstPc;
                // do-while 体首（先于前缀发射：体首即块首，整块前缀属于体；
                // 消费式移除防体区域重入递归）
                const auto dl = _doLoops.find(pc);
                if(dl != _doLoops.end()) {
                    const DoLoop loop = dl->second;
                    _doLoops.erase(dl);
                    pc = emitDoWhile(loop, ctx, indent, out);
                    continue;
                }
                // try/catch 头（先于 switch/while/if：tryStart 可能同时是这些
                // 结构的头块起点，必须由 try 认领后再递归内层；消费式移除防
                // 体区域重入递归）。try{try{…} 同起点嵌套时先认领最外层
                // （tryEnd 最大），内层由递归在同一 pc 命中
                {
                    const auto range = _tryHeads.equal_range(pc);
                    if(range.first != range.second) {
                        auto best = range.first;
                        for(auto it = range.first; it != range.second; ++it)
                            if(it->second.tryEnd > best->second.tryEnd)
                                best = it;
                        const TryRegion region = best->second;
                        _tryHeads.erase(best);
                        pc = emitTry(region, ctx, indent, out);
                        continue;
                    }
                }
                // 头块可能含条件区之前的体语句（如循环体内 k = k + 1; if (...)）：
                // 先按封闭区域发射前缀语句，再进入头处理
                const bool atHeader = pc == block.start || pc == zoneFirst;
                // switch 派发头（M5）：消费块前缀与整条派发链，直达出口
                {
                    const auto sw = _switches.find(bi);
                    if(sw != _switches.end() && atHeader && regionEnd == block.end) {
                        if(sw->second.exitPc > end) {
                            fail("switch 出口超出当前结构化区域（M5）");
                            return;
                        }
                        pc = emitSwitch(sw->second, ctx, indent, out);
                        continue;
                    }
                }
                if(atHeader && zoneFirst != INVALID && zoneFirst > pc && regionEnd == block.end) {
                    // 按 pc 逐个发射：物化临时定义可能落在无语句 tile 的 pc 上
                    for(u32 p = pc; p < zoneFirst; ++p) {
                        if(!_foldedPcs.contains(p))
                            emitAt(p, indent, out);
                    }
                    pc = zoneFirst;
                }
                // while 头（循环识别优先于 if）
                const auto wl = _whileLoops.find(bi);
                if(atHeader && regionEnd == block.end && wl != _whileLoops.end()) {
                    pc = emitWhile(bi, wl->second, ctx, indent, out);
                    if(pc > end) {
                        if(pc != ctx.breakPc) {
                            fail("循环出口超出当前结构化区域");
                            return;
                        }
                        out.push_back(
                            RecoveredStatement{ .pc = end - 1, .text = fmt::format("{}break;", indentText(indent)) });
                        pc = end;
                    }
                    continue;
                }
                // 已折叠三元头：区域透明，直达汇合
                if(atHeader && regionEnd == block.end && _foldedHeaders.contains(bi)) {
                    pc = _foldedHeaderMerge[bi];
                    continue;
                }
                // if 头
                if(atHeader && regionEnd == block.end && zoneFirst != INVALID &&
                   code[block.end - 1]->opcode() == TacOpCode::JmpNE) {
                    pc = emitIf(bi, ctx, indent, out);
                    continue;
                }
                // 直线块：先发语句，再判块尾结构跳转（仅整块在区域内时判）
                u32 stmtEnd = regionEnd;
                const size_t lastPc = block.end - 1;
                u32 jumpTarget = INVALID;
                if(regionEnd == block.end && !_foldedPcs.contains(static_cast<u32>(lastPc)) &&
                   (code[lastPc]->opcode() == TacOpCode::Jmp || code[lastPc]->opcode() == TacOpCode::JmpNE)) {
                    jumpTarget = static_cast<u32>(code[lastPc]->getOp1().value<Label>().address());
                    if(_condPcs.contains(static_cast<u32>(lastPc)))
                        stmtEnd = static_cast<u32>(lastPc); // 条件区：JmpNE 已被条件捕获
                }
                // 按 pc 逐个发射（物化临时定义与语句 tile 统一走 emitAt）
                for(u32 p = pc; p < stmtEnd; ++p) {
                    if(!_foldedPcs.contains(p))
                        emitAt(p, indent, out);
                }
                if(jumpTarget != INVALID && !_condPcs.contains(static_cast<u32>(lastPc))) {
                    if(jumpTarget == ctx.breakPc) {
                        out.push_back(RecoveredStatement{ .pc = static_cast<u32>(lastPc),
                                                          .text = fmt::format("{}break;", indentText(indent)) });
                    } else if(jumpTarget == ctx.continuePc || jumpTarget == ctx.latchPc || jumpTarget == ctx.updatePc) {
                        if(jumpTarget == ctx.updatePc) {
                            const auto savedCursor = _emitCursor;
                            _emitCursor = static_cast<size_t>(
                                std::lower_bound(_stmts.begin(), _stmts.end(), ctx.updatePc,
                                                 [](const Stmt &stmt, u32 at) { return stmt.pc < at; }) -
                                _stmts.begin());
                            emitRegion(ctx.updatePc, ctx.latchPc, LoopCtx{}, indent, out);
                            _emitCursor = savedCursor;
                        }
                        out.push_back(RecoveredStatement{ .pc = static_cast<u32>(lastPc),
                                                          .text = fmt::format("{}continue;", indentText(indent)) });
                    } else if(jumpTarget < end && jumpTarget != pc) {
                        fail(fmt::format("pc {} 的跳转目标 {} 不属于可结构化形态", lastPc, jumpTarget));
                    }
                    // 跳到区域末尾（≥ end）等价贯穿，省略
                }
                pc = regionEnd;
            }
        }

        [[nodiscard]] u32 emitWhile(const size_t blockIdx, const WhileLoop &loop, const LoopCtx &ctx, const u32 indent,
                                    Vec<RecoveredStatement> &out) {
            const auto &block = _blocks[blockIdx];
            std::string condText = "true";
            const u32 condNode = loop.hasCond ? zoneNodeFor(blockIdx) : INVALID;
            if(loop.hasCond) {
                if(condNode == INVALID) {
                    fail("循环条件节点缺失");
                    return loop.exitPc;
                }
                // Test 绑定非零常量 → while (true)；绑定零 → while (false)
                if(_nodes[condNode].kind == Expr::Kind::Int) {
                    condText = _nodes[condNode].integer != 0 ? "true" : "false";
                } else {
                    guardLoopCond(blockIdx, loop.latchJmpPc);
                    if(!_unsupported.empty())
                        return loop.exitPc;
                    condText = renderNode(condNode).text;
                }
            }
            const u32 zonePc =
                _condZones[blockIdx].firstPc == INVALID ? static_cast<u32>(block.start) : _condZones[blockIdx].firstPc;
            out.push_back(RecoveredStatement{ .pc = zonePc,
                                              .text = fmt::format("{}while ({}) {{", indentText(indent), condText) });
            LoopCtx inner{ .breakPc = loop.exitPc,
                           .continuePc = loop.continuePc,
                           .updatePc = loop.updatePc,
                           .latchPc = loop.latchJmpPc };
            emitRegion(block.end, loop.latchJmpPc, inner, indent + 1, out);
            out.push_back(RecoveredStatement{ .pc = loop.latchJmpPc, .text = fmt::format("{}}}", indentText(indent)) });
            return loop.exitPc == INVALID ? static_cast<u32>(loop.latchJmpPc + 1) : loop.exitPc;
        }

        [[nodiscard]] u32 emitDoWhile(const DoLoop &loop, const LoopCtx &ctx, const u32 indent,
                                      Vec<RecoveredStatement> &out) {
            const size_t latchBlock = blockAtPc(loop.condJumpPc);
            const u32 condNode = latchBlock < _blocks.size() ? zoneNodeFor(latchBlock) : INVALID;
            if(condNode == INVALID) {
                fail("do-while 条件节点缺失");
                return loop.exitPc;
            }
            out.push_back(
                RecoveredStatement{ .pc = loop.bodyStart, .text = fmt::format("{}do {{", indentText(indent)) });
            LoopCtx inner{ .breakPc = loop.exitPc, .continuePc = loop.condStart, .latchPc = loop.condJumpPc };
            emitRegion(loop.bodyStart, loop.condStart, inner, indent + 1, out);
            out.push_back(RecoveredStatement{
                .pc = loop.condJumpPc,
                .text = fmt::format("{}}} while ({});", indentText(indent), renderNode(condNode).text) });
            return loop.exitPc;
        }

        [[nodiscard]] u32 emitIf(const size_t blockIdx, const LoopCtx &ctx, const u32 indent,
                                 Vec<RecoveredStatement> &out) {
            const auto &block = _blocks[blockIdx];
            const u32 condNode = zoneNodeFor(blockIdx);
            if(condNode == INVALID) {
                fail("分支条件节点缺失");
                return block.end;
            }
            if(_foldedHeaders.contains(blockIdx))
                return _foldedHeaderMerge[blockIdx]; // 三元已折叠：区域透明，直达汇合
            BranchShape shape;
            if(!scanBranch(blockIdx, ctx, shape)) {
                fail("分支形态不可识别");
                return block.end;
            }
            if(tryFoldTernary(blockIdx, shape)) {
                _foldedHeaders.insert(blockIdx);
                _foldedHeaderMerge[blockIdx] = shape.mergePc;
                return shape.mergePc;
            }
            const std::string pad = indentText(indent);
            out.push_back(RecoveredStatement{ .pc = _condZones[blockIdx].firstPc,
                                              .text = fmt::format("{}if ({}) {{", pad, renderNode(condNode).text) });
            emitRegion(shape.thenBegin, shape.thenContentEnd, ctx, indent + 1, out);
            if(shape.hasElse) {
                out.push_back(
                    RecoveredStatement{ .pc = shape.elseBegin - 1, .text = fmt::format("{}}} else {{", pad) });
                emitRegion(shape.elseBegin, shape.elseEnd, ctx, indent + 1, out);
                out.push_back(RecoveredStatement{ .pc = shape.mergePc - 1, .text = fmt::format("{}}}", pad) });
            } else {
                out.push_back(RecoveredStatement{ .pc = shape.mergePc - 1, .text = fmt::format("{}}}", pad) });
            }
            return shape.mergePc;
        }

        // try/catch 发射（M5）：体区与 catch 体各自作为封闭区域递归发射，继承
        // 外层 LoopCtx（体内 break/continue 照常分类）。出口跳转目标 = joinPc ≥
        // 体区末尾，被 emitRegion 按贯穿省略；catch 变量无绑定时输出裸 catch。
        // 结构行不对应任何 TAC 指令（异常表为旁表），标记 synthetic
        [[nodiscard]] u32 emitTry(const TryRegion &region, const LoopCtx &ctx, const u32 indent,
                                  Vec<RecoveredStatement> &out) {
            const std::string pad = indentText(indent);
            out.push_back(RecoveredStatement{ .pc = region.tryStart, .text = pad + "try {", .synthetic = true });
            emitRegion(region.tryStart, region.tryEnd, ctx, indent + 1, out);
            if(!_unsupported.empty())
                return region.joinPc;
            if(region.exValueReg == vm::ThrowHandler::exValueRegNone)
                out.push_back(RecoveredStatement{ .pc = region.tryEnd, .text = pad + "} catch {", .synthetic = true });
            else
                out.push_back(RecoveredStatement{ .pc = region.tryEnd,
                                                  .text = fmt::format("{}}} catch ({}) {{", pad, region.catchName),
                                                  .synthetic = true });
            emitRegion(region.tryEnd, region.joinPc, ctx, indent + 1, out);
            if(!_unsupported.empty())
                return region.joinPc;
            out.push_back(RecoveredStatement{ .pc = region.joinPc > region.tryEnd ? region.joinPc - 1 : region.tryEnd,
                                              .text = pad + "}",
                                              .synthetic = true });
            return region.joinPc;
        }

        // switch 派发链发射（M5）：块前缀（判别值装入/物化临时）→ switch 头 →
        // 逐 case 渲染匹配值与体（break 经 LoopCtx.breakPc 命中出口；贯穿 Jmp
        // 目标在体区域之外被 emitRegion 静默省略）→ default → 收尾
        [[nodiscard]] u32 emitSwitch(const SwitchInfo &info, const LoopCtx &ctx, const u32 indent,
                                     Vec<RecoveredStatement> &out) {
            const size_t headBi = blockAtPc(info.firstPc);
            if(headBi >= _blocks.size()) {
                fail("switch 派发头块缺失");
                return info.exitPc;
            }
            // 块前缀：判别值装入与物化临时定义（先于 switch 文本）
            for(u32 p = info.headStart; p < info.firstPc; ++p) {
                if(!_foldedPcs.contains(p))
                    emitAt(p, indent, out);
            }
            std::string testText;
            const auto version = liveVersionAt(info.test, info.firstPc);
            if(version == INVALID) {
                fail("switch 判别值节点缺失");
                return info.exitPc;
            }
            testText = renderNode(_defs[info.test.index()][version].node).text;
            const std::string pad = indentText(indent);
            const std::string pad1 = indentText(indent + 1);
            out.push_back(
                RecoveredStatement{ .pc = info.firstPc, .text = fmt::format("{}switch ({}) {{", pad, testText) });
            LoopCtx swCtx{
                .breakPc = info.exitPc, .continuePc = ctx.continuePc, .updatePc = ctx.updatePc, .latchPc = ctx.latchPc
            };
            for(const auto &c : info.cases) {
                for(const auto &m : c.matches) {
                    const auto mv = liveVersionAt(m.test, m.cmpPc);
                    if(mv == INVALID ||
                       resolveCopyNode(_defs[m.test.index()][mv].node) !=
                           resolveCopyNode(_defs[info.test.index()][version].node)) {
                        fail("switch 判别值版本不一致（M5）");
                        return info.exitPc;
                    }
                    std::string valText;
                    if(m.isImm) {
                        valText = fmt::format("{}", m.imm);
                    } else {
                        const auto v = liveVersionAt(m.rhs, m.cmpPc);
                        if(v == INVALID) {
                            fail("case 匹配值节点缺失");
                            return info.exitPc;
                        }
                        valText = renderNode(_defs[m.rhs.index()][v].node).text;
                    }
                    out.push_back(
                        RecoveredStatement{ .pc = m.cmpPc, .text = fmt::format("{}case {}:", pad1, valText) });
                }
                emitRegion(c.bodyPc, c.bodyEnd, swCtx, indent + 2, out);
            }
            if(_unsupported.empty()) {
                if(info.hasDefault) {
                    out.push_back(RecoveredStatement{ .pc = info.defaultPc, .text = fmt::format("{}default:", pad1) });
                    emitRegion(info.defaultPc, info.exitPc, swCtx, indent + 2, out);
                }
                out.push_back(RecoveredStatement{ .pc = info.exitPc - 1, .text = fmt::format("{}}}", pad) });
            }
            return info.exitPc;
        }

        // 条件区的比较定义：dst 仅供块条件使用（融合后通常死寄存器），不转
        // var 声明语句，节点直接作为块条件捕获
        void noteCondCompare(const Register reg) {
            if(_stateMachine)
                noteNamedDef(reg);
            if(!_condPcs.contains(_instPc)) {
                noteNamedDef(reg);
                return;
            }
            const auto version = definedVersion(reg);
            if(version != INVALID)
                _condNodeAtPc[_instPc] = _defs[reg.index()][version].node;
        }

        void fail(const std::string &reason) {
            if(_unsupported.empty())
                _unsupported = reason;
        }

        Runtime &_rt;
        vm::Chunk &_chunk;
        const FuncMeta *_meta;
        bool _functionLike;
        DeclHost *_host;
        bool _stateMachine{};
        Vec<std::string> _machineNames;
        u32 _instPc{};
        std::string _unsupported;
        Vec<Expr> _nodes;
        Vec<Vec<DefVersion>> _defs;
        Vec<u32> _current;
        Vec<Stmt> _stmts;
        std::unordered_set<u32> _stmtPcs;
        Vec<bool> _impureOrStore;
        std::unordered_map<u32, u32> _redirect; // 折叠使用点 pc → 构造结果版本槽
        struct PendingArg {
            u32 pc;
            u32 node;
        };
        Vec<PendingArg> _pending; // 转译路径 Push 序列
        std::set<std::pair<u16, std::string>> _declaredLocals;
        std::set<std::string> _declaredGlobals;
        std::unordered_set<std::string> _usedNames;
        std::unordered_map<u16, std::pair<std::string, u32>> _switchLocalNames; // 参考 switch 合流寄存器的函数局部存储
        std::unordered_map<u16, std::pair<std::string, u32>>
            _promotedStorage; // catch 数据流提升的寄存器存储（reg → {名, 起始 pc}）
        std::unordered_map<u16, std::string> _promotedParamCopies; // 提升参数槽 → 入口拷贝源名
        u32 _tempCounter{};
        std::unordered_map<u32, DeclSpec> _declSpecs; // Load pc → 声明安装规格（M4）

        // ── M3 结构化状态 ──
        Vec<inter::BasicBlock> _blocks;
        Vec<Vec<bool>> _dominators;
        Vec<CondZone> _condZones;
        std::unordered_set<u32> _condPcs;
        std::unordered_map<u32, u32> _condNodeAtPc; // 条件区 pc → 捕获的条件节点
        std::unordered_map<size_t, WhileLoop> _whileLoops; // 头块 → 循环
        std::unordered_map<u32, DoLoop> _doLoops; // 体首 pc → do-while
        std::unordered_multimap<u32, TryRegion> _tryHeads; // tryStart → try/catch 区域（同起点嵌套并存）
        std::unordered_set<u32> _foldedPcs; // 三元折叠透明的 pc
        std::unordered_set<size_t> _foldedHeaders; // 已折叠三元条件的头块
        std::unordered_map<size_t, u32> _foldedHeaderMerge; // 头块 → 汇合 pc
        size_t _emitCursor{};
    };

    ExprRecovery recoverUnitBody(Runtime &rt, vm::Chunk &chunk, const FuncMeta *meta, const bool functionLike,
                                 DeclHost *host) {
        auto recovered = ExprBuilder{ rt, chunk, meta, functionLike, host }.run();
        if(!recovered.unsupported.empty() && functionLike) {
            auto fallback = ExprBuilder{ rt, chunk, meta, functionLike, host, true }.run();
            if(fallback.unsupported.empty())
                return fallback;
            recovered.unsupported += "；CFG 降级：" + fallback.unsupported;
        }
        return recovered;
    }
} // namespace cial::decompile
