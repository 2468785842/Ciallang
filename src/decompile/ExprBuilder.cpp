/*
 * Copyright (c) 2026/11/1
 *
 * 反编译器 M2 表达式恢复实现（doc/bytecode/decompiler-plan.md §5.2/§5.7）。
 * 见 ExprBuilder.hpp。
 *
 * 管线（单块直线 TAC）：
 *   1. 预检拒绝语句结构（跳转/with/try/条件上下文 → M3/M5）与声明装配
 *      （Load 元对象常量 → M4）；
 *   2. Pass A：寄存器版本链（LIFO 复用使同一寄存器多版本；参数寄存器预播种）
 *      + 使用点收集；
 *   3. Pass B：按后优化形态表建表达式节点、登记语句 tile（BinaryImm/
 *      CompareTestImm 逆映射、字面量 SetIndex 折叠、具名局部定义转 var 语句、
 *      Push+Call 归并）；
 *   4. 死代码：无使用的纯定义丢弃，无使用且有副作用的定义降为表达式语句；
 *   5. 物化决策（按 defPc 降序，消费方先决）：单使用且 def→落点干净区间才内联；
 *      干净区间 = 之间无存储/调用/getter，除非中间定义同落点内联（同树序不变）
 *      或本身是恒可内联的字面量；多使用在函数体内物化为临时变量，顶层纯读复制、
 *      否则拒绝（避免复制出 getter/调用次数变化或物化可观察全局状态）；
 *   6. 按 pc 序发射语句，按 TJS2 优先级加括号。
 */

#include "ExprBuilder.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <ranges>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include <fmt/format.h>

#include "gen/Instruction.hpp"
#include "runtime/Runtime.hpp"
#include "types/Array.hpp"
#include "types/Dictionary.hpp"

