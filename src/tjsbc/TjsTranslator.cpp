// Copyright (c) 2026/10/26
//

#include "TjsTranslator.hpp"

#include <cstring>
#include <map>


#include "gen/Label.hpp"
#include "runtime/AtomTable.hpp"
#include "runtime/Runtime.hpp"

namespace cial::tjsbc {

    namespace {

        // TJS 操作码（tjsInterCodeGen.h tTJSVMCodes 枚举序）
        enum TjsOp : i32 {
            OP_NOP = 0, OP_CONST = 1, OP_CP = 2, OP_CL = 3, OP_CCL = 4, OP_TT = 5,
            OP_TF = 6, OP_CEQ = 7, OP_CDEQ = 8, OP_CLT = 9, OP_CGT = 10,
            OP_SETF = 11, OP_SETNF = 12, OP_LNOT = 13, OP_NF = 14, OP_JF = 15,
            OP_JNF = 16, OP_JMP = 17,
            OP_INC = 18, OP_DEC = 22,
            OP_LOR = 26, OP_LAND = 30, OP_BOR = 34, OP_BXOR = 38, OP_BAND = 42,
            OP_SAR = 46, OP_SAL = 50, OP_SR = 54, OP_ADD = 58, OP_SUB = 62,
            OP_MOD = 66, OP_DIV = 70, OP_IDIV = 74, OP_MUL = 78,
            OP_BNOT = 82, OP_TYPEOF = 83, OP_TYPEOFD = 84, OP_TYPEOFI = 85,
            OP_EVAL = 86, OP_EEXP = 87, OP_CHKINS = 88, OP_ASC = 89, OP_CHR = 90,
            OP_NUM = 91, OP_CHS = 92, OP_INV = 93, OP_CHKINV = 94,
            OP_INT = 95, OP_REAL = 96, OP_STR = 97, OP_OCTET = 98,
            OP_CALL = 99, OP_CALLD = 100, OP_CALLI = 101, OP_NEW = 102,
            OP_GPD = 103, OP_SPD = 104, OP_SPDE = 105, OP_SPDEH = 106,
            OP_GPI = 107, OP_SPI = 108, OP_SPIE = 109, OP_GPDS = 110,
            OP_SPDS = 111, OP_GPIS = 112, OP_SPIS = 113, OP_SETP = 114,
            OP_GETP = 115, OP_DELD = 116, OP_DELI = 117, OP_SRV = 118,
            OP_RET = 119, OP_ENTRY = 120, OP_EXTRY = 121, OP_THROW = 122,
            OP_CHGTHIS = 123, OP_GLOBAL = 124, OP_ADDCI = 125,
            OP_REGMEMBER = 126, OP_DEBUGGER = 127,
        };

        [[noreturn]] void trError(const std::string &msg) {
            throw std::runtime_error("tjs translate: " + msg);
        }

        // 指令码字长度（含操作码字；与 tjsScriptBlock.cpp TranslateCodeAddress
        // 逐 case 对齐：寄存器操作数占 1 码字、CODE_ADDR 偏移占 1 码字）
        i32 opcodeSize(const i32 op) {
            switch(op) {
                case OP_GPD: case OP_GPDS: case OP_SPDS: case OP_SPD: case OP_SPDE:
                case OP_SPDEH: case OP_GPI: case OP_SPI: case OP_SPIE:
                case OP_GPIS: case OP_SPIS: case OP_DELD: case OP_DELI:
                case OP_TYPEOFD: case OP_TYPEOFI:
                    return 4; // op, 3 操作数
                case OP_NOP: case OP_NF:
                    return 1;
                case OP_CONST:
                    return 3; // op, r, da
                case OP_CP: case OP_CEQ: case OP_CDEQ: case OP_CLT: case OP_CGT:
                case OP_CHKINS:
                    return 3; // op, r1, r2
                case OP_TT: case OP_TF: case OP_SETF: case OP_SETNF: case OP_LNOT:
                case OP_BNOT: case OP_ASC: case OP_CHR: case OP_NUM: case OP_CHS:
                case OP_CL: case OP_INV: case OP_CHKINV: case OP_TYPEOF:
                case OP_EVAL: case OP_EEXP: case OP_INT: case OP_REAL: case OP_STR:
                case OP_OCTET:
                    return 2; // op, r
                case OP_CCL:
                    return 3; // op, r, n
                case OP_JF: case OP_JNF: case OP_JMP:
                    return 2; // op, 相对字偏移
                case OP_ENTRY:
                    return 3; // op, catch 偏移, ex 寄存器
                case OP_THROW: return 2;
                case OP_CHGTHIS: return 3;
                case OP_GLOBAL: return 2;
                case OP_ADDCI: return 3;
                case OP_SETP: case OP_GETP:
                    return 3;
                case OP_SRV: return 2;
                case OP_CALL: case OP_CALLD: case OP_CALLI: case OP_NEW:
                    return -1; // 变长（单独解码）
                default:
                    break;
            }
            // 16 组四连变体（INC..MUL 各 base+0/1/2/3）：
            //   二目组(LOR..MUL)：base=3 / PD=5 / PI=5 / P=4
            //   INC/DEC 组：base=2 / PD=4 / PI=4 / P=3（无源操作数）
            if(op >= OP_INC && op < OP_BNOT) {
                const i32 k4 = (op - OP_INC) % 4;
                if(op < OP_LOR) // INC/DEC 组
                    return k4 == 0 ? 2 : (k4 == 3 ? 3 : 4);
                return k4 == 0 ? 3 : (k4 == 3 ? 4 : 5);
            }
            return 1;
        }

        // 解码后的单条 TJS 指令（操作数已转 TJS 寄存器 r / 数据下标）
        struct Decoded {
            i32 op{};
            i32 word{};        // 指令首码字偏移
            i32 r1 = 0, r2 = 0, r3 = 0, r4 = 0;
            i32 dataIdx = -1;  // CONST/PD/CALLD 的常量槽下标
            i32 imm = 0;       // CCL 计数 / 跳转相对字偏移 / CALL 实参数（-1 省略 / -2 展开）
            Vec<i32> args{};   // CALL 族实参寄存器（TJS r）
            Vec<std::pair<i32, i32>> fatEntries{}; // 展开调用 (fatType, TJS r) 表
            i32 size = 1;      // 本指令码字步长（CALL 族在 decode 内计算）
        };

        enum class JumpKind { Jmp, JmpE, JmpNE };

        struct JumpFixup {
            size_t instIdx; // Jmp 类指令的 TAC 下标
            size_t targetWord; // 目标字偏移
            JumpKind kind;
        };

        struct EntryFixup {
            size_t tryStart; // ENTRY 之后首条 TAC
            size_t catchWord; // catch 目标字偏移
            u16 exValueReg;  // 异常值落点（映射后）
        };

        // 段位取值（带越界检查）
        const i8 &poolByte(const TjsBytecodeFile &f, const i32 idx) {
            if(idx < 0 || idx >= static_cast<i32>(f.pool.bytes.size()))
                trError("BYTE constant index out of range");
            return f.pool.bytes[static_cast<size_t>(idx)];
        }

        const i16 &poolShort(const TjsBytecodeFile &f, const i32 idx) {
            if(idx < 0 || idx >= static_cast<i32>(f.pool.shorts.size()))
                trError("SHORT constant index out of range");
            return f.pool.shorts[static_cast<size_t>(idx)];
        }

        const i32 &poolInt(const TjsBytecodeFile &f, const i32 idx) {
            if(idx < 0 || idx >= static_cast<i32>(f.pool.ints.size()))
                trError("INTEGER constant index out of range");
            return f.pool.ints[static_cast<size_t>(idx)];
        }

        const i64 &poolLong(const TjsBytecodeFile &f, const i32 idx) {
            if(idx < 0 || idx >= static_cast<i32>(f.pool.longs.size()))
                trError("LONG constant index out of range");
            return f.pool.longs[static_cast<size_t>(idx)];
        }

        const double &poolReal(const TjsBytecodeFile &f, const i32 idx) {
            if(idx < 0 || idx >= static_cast<i32>(f.pool.reals.size()))
                trError("REAL constant index out of range");
            return f.pool.reals[static_cast<size_t>(idx)];
        }

