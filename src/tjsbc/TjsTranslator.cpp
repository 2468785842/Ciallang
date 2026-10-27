// Copyright (c) 2026/10/26
//

#include "TjsTranslator.hpp"

#include <cstring>

#include "gen/Label.hpp"
#include <cstdio>
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
                return k4 == 0 ? 3 : 5;
            }
            return 1;
        }

        // 解码后的单条 TJS 指令（操作数已转 TJS 寄存器 r / 数据下标）
        struct Decoded {
            i32 op{};
            i32 word{};        // 指令首码字偏移
            i32 r1 = 0, r2 = 0, r3 = 0, r4 = 0;
            i32 dataIdx = -1;  // CONST/PD/CALLD 的常量槽下标
            i32 imm = 0;       // CCL 计数 / 跳转相对字偏移 / CALL 实参数
            Vec<i32> args{};   // CALL 族实参寄存器（TJS r）
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
                case OP_TYPEOF: case OP_CL:
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
                case OP_CALL: case OP_NEW: {
                    d.r1 = reg(1);
                    d.r2 = reg(2);
                    d.imm = reg(3);
                    if(d.imm < 0)
                        trError("collapsed/expanded calls are not supported in M1 translator");
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
                    if(d.imm < 0)
                        trError("collapsed/expanded calls are not supported in M1 translator");
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
                trError("class construction opcodes are not supported in M1 (M2)");
            if(d.op >= OP_INC && d.op <= OP_MUL) {
                const i32 k4 = (d.op - OP_INC) % 4;
                if(k4 == 0) {
                    d.r1 = reg(1);
                    d.r2 = reg(2);
                    return d;
                }
                trError("property arithmetic variants are not supported in M1 translator: opcode "
                        + std::to_string(d.op));
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
            if(d.op == OP_DELD || d.op == OP_DELI || d.op == OP_TYPEOFD
               || d.op == OP_TYPEOFI)
                trError("delete/typeof member are not supported in M1 translator");
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

        for(size_t ci = 0; ci < file.contexts.size(); ++ci) {
            deferredPerCtx.emplace_back();
            const TjsContext &ctx = file.contexts[ci];
            if(ctx.type == TjsContextType::Class)
                trError("class contexts are not supported in M1 translator (M2)");
            if(ctx.type == TjsContextType::Property
               || ctx.type == TjsContextType::PropertySetter
               || ctx.type == TjsContextType::PropertyGetter)
                trError("property contexts are not supported in M1 translator (M2)");
            if(ctx.type == TjsContextType::SuperClassGetter)
                trError("superClassGetter contexts are not supported in M1 translator (M2/M4)");
            if(ctx.funcDeclCollapseBase >= 0)
                trError("collapse parameters (*args) are not supported");

            // ra 基偏移恒等平移：K = VC+RC，但实参区 ra[-3-i] 要求 K ≥ 2+argc，
            // 多参函数取 max 修正（如 g(a,b) 的 %-4 越出 VC+RC=3）
            i32 k = ctx.maxVariableCount + ctx.variableReserveCount;
            if(k < ctx.funcDeclArgCount + 2)
                k = ctx.funcDeclArgCount + 2;
            const auto regF = inter::Register{static_cast<u16>(k + ctx.maxFrameCount + 1)}; // 标志物化
            const auto regT = inter::Register{static_cast<u16>(regF.index() + 1)};          // 临时
            const auto regN = inter::Register{static_cast<u16>(regF.index() + 2)};          // 名字常量临时
            const u16 regCount = static_cast<u16>(regN.index() + 1 + ctx.funcDeclArgCount + 1);

            vm::Chunk chunk{}; // 常量 0 = Void

            auto mapReg = [&](const i32 r) { return inter::Register{static_cast<u16>(r + k)}; };
            auto atomConst = [&](const String &s) {
                return chunk.addConstant(Constant{rt.atomTable.intern(s)});
            };

            // prologue：ra[-2]≈global、ra[-1]=this、ra[0]=void；函数搬实参
            chunk.emit<inter::Global>(inter::Register{static_cast<u16>(k - 2 >= 0 ? k - 2 : 0)});
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

            const i32 tacBase = static_cast<i32>(chunk.getInstVec().size());
            Vec<std::pair<i32, i32>> offsetMap{}; // 字偏移 → TAC 下标
            Vec<JumpFixup> jumpFixups{};
            Vec<EntryFixup> entryFixups{};
            i32 srvTjsReg = -1;

            // Pass 1 + 2 合流：解码即发射，跳转经 fixup 后填
            i32 i = 0;
            while(i < static_cast<i32>(ctx.code.size())) {
                Decoded d = decode(ctx.code, i);
                fprintf(stderr, "[TR] i=%d op=%d\n", i, d.op);
                offsetMap.emplace_back(d.word, static_cast<i32>(chunk.getInstVec().size()));
                const auto r1 = mapReg(d.r1);
                const auto r2 = mapReg(d.r2);
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
                                // 指针回填 FuncMeta*——此处 chunk 仍在栈上不可持指针
                                const u16 cidx =
                                    static_cast<u16>(chunk.addConstant(Constant{}).index());
                                deferredPerCtx[ci].emplace_back(cidx, slot.index);
                                chunk.emit<inter::Load>(r1, ConstIdx{cidx});
                                break;
                            }
                            case TJS_TYPE_OCTET:
                                trError("octet constants are not supported in M1 translator");
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
                    case OP_CLT: // TJS CLT = r1 > r2（编译器换序，直译不纠正）
                        chunk.emit<inter::GT>(r1, r2, regF);
                        break;
                    case OP_CGT:
                        chunk.emit<inter::LT>(r1, r2, regF);
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
                            const JumpKind kind =
                                d.op == OP_JF ? JumpKind::JmpNE : JumpKind::JmpE;
                            jumpFixups.push_back(
                                {chunk.getInstVec().size(), static_cast<size_t>(target), kind});
                            chunk.emit<inter::JmpNE>(Label{0}); // 占位，repl 时按 kind 修正
                        }
                        break;
                    }
                    case OP_CHKINS:
                        chunk.emit<inter::DLocal>(r2, regT);
                        chunk.emit<inter::ChkIns>(regT, r1);
                        break;
                    case OP_INC: case OP_DEC: {
                        chunk.emit<inter::LoadImm>(regT, Integer{1});
                        if(d.op == OP_INC)
                            chunk.emit<inter::Add>(r1, regT, r1);
                        else
                            chunk.emit<inter::Sub>(r1, regT, r1);
                        break;
                    }
                    case OP_LOR: case OP_LAND: case OP_BOR: case OP_BXOR:
                    case OP_BAND: case OP_SAR: case OP_SAL: case OP_SR:
                    case OP_ADD: case OP_SUB: case OP_MOD: case OP_DIV:
                    case OP_IDIV: case OP_MUL: {
                        const i32 base = d.op - (d.op - OP_LOR) % 4;
                        switch(base) {
                            case OP_LOR: chunk.emit<inter::LOr>(r1, r2, r1); break;
                            case OP_LAND: chunk.emit<inter::LAnd>(r1, r2, r1); break;
                            case OP_BOR: chunk.emit<inter::BOr>(r1, r2, r1); break;
                            case OP_BXOR: chunk.emit<inter::BXor>(r1, r2, r1); break;
                            case OP_BAND: chunk.emit<inter::BAnd>(r1, r2, r1); break;
                            case OP_SAR: chunk.emit<inter::BrShift>(r1, r2, r1); break;
                            case OP_SAL: chunk.emit<inter::BlShift>(r1, r2, r1); break;
                            case OP_SR: chunk.emit<inter::BurShift>(r1, r2, r1); break;
                            case OP_ADD: chunk.emit<inter::Add>(r1, r2, r1); break;
                            case OP_SUB: chunk.emit<inter::Sub>(r1, r2, r1); break;
                            case OP_MOD: chunk.emit<inter::Mod>(r1, r2, r1); break;
                            case OP_DIV: chunk.emit<inter::Div>(r1, r2, r1); break;
                            case OP_IDIV: chunk.emit<inter::Idiv>(r1, r2, r1); break;
                            case OP_MUL: chunk.emit<inter::Mul>(r1, r2, r1); break;
                            default: break;
                        }
                        break;
                    }
                    case OP_BNOT: case OP_NUM:
                        trError("opcode not supported in M1 translator");
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
                        trError("OCTET conversion is not supported in M1 translator");
                    case OP_CALL: case OP_CALLD: case OP_CALLI: {
                        inter::Register func = r2;
                        if(d.op == OP_CALLD) {
                            chunk.emit<inter::Load>(regN,
                                                    atomConst(dataSlotString(ctx, file, d.dataIdx)));
                            chunk.emit<inter::GProp>(r2, regN, regN);
                            func = regN;
                        } else if(d.op == OP_CALLI) {
                            chunk.emit<inter::GProp>(r2, r3, regT);
                            func = regT;
                        }
                        for(const i32 arg : d.args)
                            chunk.emit<inter::Push>(mapReg(arg));
                        chunk.emit<inter::Call>(r1, func,
                                                Integer{static_cast<i64>(d.imm)});
                        break;
                    }
                    case OP_NEW:
                        trError("NEW is not supported in M1 translator (class contexts are M2)");
                    case OP_GPD: case OP_GPDS: {
                        // GPDS(IGNOREPROP) = 不触发 getter 的读 = GPropRaw
                        chunk.emit<inter::Load>(regN,
                                                atomConst(dataSlotString(ctx, file, d.dataIdx)));
                        if(d.op == OP_GPD)
                            chunk.emit<inter::GProp>(r2, regN, r1);
                        else
                            chunk.emit<inter::GPropRaw>(r2, regN, r1);
                        break;
                    }
                    case OP_SPD: case OP_SPDE: case OP_SPDEH: case OP_SPDS: {
                        // SPDE/SPDEH(MEMBERENSURE)：Ciallang setProp 天然 ENSURE；
                        // SPDS(IGNOREPROP) = 绕过 setter 写槽位 = DPropRaw
                        chunk.emit<inter::Load>(regN,
                                                atomConst(dataSlotString(ctx, file, d.dataIdx)));
                        if(d.op == OP_SPDS)
                            chunk.emit<inter::DPropRaw>(r1, regN, r3);
                        else
                            chunk.emit<inter::DProp>(r1, regN, r3);
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
                    case OP_SETP: case OP_GETP:
                        trError("property closures (SETP/GETP) are not supported in M1 translator");
                    case OP_DELD: case OP_DELI: case OP_TYPEOFD: case OP_TYPEOFI:
                        trError("delete/typeof direct-indirect are not supported in M1 translator");
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
            out.isFunctionLike = ctx.type == TjsContextType::Function
                              || ctx.type == TjsContextType::ExprFunction;
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
            rt.addHandleVal(meta);
            _impl->roots.push_back(meta);
            out.funcMeta = meta;
        }

        // Phase B'：INTER_OBJECT 占位回填 FuncMeta*（chunk 已全部入 GC）
        for(size_t ci = 0; ci < deferredPerCtx.size(); ++ci) {
            for(const auto &[constIdx, targetCtx] : deferredPerCtx[ci]) {
                auto &target = program.contexts[static_cast<size_t>(targetCtx)];
                if(target.funcMeta == nullptr)
                    trError("INTER_OBJECT references a non-function context (M2: classes)");
                program.contexts[ci].chunk->getConstants()[constIdx] =
                    Constant{target.funcMeta};
            }
        }

        return program;
    }

} // namespace cial::tjsbc