namespace cial::decompile {
    namespace {
        using inter::Instruction;
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
                ArrayLit,
                DictLit,
                RegExp,
                LocalName, // 具名局部（含参数）或全局名
                ThisVal,
                GlobalObj,
                Member, // obj.name / obj["name"] / obj[expr]
                Index, // obj[expr]
                Call, // callee(args)
                Transmit, // f(...)
                Unary, // 前缀（- ! ~ + typeof int real string octet # $ isvalid & * delete）
                IncDec, // Inc/Dec 单指令（仅语句形态直接回写时可恢复）
                Binary,
                Copy, // DLocal/GLocal 匿名拷贝（透明引用源版本）
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
                DeleteMember, // delete OBJ.M / OBJ[expr]（dst 无使用）
                ExprStmt, // EXPR;（死副作用定义降级：调用/getter/转换/delete）
                Return, // return SRC / return（Void 且单元末尾时省略）
                ThrowStmt,
                Invalidate,
                DebuggerStmt,
            };
            Kind kind{};
            u32 pc{};
            u32 obj{ INVALID }, idx{ INVALID }, src{ INVALID }; // 寄存器槽
            u32 exprNode{ INVALID }; // StoreLocal 直定 / ExprStmt 的表达式节点
            Atom atom{};
            std::string name; // StoreLocal 变量名
            bool raw{};
            bool declare{}; // 输出 var 前缀（首次声明）
        };

        struct DefVersion {
            u32 node{ INVALID };
            u32 defPc{};
            Vec<u32> usePcs;
            u32 landing{ INVALID }; // 单使用版本的文本落点（消费语句/定义的 pc）
            bool hidden{}; // VM 内部写入（GMethodC receiver）：不产节点
            bool materialized{}; // 物化为临时变量（函数体）
            bool duplicated{}; // 顶层纯读复制
            std::string tempName;
        };
    } // namespace

    class ExprBuilder {
    public:
        ExprBuilder(Runtime &rt, vm::Chunk &chunk, const FuncMeta *meta, const bool functionLike) :
            _rt(rt), _chunk(chunk), _meta(meta), _functionLike(functionLike) {}

        ExprRecovery run() {
            ExprRecovery recovery;
            if(!precheck(recovery.unsupported))
                return recovery;

            scanVersions();
            buildNodesAndStmts();
            if(finishUnsupported(recovery))
                return recovery;

            collectDeadStatements();
            decideVersions();
            assignTempNames();
            if(finishUnsupported(recovery))
                return recovery;

            emitStatements(recovery.statements);
            finishUnsupported(recovery);
            return recovery;
        }

    private:
        bool finishUnsupported(ExprRecovery &recovery) const {
            if(_unsupported.empty())
                return false;
            recovery.unsupported = _unsupported;
            recovery.statements.clear();
            return true;
        }

        // ── 预检：语句结构（M3/M5）与声明装配（M4）如实拒绝 ──
        bool precheck(std::string &reason) {
            if(!_chunk.getThrowHandlers().empty()) {
                reason = "try/catch 结构化属 M5（throw handler 表非空）";
                return false;
            }
            auto &instructions = _chunk.getInstVec();
            for(size_t pc = 0; pc < instructions.size(); ++pc) {
                const auto op = instructions[pc]->opcode();
                switch(op) {
                    case TacOpCode::Jmp:
                    case TacOpCode::JmpE:
                    case TacOpCode::JmpNE:
                        reason = "控制流语句结构化属 M3（跳转存在）";
                        return false;
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
                    case TacOpCode::Test:
                    case TacOpCode::CmpEQ:
                    case TacOpCode::CmpDEQ:
                    case TacOpCode::CmpLT:
                    case TacOpCode::CmpGT:
                        reason = "_zf 条件上下文属 M3（直线代码不应出现）";
                        return false;
                    case TacOpCode::TestVoid:
                        reason = "默认参数守卫属 M3（条件上下文）";
                        return false;
                    case TacOpCode::Super:
                        reason = "super 关键字恢复属 M4（类壳）";
                        return false;
                    default:
                        break;
                }
                // 声明装配（函数/类/property 常量进寄存器）属 M4
                if(op == TacOpCode::Load) {
                    const auto &constant = _chunk.getConstants()[instructions[pc]->getOp2().value<ConstIdx>().index()];
                    if(constant.type() == ConstantType::FuncMeta || constant.type() == ConstantType::ClassMeta ||
                       constant.type() == ConstantType::PropMeta) {
                        reason = "声明装配（context 树 → 壳）属 M4";
                        return false;
                    }
                }
            }
            return true;
        }

        // ── Pass A：寄存器版本链 + 使用点收集 ──
        void scanVersions() {
            _current.assign(std::max<size_t>(_chunk.getRegCount() + 8, 64), INVALID);
            // 参数寄存器 0..arity-1 由调用方装入（帧约定），预播种具名版本
            if(_meta) {
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
                const Instruction &inst = *instructions[pc];
                // 先使用后定义：Inc/Dec、ChkIns 的操作数寄存器读旧写新
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
            if(_meta)
                for(u16 i = 0; i < static_cast<u16>(_meta->arity); ++i)
                    _current[i] = 0;
            for(size_t pc = 0; pc < instructions.size(); ++pc) {
                _instPc = static_cast<u32>(pc);
                const Instruction &inst = *instructions[pc];
                _impureOrStore[pc] = isImpureOrStore(inst);
                buildInstruction(inst);
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
                case TacOpCode::Debugger:
                    pushStmt(Stmt{ .kind = Stmt::Kind::DebuggerStmt, .pc = _instPc });
                    break;
                case TacOpCode::Load:
                    defineNode(inst.getOp1().value<Register>(), loadConstNode(inst.getOp2().value<ConstIdx>()));
                    noteNamedDef(inst.getOp1().value<Register>());
                    break;
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
                case TacOpCode::CompareTest:
                    defineNode(inst.getOp3().value<Register>(),
                               makeBinary(static_cast<const inter::CompareTest &>(inst).comparison,
                                          operandNode(inst.getOp1().value<Register>()),
                                          operandNode(inst.getOp2().value<Register>())));
                    noteNamedDef(inst.getOp3().value<Register>());
                    break;
                case TacOpCode::CompareTestImm: {
                    const auto &cmp = static_cast<const inter::CompareTestImm &>(inst);
                    const auto imm = makeLeaf(Expr::Kind::Int, cmp.getOp2().value<Integer>());
                    defineNode(cmp.getOp3().value<Register>(),
                               makeBinary(cmp.comparison, operandNode(cmp.getOp1().value<Register>()), imm));
                    noteNamedDef(cmp.getOp3().value<Register>());
                    break;
                }
                case TacOpCode::GPropC:
                case TacOpCode::GMethodC:
                    defineNode(
                        inst.getOp3().value<Register>(),
                        memberNode(operandNode(inst.getOp1().value<Register>()), inst.getOp2().value<Atom>(), INVALID));
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
                    break;
                }
                case TacOpCode::NewDict: {
                    const auto node = newNode(Expr::Kind::DictLit);
                    defineNode(inst.getOp1().value<Register>(), node);
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
                    // 同名目标"的语句形态可恢复（IRGen 不发射；来自 TJS2 转译）
                    const auto reg = inst.getOp1().value<Register>();
                    const auto node = newNode(Expr::Kind::IncDec);
                    _nodes[node].text = inst.opcode() == TacOpCode::Inc ? "++" : "--";
                    _nodes[node].a = operandNode(reg);
                    defineNode(reg, node);
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
                    fail("预检未覆盖的指令");
                    break;
            }
        }

        // 具名局部寄存器的直接定义 = var 声明（IRGen 零拷贝：表达式 dst 直接成为
        // 变量槽）。转为 StoreLocal 语句，版本改绑为变量名
        void noteNamedDef(const Register reg) {
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
            if(_declaredLocals.contains({ regIndex, name }))
                return false;
            _declaredLocals.emplace(regIndex, name);
            return true;
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
                // 字典：键须为编译期字符串常量；动态键恢复属后续批次
                if(idxNode == INVALID || _nodes[idxNode].kind != Expr::Kind::Str) {
                    fail("字典字面量键非常量（动态键恢复属后续批次）");
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
                case ConstantType::Octet:
                case ConstantType::FuncMeta:
                case ConstantType::ClassMeta:
                case ConstantType::PropMeta:
                    fail("该常量类型无可打印字面量（声明装配属 M4）");
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

        // pc 处的局部名：取 reg 匹配且 startPC ≤ pc 的最新声明（寄存器跨作用域
        // 复用时旧名已被新名覆盖；endPC 未由 IRGen 填写，恒 0 不参与判断）
        [[nodiscard]] std::string localNameAt(const Register reg, const u32 pc) const {
            if(!_meta)
                return {};
            std::string best;
            u32 bestStart = 0;
            bool found = false;
            for(const auto &local : _meta->localVars) {
                if(local.reg != reg || local.startPC.address() > pc)
                    continue;
                if(!found || local.startPC.address() >= bestStart) {
                    bestStart = static_cast<u32>(local.startPC.address());
                    best = atomText(_rt, local.identifier);
                    found = true;
                }
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
                    if(_stmtPcs.contains(def.defPc))
                        continue; // 已是语句（StoreLocal 直定等）
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
        // （同一表达式树，求值序不变）或本身恒可内联
        [[nodiscard]] bool spanClean(const DefVersion &def, const std::unordered_map<u32, u32> &defSlotAtPc) const {
            if(alwaysInlinable(def.node))
                return true;
            if(def.landing == INVALID || def.landing <= def.defPc)
                return true; // 自引用/同 pc
            for(u32 pc = def.defPc + 1; pc < def.landing; ++pc) {
                if(!_impureOrStore[pc])
                    continue;
                const auto it = defSlotAtPc.find(pc);
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
                case Expr::Kind::RegExp:
                    return { fmt::format("/{}/", expr.text), Prec::Postfix };
                case Expr::Kind::LocalName:
                    return { expr.text, Prec::Primary };
                case Expr::Kind::ThisVal:
                    return { "this", Prec::Primary };
                case Expr::Kind::GlobalObj:
                    return { "global", Prec::Primary };
                case Expr::Kind::Copy:
                    return { expr.srcVersion == INVALID ? std::string{ "<copy>" } : renderVersion(expr.srcVersion),
                             Prec::Primary };
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
                std::string text;
                if(!renderStmt(*it->second, text))
                    return;
                if(!text.empty())
                    out.push_back(RecoveredStatement{ .pc = static_cast<u32>(pc), .text = std::move(text) });
            }
        }

        [[nodiscard]] bool renderStmt(const Stmt &stmt, std::string &out) {
            // slotOf 失败时已记录原因；此处防 INVALID 槽位进入渲染（versionOf 越界）
            const bool needsObj = stmt.kind == Stmt::Kind::StoreMember || stmt.kind == Stmt::Kind::StoreIndex ||
                stmt.kind == Stmt::Kind::StoreDeref || stmt.kind == Stmt::Kind::DeleteMember;
            const bool needsSrc = stmt.exprNode == INVALID && stmt.kind != Stmt::Kind::DebuggerStmt &&
                stmt.kind != Stmt::Kind::DeleteMember && stmt.kind != Stmt::Kind::ExprStmt;
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

        void fail(const std::string &reason) {
            if(_unsupported.empty())
                _unsupported = reason;
        }

        Runtime &_rt;
        vm::Chunk &_chunk;
        const FuncMeta *_meta;
        bool _functionLike;
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
        u32 _tempCounter{};
    };

    ExprRecovery recoverUnitBody(Runtime &rt, vm::Chunk &chunk, const FuncMeta *meta, const bool functionLike) {
        ExprBuilder builder{ rt, chunk, meta, functionLike };
        return builder.run();
    }
} // namespace cial::decompile