        const Vec<u8> &poolOctet(const TjsBytecodeFile &f, const i32 idx) {
            if(idx < 0 || idx >= static_cast<i32>(f.pool.octets.size()))
                trError("OCTET constant index out of range");
            return f.pool.octets[static_cast<size_t>(idx)];
        }

        const String &poolString(const TjsBytecodeFile &f, const i32 idx) {
            if(idx < 0 || idx >= static_cast<i32>(f.pool.strings.size()))
                trError("STRING constant index out of range");
            return f.pool.strings[static_cast<size_t>(idx)];
        }

        // DataArea 槽下标 → STRING 值（槽类型须为 STRING）
        const String &dataSlotString(const TjsContext &ctx, const TjsBytecodeFile &f,
                                     const i32 dataIdx) {
            if(dataIdx < 0 || dataIdx >= static_cast<i32>(ctx.data.size()))
                trError("member operand: data slot index out of range");
            const auto &slot = ctx.data[static_cast<size_t>(dataIdx)];
            if(slot.type != TJS_TYPE_STRING)
                trError("member operand: data slot is not a string");
            return poolString(f, slot.index);
        }

        // 解码单条 TJS 指令（CALL 族变长展开，TranslateCodeAddress 对齐）
        Decoded decode(const Vec<i16> &code, const i32 i) {
            Decoded d{};
            d.op = code[i];
            d.word = i;
            d.size = opcodeSize(d.op); // 非 CALL 族默认；CALL 族在分支内覆盖
            auto reg = [&](const i32 wordIdx) { return static_cast<i32>(code[i + wordIdx]); };

            switch(d.op) {
                case OP_NOP: case OP_NF: case OP_EXTRY: case OP_REGMEMBER:
                case OP_DEBUGGER:
                    return d;
                case OP_CHGTHIS:
                    d.r1 = reg(1); // 函数
                    d.r2 = reg(2); // this 值
                    return d;
                case OP_ENTRY:
                    d.r1 = reg(1); // catch 目标（相对字偏移）
                    d.r2 = reg(2); // 异常值寄存器（TJS r）
                    return d;
                case OP_CONST:
                    d.r1 = reg(1);
                    d.dataIdx = reg(2);
                    return d;
                case OP_CP: case OP_CEQ: case OP_CDEQ: case OP_CLT: case OP_CGT:
                    d.r1 = reg(1);
                    d.r2 = reg(2);
                    return d;
                case OP_TT: case OP_TF:
                    d.r1 = reg(1);
                    return d;
                case OP_SETF: case OP_SETNF:
                    d.r1 = reg(1);
                    return d;
                case OP_LNOT: case OP_CHS: case OP_INV: case OP_CHKINV:
                case OP_INT: case OP_REAL: case OP_STR: case OP_INC:
                case OP_DEC: case OP_SRV: case OP_THROW: case OP_GLOBAL:
                case OP_BNOT: case OP_NUM: case OP_ASC: case OP_CHR:
                case OP_TYPEOF: case OP_CL: case OP_EVAL: case OP_EEXP:
                case OP_OCTET:
                    d.r1 = reg(1);
                    return d;
                case OP_RET:
                    return d; // 无操作数（返回值由前置 SRV 指定）
                case OP_CCL:
                    d.r1 = reg(1);
                    d.imm = reg(2);
                    return d;
                case OP_JF: case OP_JNF: case OP_JMP:
                    d.imm = reg(1);
                    return d;
                case OP_CHKINS:
                    d.r1 = reg(1); // 类名 reg
                    d.r2 = reg(2); // 对象 reg
                    return d;
                case OP_SETP: // [target, src]：*target = src
                    d.r1 = reg(1);
                    d.r2 = reg(2);
                    return d;
                case OP_GETP: // [dst, target]：dst = *target
                    d.r1 = reg(1);
                    d.r2 = reg(2);
                    return d;
                case OP_CALL: case OP_NEW: {
                    d.r1 = reg(1);
                    d.r2 = reg(2);
                    d.imm = reg(3);
                    if(d.imm == -1) { // 省略透传（tjs2-bytecode.md §4）
                        d.size = 4;
                        return d;
                    }
                    if(d.imm == -2) { // 展开调用：条目表 (fatType, reg) × count
                        const i32 count = reg(4);
                        d.size = 5 + count * 2;
                        d.fatEntries.reserve(static_cast<size_t>(count));
                        for(i32 k = 0; k < count; ++k)
                            d.fatEntries.emplace_back(reg(5 + k * 2), reg(6 + k * 2));
                        return d;
                    }
                    d.args.reserve(static_cast<size_t>(d.imm));
                    for(i32 k = 0; k < d.imm; ++k)
                        d.args.push_back(reg(4 + k));
                    d.size = 4 + d.imm;
                    return d;
                }
                case OP_CALLD: case OP_CALLI: {
                    d.r1 = reg(1);
                    d.r2 = reg(2);
                    if(d.op == OP_CALLD)
                        d.dataIdx = reg(3);
                    else
                        d.r3 = reg(3);
                    d.imm = reg(4);
                    if(d.imm == -1) {
                        d.size = 5;
                        return d;
                    }
                    if(d.imm == -2) {
                        const i32 count = reg(5);
                        d.size = 6 + count * 2;
                        d.fatEntries.reserve(static_cast<size_t>(count));
                        for(i32 k = 0; k < count; ++k)
                            d.fatEntries.emplace_back(reg(6 + k * 2), reg(7 + k * 2));
                        return d;
                    }
                    d.args.reserve(static_cast<size_t>(d.imm));
                    for(i32 k = 0; k < d.imm; ++k)
                        d.args.push_back(reg(5 + k));
                    d.size = 5 + d.imm;
                    return d;
                }
                default:
                    break;
            }
            // 4 连组：base / PD / PI / P
            if(d.op == OP_ADDCI || d.op == OP_REGMEMBER)
                trError("class construction opcodes are not supported outside class bodies");
            if(d.op >= OP_INC && d.op <= OP_MUL) {
                const i32 k4 = (d.op - OP_INC) % 4;
                if(k4 == 0) { // base：破坏性双目 r1 = r1 OP r2 / INC·DEC r1
                    d.r1 = reg(1);
                    d.r2 = reg(2);
                    return d;
                }
                if(k4 == 1 || k4 == 2) { // PD（成员名 DataArea 槽）/ PI（成员名 reg）
                    d.r1 = reg(1); // dst
                    d.r2 = reg(2); // obj
                    if(k4 == 1)
                        d.dataIdx = reg(3);
                    else
                        d.r3 = reg(3);
                    d.r4 = reg(4); // rhs
                    return d;
                }
                // P（属性闭包）：二目组 [dst, target, param]、INC/DEC 组 [dst, target]
                d.r1 = reg(1); // dst（0 = 丢弃）
                d.r2 = reg(2); // target：属性节点对象
                if(d.op >= OP_LOR)
                    d.r3 = reg(3); // param
                return d;
            }
            // 属性 4 字族，两套操作数序（tjsDisassemble 对齐）：
            //   GPD/GPDS: dst, obj, memberData    GPI/GPIS: dst, obj, memberReg
            //   SPD/SPDE/SPDEH/SPDS: obj, memberData, src
            //   SPI/SPIE/SPIS: obj, memberReg, src
            //   DELD/TYPEOFD: dst, obj, memberData  DELI/TYPEOFI: dst, obj, memberReg
            if(d.op == OP_GPD || d.op == OP_GPDS) {
                d.r1 = reg(1);      // dst
                d.r2 = reg(2);      // obj
                d.dataIdx = reg(3); // member 名（DataArea 槽）
                d.r4 = reg(4);
                return d;
            }
            if(d.op == OP_SPD || d.op == OP_SPDE || d.op == OP_SPDEH || d.op == OP_SPDS) {
                d.r1 = reg(1);      // obj
                d.dataIdx = reg(2); // member 名（DataArea 槽）
                d.r3 = reg(3);      // src
                d.r4 = reg(4);
                return d;
            }
            if(d.op == OP_GPI || d.op == OP_GPIS) {
                d.r1 = reg(1); // dst
                d.r2 = reg(2); // obj
                d.r3 = reg(3); // member reg
                d.r4 = reg(4);
                return d;
            }
            if(d.op == OP_SPI || d.op == OP_SPIE || d.op == OP_SPIS) {
                d.r1 = reg(1); // obj
                d.r2 = reg(2); // member reg
                d.r3 = reg(3); // src
                d.r4 = reg(4);
                return d;
            }
            if(d.op == OP_DELD) { // [dst, obj, daIdx]
                d.r1 = reg(1);      // dst（bool 结果）
                d.r2 = reg(2);      // obj
                d.dataIdx = reg(3); // member 名（DataArea 槽）
                d.r4 = reg(4);
                return d;
            }
            if(d.op == OP_DELI) { // [dst, obj, memberReg]
                d.r1 = reg(1);
                d.r2 = reg(2);
                d.r3 = reg(3);
                d.r4 = reg(4);
                return d;
            }
            if(d.op == OP_TYPEOFD) { // [dst, obj, daIdx]
                d.r1 = reg(1);
                d.r2 = reg(2);
                d.dataIdx = reg(3);
                d.r4 = reg(4);
                return d;
            }
            if(d.op == OP_TYPEOFI) { // [dst, obj, memberReg]
                d.r1 = reg(1);
                d.r2 = reg(2);
                d.r3 = reg(3);
                d.r4 = reg(4);
                return d;
            }
            trError("unsupported instruction in M1 translator: opcode " + std::to_string(d.op));
        }

    } // namespace

    struct TjsTranslator::Impl {
        Runtime &rt;
        const TjsBytecodeFile &file;
        Vec<MarkSweepHeader *> roots{};

        Impl(Runtime &rt, const TjsBytecodeFile &file) : rt(rt), file(file) {}

        ~Impl() {
            for(const auto *root : roots)
                rt.removeHandleVal(const_cast<MarkSweepHeader *>(root));
        }
    };

    TjsTranslator::TjsTranslator(Runtime &rt, const TjsBytecodeFile &file) :
        _impl(std::make_unique<Impl>(rt, file)) {}

    TjsTranslator::~TjsTranslator() = default;

    TjsTranslatedProgram TjsTranslator::translate() const {
        using inter::Label;
        Runtime &rt = _impl->rt;
        const TjsBytecodeFile &file = _impl->file;

        TjsTranslatedProgram program{};
        program.topLevel = file.topLevel;
        program.contexts.resize(file.contexts.size());

        // INTER_OBJECT 占位回填记录（Phase B' 经 GC chunk 指针回填 FuncMeta*）：
        // 每 context 一组 (常量下标, 目标 context 索引)
        Vec<Vec<std::pair<u16, i32>>> deferredPerCtx{};
        // ctClass 的 var 成员：(名字 Atom, 初值常量槽)
        Vec<Vec<std::pair<Atom, TjsDataSlot>>> classVarsPerCtx{};

        for(size_t ci = 0; ci < file.contexts.size(); ++ci) {
            deferredPerCtx.emplace_back();
            classVarsPerCtx.emplace_back();
            const TjsContext &ctx = file.contexts[ci];
            if(ctx.type == TjsContextType::SuperClassGetter)
                continue; // 由 ctClass 经 superClassGetter 加载期解释消费，无产物

            // ── ctClass：加载期解释（mapping.md §7）——body 不生成 TAC ──
            if(ctx.type == TjsContextType::Class) {
                auto &out = program.contexts[ci];
                out.isClass = true;
                out.parent = ctx.parent;
                out.type = ctx.type;
                auto classMetaGuard = rt.create<ClassMeta>(
                    rt.atomTable.intern(poolString(file, ctx.name)), 0);
                auto *meta = classMetaGuard.get();
                rt.addHandleVal(meta);
                _impl->roots.push_back(meta);
                out.classMeta = meta;

                // 解释 body：CONST/CL 跟踪寄存器常量；SPDS this.*name ← reg 记 var；
                // GPD+CHGTHIS+CALL 为编译器生成的 super 类体初始化三连（继承语义由
                // superClassGetter 解释出的 extends 承担），整段跳过
                std::map<i32, i32> regSlot; // 寄存器 → 常量槽
                i32 i2 = 0;
                while(i2 < static_cast<i32>(ctx.code.size())) {
                    const i32 op = ctx.code[static_cast<size_t>(i2)];
                    if(op == OP_CONST) {
                        regSlot[ctx.code[static_cast<size_t>(i2 + 1)]] =
                            ctx.code[static_cast<size_t>(i2 + 2)];
                        i2 += 3;
                    } else if(op == OP_CL || op == OP_ADDCI) {
                        if(op == OP_CL)
                            regSlot.erase(ctx.code[static_cast<size_t>(i2 + 1)]);
                        i2 += op == OP_ADDCI ? 3 : 2;
                    } else if(op == OP_GPD) {
                        // super 初始化三连：GPD r,-2,<父类槽>; CHGTHIS r,-1; CALL 0,r,n
                        const i32 callAt = i2 + 7;
                        if(i2 + 11 <= static_cast<i32>(ctx.code.size())
                           && ctx.code[static_cast<size_t>(i2 + 4)] == OP_CHGTHIS
                           && ctx.code[static_cast<size_t>(callAt)] == OP_CALL) {
                            i2 += 11 + ctx.code[static_cast<size_t>(callAt + 3)];
                        } else {
                            trError("class body contains unrecognized GPD pattern (M2)");
                        }
                    } else if(op == OP_SPDS || op == OP_SPDE || op == OP_SPDEH) {
                        // obj=%-1(this)，member=DataArea 槽（STRING 名），src=reg
                        //（src=r0 = void：无初值的 var 声明）
                        const i32 memberData = ctx.code[static_cast<size_t>(i2 + 2)];
                        const i32 srcReg = ctx.code[static_cast<size_t>(i2 + 3)];
                        const auto slotIt = regSlot.find(srcReg);
                        if(ctx.code[static_cast<size_t>(i2 + 1)] != -1
                           || (slotIt == regSlot.end() && srcReg != 0))
                            trError("class body contains non-static member pattern (M2)");
                        const auto &nameSlot = ctx.data[static_cast<size_t>(memberData)];
                        if(nameSlot.type != TJS_TYPE_STRING)
                            trError("class var member name is not a string");
                        const auto nameAtom =
                            rt.atomTable.intern(poolString(file, nameSlot.index));
                        meta->setMember(
                            MemberShapeMeta{ .name = nameAtom, .isVar = true },
                            ClassFieldMeta{});
                        // 初值槽 = src 寄存器 CONST 跟踪的槽；r0 → void
                        classVarsPerCtx[ci].emplace_back(
                            nameAtom,
                            slotIt != regSlot.end()
                                ? ctx.data[static_cast<size_t>(slotIt->second)]
                                : TjsDataSlot{TJS_TYPE_VOID, 0});
                        i2 += 4;
                    } else if(op == OP_SRV) {
                        i2 += 2;
                    } else if(op == OP_REGMEMBER || op == OP_RET || op == OP_NOP) {
                        // 方法注册经 parent 链在 Phase B 处理；RET 结束
                        if(op == OP_RET)
                            break;
                        i2 += 1;
                    } else if(op == OP_DEBUGGER) {
                        i2 += 1;
                    } else {
                        trError("class body contains unrecognized pattern (M2): opcode "
                                + std::to_string(op));
                    }
                }

                // superClassGetter 加载期解释（mapping.md §7）：getter ctx 代码内
                // 按 superClassGetterPointers 逐个取子程序，识别
                // `GPD r,-2,<名字槽>; SRV r; RET` 得父类名，填 extends
                if(ctx.superClassGetter >= 0) {
                    const auto &getter = file.contexts[static_cast<size_t>(ctx.superClassGetter)];
                    for(const i32 ptr : getter.superClassGetterPointers) {
                        if(ptr < 0 || ptr + 7 > static_cast<i32>(getter.code.size())
                           || getter.code[static_cast<size_t>(ptr)] != OP_GPD
                           || getter.code[static_cast<size_t>(ptr + 4)] != OP_SRV
                           || getter.code[static_cast<size_t>(ptr + 6)] != OP_RET)
                            trError("superClassGetter contains unsupported pattern (M2)");
                        const i32 memberData = getter.code[static_cast<size_t>(ptr + 3)];
                        meta->extends.push_back(rt.atomTable.intern(
                            dataSlotString(getter, file, memberData)));
                    }
                }
                continue;
            }

            // ── ctProperty 聚合：getter/setter 子 context 在 Phase B 装配 ──
            if(ctx.type == TjsContextType::Property) {
                auto &out = program.contexts[ci];
                out.isProperty = true;
                out.parent = ctx.parent;
                out.type = ctx.type;
                out.nameAtom = ctx.name >= 0 ? rt.atomTable.intern(poolString(file, ctx.name))
                                             : Atom{}; // 匿名 context（name=-1）
                continue;
            }
            // getter/setter：按函数路径转译（type 保留，Phase B 装配 PropMeta）

            // ra 基偏移恒等平移：K = VC+RC，但实参区 ra[-3-i] 要求 K ≥ 2+argc，
            // 多参函数取 max 修正（如 g(a,b) 的 %-4 越出 VC+RC=3）
            i32 k = ctx.maxVariableCount + ctx.variableReserveCount;
            if(k < ctx.funcDeclArgCount + 2)
                k = ctx.funcDeclArgCount + 2;
            const auto regF = inter::Register{static_cast<u16>(k + ctx.maxFrameCount + 1)}; // 标志物化
            const auto regT = inter::Register{static_cast<u16>(regF.index() + 1)};          // 临时
            const auto regN = inter::Register{static_cast<u16>(regF.index() + 2)};          // 名字常量临时
            const auto regT2 = inter::Register{static_cast<u16>(regF.index() + 3)};         // 第二临时
            const u16 regCount = static_cast<u16>(regT2.index() + 1 + ctx.funcDeclArgCount + 1);
            // TJS dst=0 的调用是丢弃语义（原版传 NULL 结果指针，不写回）——落到
            // 窗口顶部保留位。不能写 %K：TJS ra[0] 是 void 寄存器，SRV r0 读它，
            // 写入调用的返回值会沿隐式 return 链污染上层（test_variant 暴露）
            const auto regDiscard = inter::Register{static_cast<u16>(regCount - 1)};

            vm::Chunk chunk{}; // 常量 0 = Void

            auto mapReg = [&](const i32 r) { return inter::Register{static_cast<u16>(r + k)}; };
            auto atomConst = [&](const String &s) {
                return chunk.addConstant(Constant{rt.atomTable.intern(s)});
            };

            // prologue：ra[-2]、ra[-1] 都取 this、ra[0]=void；函数搬实参。
            // TJS2 的 ra[-2] 是 objthis-proxy（this 优先、global 回落）：方法/getter
            // 内经 -2 的成员访问必须解析到实例——映射为 This（Ciallang getThis 自带
            // this→global 回落；顶层/全局函数 this 本就是全局对象，行为不变）
            chunk.emit<inter::This>(inter::Register{static_cast<u16>(k - 2 >= 0 ? k - 2 : 0)});
            chunk.emit<inter::This>(inter::Register{static_cast<u16>(k - 1)});
            chunk.emit<inter::Load>(inter::Register{static_cast<u16>(k)}, ConstIdx{0});
            if(ctx.type != TjsContextType::TopLevel && ctx.funcDeclArgCount > 0) {
                // 实参搬运：%i → %K-3-i。目标与源常为同组槽位的置换（g(a,b):
                // %0↔%1），顺序搬运必丢一个——先批量备份到 scratch，再落位
                const u16 scratchBase = static_cast<u16>(regN.index() + 1);
                for(i32 a = 0; a < ctx.funcDeclArgCount; ++a)
                    chunk.emit<inter::DLocal>(inter::Register{static_cast<u16>(a)},
                                              inter::Register{static_cast<u16>(scratchBase + a)});
                for(i32 a = 0; a < ctx.funcDeclArgCount; ++a)
                    chunk.emit<inter::DLocal>(
                        inter::Register{static_cast<u16>(scratchBase + a)},
                        inter::Register{static_cast<u16>(k - 3 - a)});
            }

            // collapse 参数（TJS `function f(a, b*)`）：FuncDeclCollapseBase>=0 时
            // 把本帧实际实参 %base.. 收拢成 Array 落 TJS 槽 ra[-3-base]
            //（须在实参仍完整的 prologue 末尾执行）
            if(ctx.funcDeclCollapseBase >= 0)
                chunk.emitRaw<inter::CollapseArgs>(
                    mapReg(-3 - ctx.funcDeclCollapseBase),
                    Integer{ctx.funcDeclCollapseBase});

            const i32 tacBase = static_cast<i32>(chunk.getInstVec().size());
            Vec<std::pair<i32, i32>> offsetMap{}; // 字偏移 → TAC 下标
            Vec<JumpFixup> jumpFixups{};
            Vec<EntryFixup> entryFixups{};
            i32 srvTjsReg = -1;

            // Pass 1 + 2 合流：解码即发射，跳转经 fixup 后填
            i32 i = 0;
            while(i < static_cast<i32>(ctx.code.size())) {
                Decoded d = decode(ctx.code, i);
                offsetMap.emplace_back(d.word, static_cast<i32>(chunk.getInstVec().size()));
                const auto r1 = mapReg(d.r1);
                const auto r2 = mapReg(d.r2);

                // ── 前瞻融合（M3）：比较/TT/TF 紧随 JF/JNF 时，Cmp 直写 _zf +
                // JmpE/JmpNE 直连，省 %F 物化与 Test。TJS 编译器保证 flag 消费者
                // （JF/JNF/SETF/SETNF/NF）紧邻生产者，非跳转消费者走旧路径 ──
                {
                    const i32 next =
                        i + d.size < static_cast<i32>(ctx.code.size()) ? ctx.code[static_cast<size_t>(i + d.size)] : -1;
                    const bool nextIsJump = next == OP_JF || next == OP_JNF;
                    const bool isCmp = d.op == OP_CEQ || d.op == OP_CDEQ
                                    || d.op == OP_CLT || d.op == OP_CGT;
                    const bool isTTTF = d.op == OP_TT || d.op == OP_TF;
                    if((isCmp || isTTTF) && nextIsJump) {
                        if(isCmp) {
                            switch(d.op) {
                                case OP_CEQ: chunk.emit<inter::CmpEQ>(r1, r2); break;
                                case OP_CDEQ: chunk.emit<inter::CmpDEQ>(r1, r2); break;
                                case OP_CLT: chunk.emit<inter::CmpLT>(r1, r2); break;
                                default: chunk.emit<inter::CmpGT>(r1, r2); break;
                            }
                        } else {
                            chunk.emit<inter::Test>(r1); // TT/TF = _zf ← bool(r)
                        }
                        Decoded j = decode(ctx.code, i + d.size);
                        offsetMap.emplace_back(j.word, static_cast<i32>(chunk.getInstVec().size()));
                        const i32 target = j.word + j.imm;
                        // JF=真跳 → JmpE；JNF=假跳 → JmpNE；TF 取反翻转方向
                        JumpKind kind = (next == OP_JF) != (d.op == OP_TF) ? JumpKind::JmpE
                                                                          : JumpKind::JmpNE;
                        jumpFixups.push_back({chunk.getInstVec().size(),
                                              static_cast<size_t>(target), kind});
                        chunk.emit<inter::JmpE>(Label{0}); // 占位，repl 按 kind 修正
                        i = j.word + j.size;
                        continue;
                    }
                }
                const auto r3 = mapReg(d.r3);
                const auto r4 = mapReg(d.r4);

                switch(d.op) {
                    case OP_NOP: case OP_EXTRY: case OP_DEBUGGER:
                        break;
                    case OP_ENTRY:
                        entryFixups.push_back(
                            {chunk.getInstVec().size(),
                             static_cast<size_t>(d.word + d.r1), mapReg(d.r2).index()});
                        break;
                    case OP_CONST: {
                        const auto &slot = ctx.data[static_cast<size_t>(d.dataIdx)];
                        switch(slot.type) {
                            case TJS_TYPE_VOID:
                            case TJS_TYPE_OBJECT:
                                chunk.emit<inter::Load>(r1, ConstIdx{0});
                                break;
                            case TJS_TYPE_BYTE:
                                chunk.emit<inter::LoadImm>(
                                    r1, static_cast<Integer>(poolByte(file, slot.index)));
                                break;
                            case TJS_TYPE_SHORT:
                                chunk.emit<inter::LoadImm>(
                                    r1, static_cast<Integer>(poolShort(file, slot.index)));
                                break;
                            case TJS_TYPE_INTEGER:
                                chunk.emit<inter::LoadImm>(
                                    r1, static_cast<Integer>(poolInt(file, slot.index)));
                                break;
                            case TJS_TYPE_LONG:
                                chunk.emit<inter::LoadImm>(
                                    r1, static_cast<Integer>(poolLong(file, slot.index)));
                                break;
                            case TJS_TYPE_REAL:
                                chunk.emit<inter::Load>(
                                    r1, chunk.addConstant(
                                            Constant{Real{poolReal(file, slot.index)}}));
                                break;
                            case TJS_TYPE_STRING:
                                chunk.emit<inter::Load>(
                                    r1, atomConst(poolString(file, slot.index)));
                                break;
                            case TJS_TYPE_INTER_OBJECT: {
                                if(slot.index < 0
                                   || slot.index >= static_cast<i32>(file.contexts.size()))
                                    trError("INTER_OBJECT index out of range");
                                // 占位常量：Phase B'（全部 context 完成后）经 GC chunk
                                // 指针回填 FuncMeta*——此处 chunk 仍在栈上不可持指针。
                                // 直接 append 绕过 addConstant 去重：空 Constant 彼此
                                // 相等，去重会让多个 INTER_OBJECT 站点共享同一槽位，
                                // 回填时互相覆盖（global.A 写成 B 的类对象这类错位）
                                auto &consts = chunk.getConstants();
                                if(consts.size() >= std::numeric_limits<u16>::max())
                                    trError("too many constants");
                                const u16 cidx = static_cast<u16>(consts.size());
                                consts.emplace_back(Constant{});
                                deferredPerCtx[ci].emplace_back(cidx, slot.index);
                                chunk.emit<inter::Load>(r1, ConstIdx{cidx});
                                break;
                            }
                            case TJS_TYPE_OCTET: {
                                const auto &bytes = poolOctet(file, slot.index);
                                auto *payload = SharedPayload<Octet>::makeSingle(
                                    Octet{bytes.data(), static_cast<u32>(bytes.size())});
                                chunk.emit<inter::Load>(r1, chunk.addConstant(Constant{payload}));
                                break;
                            }
                            default:
                                trError("unknown constant slot type");
                        }
                        break;
                    }
                    case OP_CP:
                        chunk.emit<inter::DLocal>(r2, r1);
                        break;
                    case OP_CL:
                        chunk.emit<inter::Load>(r1, ConstIdx{0});
                        break;
                    case OP_CCL:
                        for(i32 n = 0; n < d.imm; ++n)
                            chunk.emit<inter::Load>(inter::Register{static_cast<u16>(r1.index() + n)}, ConstIdx{0});
                        break;
                    case OP_CEQ:
                        chunk.emit<inter::EQ>(r1, r2, regF);
                        break;
                    case OP_CDEQ:
                        chunk.emit<inter::AbsEQ>(r1, r2, regF);
                        break;
                    // CLT/CGT 是自然序：CLT → tTJSVariant::GreaterThan 实现为
                    // `this < val2`（TJS2 历史命名陷阱），CGT → LittlerThan 实现为
                    // `this > val2`（tjsVariant.cpp:827/853 实测）
                    case OP_CLT:
                        chunk.emit<inter::LT>(r1, r2, regF);
                        break;
                    case OP_CGT:
                        chunk.emit<inter::GT>(r1, r2, regF);
                        break;
                    case OP_TT:
                        chunk.emit<inter::LNot>(r1, regF);
                        chunk.emit<inter::LNot>(regF, regF);
                        break;
                    case OP_TF:
                        chunk.emit<inter::LNot>(r1, regF);
                        break;
                    case OP_SETF:
                        chunk.emit<inter::DLocal>(regF, r1);
                        break;
                    case OP_SETNF:
                        chunk.emit<inter::LNot>(regF, regT);
                        chunk.emit<inter::DLocal>(regT, r1);
                        break;
                    case OP_LNOT:
                        chunk.emit<inter::LNot>(r1, r1);
                        break;
                    case OP_NF:
                        chunk.emit<inter::LNot>(regF, regF);
                        break;
                    case OP_JF: case OP_JNF: case OP_JMP: {
                        const i32 target = d.word + d.imm;
                        if(d.op == OP_JMP) {
                            jumpFixups.push_back(
                                {chunk.getInstVec().size(), static_cast<size_t>(target),
                                 JumpKind::Jmp});
                            chunk.emit<inter::Jmp>(Label{0});
                        } else {
                            chunk.emit<inter::Test>(regF);
                            // TJS JF = flag 真则跳 → Test 置 _zf=flag 后 JmpE（真跳）；
                            // JNF = 假则跳 → JmpNE。mapping.md §3 表格同款
                            const JumpKind kind =
                                d.op == OP_JF ? JumpKind::JmpE : JumpKind::JmpNE;
                            jumpFixups.push_back(
                                {chunk.getInstVec().size(), static_cast<size_t>(target), kind});
                            chunk.emit<inter::JmpE>(Label{0}); // 占位，repl 时按 kind 修正
                        }
                        break;
                    }
                    case OP_CHKINS:
                        chunk.emit<inter::DLocal>(r2, regT);
                        chunk.emit<inter::ChkIns>(regT, r1);
                        break;
                    case OP_INC: case OP_INC + 1: case OP_INC + 2: case OP_INC + 3:
                    case OP_DEC: case OP_DEC + 1: case OP_DEC + 2: case OP_DEC + 3: {
                        const i32 k4 = (d.op - OP_INC) % 4;
                        const bool isInc = d.op < OP_DEC; // 组判定：INC 族 18..21
                        // v = v ± 1（1 预载在 regT2；v==regT2 时由调用方保证不发生）
                        auto emitOne = [&](const inter::Register v) {
                            if(isInc)
                                chunk.emit<inter::Add>(v, regT2, v);
                            else
                                chunk.emit<inter::Sub>(v, regT2, v);
                        };
                        if(k4 == 0) { // base：单指令 ±1（int/real，其余 TJS increment 报错）
                            if(isInc)
                                chunk.emit<inter::Inc>(r1);
                            else
                                chunk.emit<inter::Dec>(r1);
                            break;
                        }
                        if(k4 == 3) { // P：*target ± 1（属性节点读-算-写）
                            chunk.emit<inter::DerefGet>(r2, regT);
                            chunk.emit<inter::LoadImm>(regT2, Integer{1});
                            emitOne(regT);
                            chunk.emit<inter::DerefSet>(r2, regT);
                            if(d.r1 != 0)
                                chunk.emit<inter::DLocal>(regT, r1);
                            break;
                        }
                        // PD/PI：o[m] ± 1（读-算-写）；PD 且 obj=proxy 时走
                        // GThis/DThis（this→global 回落链）
                        if(k4 == 1) {
                            if(d.r2 == -2) { // obj = proxy
                                const Atom nameAtom2 = rt.atomTable.intern(
                                    dataSlotString(ctx, file, d.dataIdx));
                                chunk.emit<inter::GThis>(nameAtom2, regT);
                                chunk.emit<inter::LoadImm>(regT2, Integer{1});
                                emitOne(regT);
                                chunk.emit<inter::DThis>(nameAtom2, regT);
                                if(d.r1 != 0) // dst=0 丢弃
                                    chunk.emit<inter::DLocal>(regT, r1);
                                break;
                            }
                            // 成员名 Atom 直引（省名字寄存器 Load 与 dispatch 再 intern）
                            const Atom nameAtom = rt.atomTable.intern(
                                dataSlotString(ctx, file, d.dataIdx));
                            chunk.emit<inter::GPropC>(r2, nameAtom, regT);
                            chunk.emit<inter::LoadImm>(regT2, Integer{1});
                            emitOne(regT);
                            chunk.emit<inter::DPropC>(r2, nameAtom, regT);
                            if(d.r1 != 0) // dst=0 丢弃
                                chunk.emit<inter::DLocal>(regT, r1);
                            break;
                        }
                        // k4 == 2（PI）：整数成员名寄存器形态（数组数值下标）
                        chunk.emit<inter::GProp>(r2, r3, regT);
                        chunk.emit<inter::LoadImm>(regT2, Integer{1});
                        emitOne(regT);
                        chunk.emit<inter::DProp>(r2, r3, regT);
                        if(d.r1 != 0) // dst=0 丢弃
                            chunk.emit<inter::DLocal>(regT, r1);
                        break;
                    }
                    // 14 组二目运算 × 4 变体（base/PD/PI/P），组内按 k4 分派
                    case OP_LOR: case OP_LOR + 1: case OP_LOR + 2: case OP_LOR + 3:
                    case OP_LAND: case OP_LAND + 1: case OP_LAND + 2: case OP_LAND + 3:
                    case OP_BOR: case OP_BOR + 1: case OP_BOR + 2: case OP_BOR + 3:
                    case OP_BXOR: case OP_BXOR + 1: case OP_BXOR + 2: case OP_BXOR + 3:
                    case OP_BAND: case OP_BAND + 1: case OP_BAND + 2: case OP_BAND + 3:
                    case OP_SAR: case OP_SAR + 1: case OP_SAR + 2: case OP_SAR + 3:
                    case OP_SAL: case OP_SAL + 1: case OP_SAL + 2: case OP_SAL + 3:
                    case OP_SR: case OP_SR + 1: case OP_SR + 2: case OP_SR + 3:
                    case OP_ADD: case OP_ADD + 1: case OP_ADD + 2: case OP_ADD + 3:
                    case OP_SUB: case OP_SUB + 1: case OP_SUB + 2: case OP_SUB + 3:
                    case OP_MOD: case OP_MOD + 1: case OP_MOD + 2: case OP_MOD + 3:
                    case OP_DIV: case OP_DIV + 1: case OP_DIV + 2: case OP_DIV + 3:
                    case OP_IDIV: case OP_IDIV + 1: case OP_IDIV + 2: case OP_IDIV + 3:
                    case OP_MUL: case OP_MUL + 1: case OP_MUL + 2: case OP_MUL + 3: {
                        const i32 k4 = (d.op - OP_LOR) % 4;
                        const i32 base = d.op - k4;
                        auto emitBinary = [&](const inter::Register a, const inter::Register b,
                                              const inter::Register dst) {
                            switch(base) {
                                case OP_LOR: chunk.emit<inter::LOr>(a, b, dst); break;
                                case OP_LAND: chunk.emit<inter::LAnd>(a, b, dst); break;
                                case OP_BOR: chunk.emit<inter::BOr>(a, b, dst); break;
                                case OP_BXOR: chunk.emit<inter::BXor>(a, b, dst); break;
                                case OP_BAND: chunk.emit<inter::BAnd>(a, b, dst); break;
                                case OP_SAR: chunk.emit<inter::BrShift>(a, b, dst); break;
                                case OP_SAL: chunk.emit<inter::BlShift>(a, b, dst); break;
                                case OP_SR: chunk.emit<inter::BurShift>(a, b, dst); break;
                                case OP_ADD: chunk.emit<inter::Add>(a, b, dst); break;
                                case OP_SUB: chunk.emit<inter::Sub>(a, b, dst); break;
                                case OP_MOD: chunk.emit<inter::Mod>(a, b, dst); break;
                                case OP_DIV: chunk.emit<inter::Div>(a, b, dst); break;
                                case OP_IDIV: chunk.emit<inter::Idiv>(a, b, dst); break;
                                case OP_MUL: chunk.emit<inter::Mul>(a, b, dst); break;
                                default: break;
                            }
                        };
                        if(k4 == 1 || k4 == 2) {
                            // PD/PI：o[m] = o[m] OP p（读-算-写，mapping.md §6.1）；
                            // PD 且 obj=proxy 时走 GThis/DThis（this→global 回落链）
                            if(k4 == 1) {
                                if(d.r2 == -2) {
                                    const Atom nameAtom2 = rt.atomTable.intern(
                                        dataSlotString(ctx, file, d.dataIdx));
                                    chunk.emit<inter::GThis>(nameAtom2, regT);
                                    emitBinary(regT, r4, regT);
                                    chunk.emit<inter::DThis>(nameAtom2, regT);
                                    if(d.r1 != 0)
                                        chunk.emit<inter::DLocal>(regT, r1);
                                    break;
                                }
                                // 成员名 Atom 直引
                                const Atom nameAtom = rt.atomTable.intern(
                                    dataSlotString(ctx, file, d.dataIdx));
                                chunk.emit<inter::GPropC>(r2, nameAtom, regT);
                                emitBinary(regT, r4, regT);
                                chunk.emit<inter::DPropC>(r2, nameAtom, regT);
                                if(d.r1 != 0) // dst=0 丢弃（%K 是 void 槽，不得污染）
                                    chunk.emit<inter::DLocal>(regT, r1);
                            } else {
                                // k4 == 2（PI）：整数成员名寄存器形态（数组数值下标）
                                chunk.emit<inter::GProp>(r2, r3, regT);
                                emitBinary(regT, r4, regT);
                                chunk.emit<inter::DProp>(r2, r3, regT);
                                if(d.r1 != 0)
                                    chunk.emit<inter::DLocal>(regT, r1);
                            }
                        } else if(k4 == 3) {
                            // P：*target = *target OP param（属性节点读-算-写）
                            chunk.emit<inter::DerefGet>(r2, regT);
                            emitBinary(regT, r3, regT);
                            chunk.emit<inter::DerefSet>(r2, regT);
                            if(d.r1 != 0)
                                chunk.emit<inter::DLocal>(regT, r1);
                        } else {
                            emitBinary(r1, r2, r1);
                        }
                        break;
                    }
                    case OP_EVAL: case OP_EEXP:
                        // 字符串即时求值（源码路径 EvalStr 同款；EEXP 内嵌表达式
                        // 形态差异仅编译期，运行期语义一致）
                        chunk.emit<inter::EvalStr>(r1, r1);
                        break;
                    case OP_BNOT:
                        chunk.emit<inter::BNot>(r1, r1);
                        break;
                    case OP_NUM:
                        chunk.emit<inter::ToNumber>(r1, r1);
                        break;
                    case OP_TYPEOF:
                        chunk.emit<inter::TypeOf>(r1, r1);
                        break;
                    case OP_ASC:
                        chunk.emit<inter::Asc>(r1, r1);
                        break;
                    case OP_CHR:
                        chunk.emit<inter::Chr>(r1, r1);
                        break;
                    case OP_CHS:
                        chunk.emit<inter::ChgSign>(r1, r1);
                        break;
                    case OP_INV:
                        chunk.emit<inter::Inv>(r1);
                        break;
                    case OP_CHKINV:
                        chunk.emit<inter::ChkInv>(r1, r1);
                        break;
                    case OP_INT:
                        chunk.emit<inter::ToInt>(r1, r1);
                        break;
                    case OP_REAL:
                        chunk.emit<inter::ToReal>(r1, r1);
                        break;
                    case OP_STR:
                        chunk.emit<inter::ToString>(r1, r1);
                        break;
                    case OP_OCTET:
                        chunk.emit<inter::ToOctet>(r1, r1);
                        break;
                    case OP_CALL: case OP_CALLD: case OP_CALLI: {
                        inter::Register func = r2;
                        if(d.op == OP_CALLD) {
                            if(d.r2 == -2) { // obj = proxy：基类构造直调（this→global 回落）
                                const Atom nameAtom2 =
                                    rt.atomTable.intern(dataSlotString(ctx, file, d.dataIdx));
                                chunk.emit<inter::GThis>(nameAtom2, regN);
                            } else {
                                chunk.emit<inter::GPropC>(
                                    r2, rt.atomTable.intern(dataSlotString(ctx, file, d.dataIdx)),
                                    regN);
                            }
                            func = regN;
                        } else if(d.op == OP_CALLI) {
                            chunk.emit<inter::GProp>(r2, r3, regT);
                            func = regT;
                        }
                        if(d.imm == -1) { // 省略透传：转发本帧实际实参
                            chunk.emitRaw<inter::CallOmit>(d.r1 == 0 ? regDiscard : r1, func);
                            break;
                        }
                        if(d.imm == -2) { // 展开调用
                            Vec<std::pair<u8, inter::Register>> entries{};
                            entries.reserve(d.fatEntries.size());
                            for(const auto &[type, tjsReg] : d.fatEntries)
                                entries.emplace_back(static_cast<u8>(type), mapReg(tjsReg));
                            chunk.emitRaw<inter::CallExpand>(
                                d.r1 == 0 ? regDiscard : r1, func,
                                Integer{ctx.funcDeclUnnamedArgArrayBase},
                                std::move(entries));
                            break;
                        }
                        for(const i32 arg : d.args)
                            chunk.emit<inter::Push>(mapReg(arg));
                        chunk.emit<inter::Call>(d.r1 == 0 ? regDiscard : r1, func,
                                                Integer{static_cast<i64>(d.imm)});
                        break;
                    }
                    case OP_NEW: {
                        // 构造：类值上 Call（ClassObject::call 创建 DataObject +
                        // initDefaultVal + 构造器），与 TJS CreateNew 对齐
                        if(d.imm == -1) {
                            chunk.emitRaw<inter::CallOmit>(d.r1 == 0 ? regDiscard : r1, r2);
                            break;
                        }
                        if(d.imm == -2) {
                            Vec<std::pair<u8, inter::Register>> entries{};
                            entries.reserve(d.fatEntries.size());
                            for(const auto &[type, tjsReg] : d.fatEntries)
                                entries.emplace_back(static_cast<u8>(type), mapReg(tjsReg));
                            chunk.emitRaw<inter::CallExpand>(
                                d.r1 == 0 ? regDiscard : r1, r2,
                                Integer{ctx.funcDeclUnnamedArgArrayBase},
                                std::move(entries));
                            break;
                        }
                        for(const i32 arg : d.args)
                            chunk.emit<inter::Push>(mapReg(arg));
                        chunk.emit<inter::Call>(d.r1 == 0 ? regDiscard : r1, r2,
                                                Integer{static_cast<i64>(d.imm)});
                        break;
                    }
                    case OP_GPD: case OP_GPDS: {
                        // GPDS(IGNOREPROP) = 不触发 getter 的读 = GPropRaw
                        if(d.r2 == -2) { // obj = ra[-2] proxy：this→global 回落链
                            const Atom nameAtom2 =
                                rt.atomTable.intern(dataSlotString(ctx, file, d.dataIdx));
                            if(d.op == OP_GPD)
                                chunk.emit<inter::GThis>(nameAtom2, r1);
                            else
                                chunk.emit<inter::GRefRaw>(nameAtom2, r1);
                            break;
                        }
                        // 成员名 Atom 直引
                        const Atom nameAtom =
                            rt.atomTable.intern(dataSlotString(ctx, file, d.dataIdx));
                        if(d.op == OP_GPD)
                            chunk.emit<inter::GPropC>(r2, nameAtom, r1);
                        else
                            chunk.emit<inter::GPropRawC>(r2, nameAtom, r1);
                        break;
                    }
                    case OP_SPD: case OP_SPDE: case OP_SPDEH: case OP_SPDS: {
                        // SPDE/SPDEH(MEMBERENSURE)：Ciallang setThis/setProp 天然 ENSURE；
                        // SPDS(IGNOREPROP) = 绕过 setter 写槽位 = DPropRaw
                        if(d.r1 == -2) { // obj = proxy：经 DThis 走 this→global 回落
                            const Atom nameAtom2 =
                                rt.atomTable.intern(dataSlotString(ctx, file, d.dataIdx));
                            if(d.op == OP_SPDS) {
                                // 无 atom 形态的原始写：落 this 槽位（TJS2 proxy 原始写
                                // 在 this 空时落 global 的差异记录为已知近似）
                                chunk.emit<inter::DPropRawC>(
                                    inter::Register{static_cast<u16>(k - 1)}, nameAtom2, r3);
                            } else {
                                chunk.emit<inter::DThis>(nameAtom2, r3);
                            }
                            break;
                        }
                        const Atom nameAtom =
                            rt.atomTable.intern(dataSlotString(ctx, file, d.dataIdx));
                        if(d.op == OP_SPDS)
                            chunk.emit<inter::DPropRawC>(r1, nameAtom, r3);
                        else
                            chunk.emit<inter::DPropC>(r1, nameAtom, r3);
                        break;
                    }
                    case OP_GPI: case OP_GPIS: {
                        if(d.op == OP_GPIS)
                            chunk.emit<inter::GPropRaw>(r2, r3, r1);
                        else
                            chunk.emit<inter::GProp>(r2, r3, r1);
                        break;
                    }
                    case OP_SPI: case OP_SPIE:
                        chunk.emit<inter::DProp>(r1, r2, r3);
                        break;
                    case OP_SPIS:
                        chunk.emit<inter::DPropRaw>(r1, r2, r3);
                        break;
                    case OP_GETP: // dst = *target：强制属性访问读（调 getter）
                        chunk.emit<inter::DerefGet>(r2, r1);
                        break;
                    case OP_SETP: // *target = src：强制属性访问写（调 setter）
                        chunk.emit<inter::DerefSet>(r1, r2);
                        break;
                    case OP_DELD: case OP_DELI: {
                        // dst = delete obj.member（bool）；DELD 成员名 Atom 直引
                        if(d.op == OP_DELD)
                            chunk.emit<inter::DelPropC>(
                                r2, rt.atomTable.intern(dataSlotString(ctx, file, d.dataIdx)), r1);
                        else
                            chunk.emit<inter::DelProp>(r2, r3, r1);
                        break;
                    }
                    case OP_TYPEOFD: case OP_TYPEOFI: {
                        // dst = typeof obj.member。已知近似：TJS 成员不存在时返回
                        // "undefined" 字符串，此处 GProp 直接抛错（其余路径一致）
                        if(d.op == OP_TYPEOFD)
                            chunk.emit<inter::GPropC>(
                                r2, rt.atomTable.intern(dataSlotString(ctx, file, d.dataIdx)), r1);
                        else
                            chunk.emit<inter::GProp>(r2, r3, r1);
                        chunk.emit<inter::TypeOf>(r1, r1);
                        break;
                    }
                    case OP_SRV:
                        srvTjsReg = d.r1;
                        break;
                    case OP_RET:
                        chunk.emit<inter::Ret>(inter::Register{static_cast<u16>(
                            srvTjsReg >= 0 ? srvTjsReg + k : k)}); // 无 SRV → ra[0]=void
                        srvTjsReg = -1;
                        break;
                    case OP_THROW:
                        chunk.emit<inter::Throw>(r1);
                        break;
                    case OP_CHGTHIS:
                        chunk.emit<inter::ChgThis>(r1, r2); // dst=函数, src=this 值
                        break;
                    case OP_GLOBAL:
                        chunk.emit<inter::Global>(r1);
                        break;
                    case OP_ADDCI: case OP_REGMEMBER:
                        trError("class construction opcodes are not supported in M1 (M2)");
                    default:
                        break;
                }
                i += d.size;
            }

            // 尾部兜底 Ret（无 RET 收尾的路径；ra[0]=void）
            chunk.emit<inter::Ret>(inter::Register{static_cast<u16>(k)});

            // 跳转回填（目标必须落在指令边界）
            auto wordToTac = [&](const i32 word) -> i32 {
                for(const auto &[w, t] : offsetMap)
                    if(w == word)
                        return t;
                return -1;
            };
            for(const auto &fix : jumpFixups) {
                const i32 tac = wordToTac(fix.targetWord);
                if(tac < 0)
                    trError("jump target not at instruction boundary");
                switch(fix.kind) {
                    case JumpKind::Jmp: chunk.repl<inter::Jmp>(fix.instIdx, Label{static_cast<size_t>(tac)}); break;
                    case JumpKind::JmpE: chunk.repl<inter::JmpE>(fix.instIdx, Label{static_cast<size_t>(tac)}); break;
                    case JumpKind::JmpNE:
                        chunk.repl<inter::JmpNE>(fix.instIdx,
                                                 Label{static_cast<size_t>(tac)});
                        break;
                }
            }

            // ENTRY 异常处理器登记（tryEnd = catch 地址 = 恢复点）
            for(const auto &entry : entryFixups) {
                const i32 tac = wordToTac(entry.catchWord);
                if(tac < 0)
                    trError("catch target not at instruction boundary");
                chunk.addThrowHandler(
                    {static_cast<u32>(entry.tryStart), static_cast<u32>(tac), entry.exValueReg});
            }

            chunk.setRegCount(regCount);
            auto chunkGuard = rt.create<vm::Chunk>(std::move(chunk));
            auto *chunkPtr = chunkGuard.get();
            rt.addHandleVal(chunkPtr);
            _impl->roots.push_back(chunkPtr);

            auto &out = program.contexts[ci];
            out.parent = ctx.parent;
            out.type = ctx.type;
            out.arity = ctx.funcDeclArgCount;
            out.nameAtom = ctx.name >= 0 ? rt.atomTable.intern(poolString(file, ctx.name))
                                         : Atom{}; // 匿名 context（name=-1）
            out.isFunctionLike = ctx.type == TjsContextType::Function
                              || ctx.type == TjsContextType::ExprFunction
                              || ctx.type == TjsContextType::PropertySetter
                              || ctx.type == TjsContextType::PropertyGetter;
            out.chunk = chunkPtr;
        }

        // Phase B：为函数 context 建 FuncMeta，回填 INTER_OBJECT 占位常量
        for(size_t ci = 0; ci < file.contexts.size(); ++ci) {
            auto &out = program.contexts[ci];
            if(!out.isFunctionLike)
                continue;
            const auto &ctx = file.contexts[ci];
            auto metaGuard = rt.create<FuncMeta>(static_cast<u32>(ctx.funcDeclArgCount),
                                                 out.chunk, Vec<inter::LocalVariable>{});
            auto *meta = metaGuard.get();
            meta->name = out.nameAtom; // super.<Ctor>() 的基类构造器按名匹配依赖此字段
            rt.addHandleVal(meta);
            _impl->roots.push_back(meta);
            out.funcMeta = meta;

            // 类成员：parent 为 ctClass → 注册进其 ClassMeta（REGMEMBER 加载期等价）。
            // 构造器（名 == 类名）只走 setConstructor——与源码路径一致不入成员表，
            // ClassObject::getProp(类名) 才能返回类对象本身，super.<Ctor>() /
            // global.<Ctor>() 直调才能进 ClassObject::call 的 super 构造分支
            if(ctx.parent >= 0
               && program.contexts[static_cast<size_t>(ctx.parent)].isClass) {
                auto &owner = program.contexts[static_cast<size_t>(ctx.parent)];
                if(out.nameAtom == owner.classMeta->className)
                    owner.classMeta->setConstructor(meta);
                else
                    owner.classMeta->setMember(
                        MemberShapeMeta{ .name = out.nameAtom, .isMethod = true },
                        ClassFieldMeta{ meta });
            }
        }

        // Phase B-class-var：类 var 成员 + initDefaultVal chunk（实例 this 上落初值）。
        // 无 var 成员的类也必须产出（initClassFields 对每个类无条件执行
        // initDefaultVal，空缺为 null 会在 new 时解引用空指针）
        for(size_t ci = 0; ci < file.contexts.size(); ++ci) {
            auto &out = program.contexts[ci];
            if(!out.isClass)
                continue;
            vm::Chunk initChunk{}; // 常量 0 = Void
            for(const auto &[nameAtom, slot] : classVarsPerCtx[ci]) {
                switch(slot.type) {
                    case TJS_TYPE_VOID:
                        initChunk.emit<inter::Load>(inter::Register{0}, ConstIdx{0});
                        break;
                    case TJS_TYPE_BYTE:
                        initChunk.emit<inter::LoadImm>(
                            inter::Register{0},
                            Integer{static_cast<i64>(poolByte(file, slot.index))});
                        break;
                    case TJS_TYPE_SHORT:
                        initChunk.emit<inter::LoadImm>(
                            inter::Register{0},
                            Integer{static_cast<i64>(poolShort(file, slot.index))});
                        break;
                    case TJS_TYPE_INTEGER:
                        initChunk.emit<inter::LoadImm>(
                            inter::Register{0},
                            Integer{static_cast<i64>(poolInt(file, slot.index))});
                        break;
                    case TJS_TYPE_LONG:
                        initChunk.emit<inter::LoadImm>(inter::Register{0},
                                                       Integer{poolLong(file, slot.index)});
                        break;
                    case TJS_TYPE_REAL:
                        initChunk.emit<inter::Load>(
                            inter::Register{0},
                            initChunk.addConstant(Constant{Real{poolReal(file, slot.index)}}));
                        break;
                    case TJS_TYPE_STRING:
                        initChunk.emit<inter::Load>(
                            inter::Register{0},
                            initChunk.addConstant(
                                Constant{rt.atomTable.intern(poolString(file, slot.index))}));
                        break;
                    default:
                        trError("class var initializer slot type not supported");
                }
                initChunk.emit<inter::DThis>(nameAtom, inter::Register{0});
                out.classMeta->setMember(
                    MemberShapeMeta{ .name = nameAtom, .isVar = true },
                    ClassFieldMeta{});
            }
            // initClassFields 契约：块以 Ret 结尾（空 var 表也要有，run0 不读越界码）
            initChunk.emit<inter::Ret>(inter::Register{0});
            auto initChunkGuard = rt.create<vm::Chunk>(std::move(initChunk));
            auto *initChunkPtr = initChunkGuard.get();
            auto initMetaGuard = rt.create<FuncMeta>(0, initChunkPtr, Vec<inter::LocalVariable>{});
            auto *initMeta = initMetaGuard.get();
            initChunkPtr->setRegCount(1); // 仅用 %0
            rt.addHandleVal(initChunkPtr);
            rt.addHandleVal(initMeta);
            _impl->roots.push_back(initChunkPtr);
            _impl->roots.push_back(initMeta);
            out.classMeta->initDefaultVal = initMeta;
        }

        // Phase B-prop：property ctx 装配 PropMeta（getter/setter 子 context）
        for(size_t ci = 0; ci < file.contexts.size(); ++ci) {
            auto &out = program.contexts[ci];
            if(!out.isProperty)
                continue;
            FuncMeta *setter = nullptr;
            FuncMeta *getter = nullptr;
            for(size_t k = 0; k < file.contexts.size(); ++k) {
                const auto &child = file.contexts[k];
                if(child.parent != static_cast<i32>(ci))
                    continue;
                auto &childOut = program.contexts[k];
                if(child.type == TjsContextType::PropertySetter)
                    setter = childOut.funcMeta;
                else if(child.type == TjsContextType::PropertyGetter)
                    getter = childOut.funcMeta;
            }
            auto propMetaGuard = rt.create<PropMeta>(setter, getter);
            auto *propMeta = propMetaGuard.get();
            rt.addHandleVal(propMeta);
            _impl->roots.push_back(propMeta);
            out.propMeta = propMeta;

            // 类成员 property：注册进父 ClassMeta（initClassFields 按实例建
            // 绑定 this 的 Property 对象）；顶层 property 由顶层码字 SPDS 注册
            if(out.parent >= 0) {
                auto &owner = program.contexts[static_cast<size_t>(out.parent)];
                if(owner.isClass)
                    owner.classMeta->setMember(
                        MemberShapeMeta{ .name = out.nameAtom, .isProp = true },
                        ClassFieldMeta{ propMeta });
            }
        }

        // Phase B'：INTER_OBJECT 占位回填（FuncMeta/ClassMeta/PropMeta）
        for(size_t ci = 0; ci < deferredPerCtx.size(); ++ci) {
            for(const auto &[constIdx, targetCtx] : deferredPerCtx[ci]) {
                auto &target = program.contexts[static_cast<size_t>(targetCtx)];
                if(target.funcMeta)
                    program.contexts[ci].chunk->getConstants()[constIdx] =
                        Constant{target.funcMeta};
                else if(target.classMeta)
                    program.contexts[ci].chunk->getConstants()[constIdx] =
                        Constant{target.classMeta};
                else if(target.propMeta)
                    program.contexts[ci].chunk->getConstants()[constIdx] =
                        Constant{target.propMeta};
                else
                    trError("INTER_OBJECT references an unsupported context");
            }
        }

        return program;
    }

} // namespace cial::tjsbc
