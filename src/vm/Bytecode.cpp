//
// Created by LiDong on 2026/1/27.
//

#include "Bytecode.hpp"

#include <fmt/format.h>

#include "VMState.hpp"
#include "types/Property.hpp"

namespace cial::vm {
    void Bytecode::visit(const inter::NOP *) { _code.push_back(static_cast<u8>(VmOpCode::NOP)); }

    void Bytecode::visit(const inter::Debugger *) { _code.push_back(static_cast<u8>(VmOpCode::Debugger)); }

    void Bytecode::visit(const inter::Push *tac) {
        const u16 dst = tac->src().index();
        _code.push_back(static_cast<u8>(VmOpCode::Push));
        writeU16(_code, dst);
    }

    void Bytecode::visit(const inter::PopN *tac) {
        const i64 imm = tac->getOp1Val<Integer>();
        _code.push_back(static_cast<u8>(VmOpCode::PopN));
        encodeSleb128(imm, _code);
    }

    void Bytecode::visit(const inter::Global *tac) { assert(false); }
    void Bytecode::visit(const inter::Super *tac) { assert(false); }
    void Bytecode::visit(const inter::This *tac) { assert(false); }

    void Bytecode::visit(const inter::Test *tac) {
        const u16 r = tac->src().index();
        _code.push_back(static_cast<u8>(VmOpCode::Test));
        writeU16(_code, r);
    }

    void Bytecode::visit(const inter::Inv *tac) { assert(false); }
    void Bytecode::visit(const inter::Throw *tac) { assert(false); }

    void Bytecode::visit(const inter::Ret *tac) {
        const u16 r1 = tac->src().index();
        _code.push_back(static_cast<u8>(VmOpCode::Ret));
        writeU16(_code, r1);
    }

    void Bytecode::visit(const inter::Jmp *tac) { assert(false); }
    void Bytecode::visit(const inter::JmpE *tac) { assert(false); }

    void Bytecode::visit(const inter::JmpNE *tac) {
        _code.push_back(static_cast<u8>(VmOpCode::JmpNE));
        _tacPosToBcPos[static_cast<u32>(tac->label().address())] = _code.size();
        writeU32(_code, 0);
    }

    void Bytecode::visit(const inter::ToInt *tac) { assert(false); }
    void Bytecode::visit(const inter::ToReal *tac) { assert(false); }
    void Bytecode::visit(const inter::ToString *tac) { assert(false); }

    void Bytecode::visit(const inter::LNot *tac) { assert(false); }

    void Bytecode::visit(const inter::ChgSign *tac) { assert(false); }
    void Bytecode::visit(const inter::ChgThis *tac) { assert(false); }

    void Bytecode::visit(const inter::ChkInv *tac) { assert(false); }
    void Bytecode::visit(const inter::ChkIns *tac) { assert(false); }

    void Bytecode::visit(const inter::Load *tac) {
        const u16 dst = tac->dst().index();
        const u16 val = tac->value().index();
        _code.push_back(static_cast<u8>(VmOpCode::Load));
        writeU16(_code, dst);
        writeU16(_code, val);
    }

    void Bytecode::visit(const inter::LoadImm *tac) {
        const u16 dst = tac->dst().index();
        const i64 val = tac->value();
        _code.push_back(static_cast<u8>(VmOpCode::LoadImm));
        writeU16(_code, dst);
        encodeSleb128(val, _code);
    }

    void Bytecode::visit(const inter::DGlobal *tac) {
        const u32 atom = static_cast<u32>(tac->atom().v);
        const u16 dst = tac->src().index();
        _code.push_back(static_cast<u8>(VmOpCode::DGlobal));
        writeU32(_code, atom);
        writeU16(_code, dst);
    }

    void Bytecode::visit(const inter::GGlobal *tac) {
        const u32 atom = static_cast<u32>(tac->atom().v);
        const u16 dst = tac->dst().index();
        _code.push_back(static_cast<u8>(VmOpCode::GGlobal));
        writeU32(_code, atom);
        writeU16(_code, dst);
    }

    void Bytecode::visit(const inter::GThis *tac) {
        const u32 atom = static_cast<u32>(tac->atom().v);
        const u16 dst = tac->dst().index();
        _code.push_back(static_cast<u8>(VmOpCode::GThis));
        writeU32(_code, atom);
        writeU16(_code, dst);
    }
    void Bytecode::visit(const inter::DThis *tac) { assert(false); }

    void Bytecode::visit(const inter::Mov *tac) { assert(false); }

    void Bytecode::visit(const inter::CP *tac) {
        const u16 r1 = tac->src().index();
        const u16 r2 = tac->dst().index();
        _code.push_back(static_cast<u8>(VmOpCode::CP));
        writeU16(_code, r1);
        writeU16(_code, r2);
    }

    void Bytecode::visit(const inter::Add *tac) {
        const u16 r1 = tac->src1().index();
        const u16 r2 = tac->src2().index();
        const u16 r3 = tac->dst().index();
        if(r2 == r3) {
            _code.push_back(static_cast<u8>(VmOpCode::Add));
            writeU16(_code, r1);
            writeU16(_code, r3);
        } else {
            _code.push_back(static_cast<u8>(VmOpCode::Mov));
            writeU16(_code, r2);
            writeU16(_code, r3);

            _code.push_back(static_cast<u8>(VmOpCode::Add));
            writeU16(_code, r1);
            writeU16(_code, r3);
        }
    }

    void Bytecode::visit(const inter::Sub *tac) {
        const u16 r1 = tac->src1().index();
        const u16 r2 = tac->src2().index();
        const u16 r3 = tac->dst().index();
        if(r2 != r3) {
            _code.push_back(static_cast<u8>(VmOpCode::Mov));
            writeU16(_code, r2);
            writeU16(_code, r3);
        }
        _code.push_back(static_cast<u8>(VmOpCode::Sub));
        writeU16(_code, r1);
        writeU16(_code, r3);
    }

    void Bytecode::visit(const inter::Mul *tac) { assert(false); }
    void Bytecode::visit(const inter::Div *tac) { assert(false); }
    void Bytecode::visit(const inter::Idiv *tac) { assert(false); }
    void Bytecode::visit(const inter::Mod *tac) { assert(false); }

    void Bytecode::visit(const inter::BXor *tac) { assert(false); }
    void Bytecode::visit(const inter::BOr *tac) { assert(false); }
    void Bytecode::visit(const inter::BAnd *tac) { assert(false); }
    void Bytecode::visit(const inter::BlShift *tac) { assert(false); }
    void Bytecode::visit(const inter::BrShift *tac) { assert(false); }
    void Bytecode::visit(const inter::BurShift *tac) { assert(false); }

    void Bytecode::visit(const inter::EQ *tac) { assert(false); }
    void Bytecode::visit(const inter::NEQ *tac) { assert(false); }
    void Bytecode::visit(const inter::AbsEQ *tac) { assert(false); }
    void Bytecode::visit(const inter::AbsNEQ *tac) { assert(false); }

    void Bytecode::visit(const inter::LT *tac) {
        const u16 r1 = tac->src1().index();
        const u16 r2 = tac->src2().index();
        const u16 r3 = tac->dst().index();
        if(r2 == r3) {
            _code.push_back(static_cast<u8>(VmOpCode::LT));
            writeU16(_code, r1);
            writeU16(_code, r3);
        } else {
            _code.push_back(static_cast<u8>(VmOpCode::Mov));
            writeU16(_code, r2);
            writeU16(_code, r3);

            _code.push_back(static_cast<u8>(VmOpCode::LT));
            writeU16(_code, r1);
            writeU16(_code, r3);
        }
    }

    void Bytecode::visit(const inter::LE *tac) { assert(false); }
    void Bytecode::visit(const inter::GT *tac) { assert(false); }
    void Bytecode::visit(const inter::GE *tac) { assert(false); }
    void Bytecode::visit(const inter::LAnd *tac) { assert(false); }
    void Bytecode::visit(const inter::LOr *tac) { assert(false); }

    void Bytecode::visit(const inter::Call *tac) {
        const u16 r1 = tac->dst().index();
        const u16 r2 = tac->memberReg().index();
        const i64 imm = tac->argCount();
        _code.push_back(static_cast<u8>(VmOpCode::Call));
        writeU16(_code, r1);
        writeU16(_code, r2);
        encodeSleb128(imm, _code);
    }

    void Bytecode::visit(const inter::GProp *tac) { assert(false); }
    void Bytecode::visit(const inter::DProp *tac) { assert(false); }


    static bool propObjectSet(VMState &vmState, const Value &src, const Value &dst) {
        if(dst.isObject()) {
            if(const auto *prop = dynamic_cast<Property *>(dst.asObject().value())) {
                prop->invokeSet(vmState, src);
                return true;
            }
        }
        return false;
    }

    static bool propObjectGet(VMState &vmState, const Value &src, Value &dst) {
        if(src.isObject()) {
            if(const auto object = src.asObject().unwrap()) {
                if(const auto *prop = dynamic_cast<Property *>(object)) {
                    dst = prop->invokeGet(vmState);
                    return true;
                }
            }
        }
        return false;
    }

    size_t Bytecode::dispatch(VMState *vmState) const {
        if(vmState->getPC() >= _code.size())
            return 0;

        // auto instTable = vmState->curFrame()->chunk->dumpInstructions().toStdStr();
        u16 count = 1;
        const u8 *pc = _code.data() + vmState->getPC();
        const size_t remaining{ _code.size() - vmState->getPC() };
        const auto op = static_cast<VmOpCode>(*pc++);

#define MV_CUR(n)                                                                                                      \
    do {                                                                                                               \
        count += n;                                                                                                    \
        pc += n;                                                                                                       \
    } while(false)

#define CHECK(n)                                                                                                       \
    do {                                                                                                               \
        if(remaining < n)                                                                                              \
            throw std::runtime_error(fmt::format("truncated opcode: {}", static_cast<u8>(op)));                        \
    } while(false)

        switch(op) {
            case VmOpCode::NOP:
                break;

            case VmOpCode::Load: {
                CHECK(4);
                const u16 r1 = readU16(pc);
                MV_CUR(2);
                const u16 cIdx = readU16(pc);
                MV_CUR(2);
                vmState->regRef(r1) =
                    vmState->curFrame()->chunk->getConstant(ConstIdx{ cIdx }).createValue(&vmState->rt);
                break;
            }
            case VmOpCode::LoadImm: {
                CHECK(2);
                const u16 r1 = readU16(pc);
                MV_CUR(2);
                const auto [imm, len] = decodeSleb128(pc, remaining - count);
                MV_CUR(len);
                vmState->regRef(r1) = Value{ imm };
                break;
            }
            case VmOpCode::Push: {
                CHECK(2);
                const u16 r1 = readU16(pc);
                MV_CUR(2);
                vmState->push(vmState->reg(r1));
                break;
            }
            case VmOpCode::PopN: {
                assert(false);
            }
            case VmOpCode::CP: {
                CHECK(4);
                const u16 r1 = readU16(pc);
                MV_CUR(2);
                const u16 r2 = readU16(pc);
                MV_CUR(2);
                Value srcVal = vmState->reg(r1);
                propObjectGet(*vmState, srcVal, srcVal);
                vmState->regRef(r2) = srcVal;
                break;
            }

            case VmOpCode::Add: {
                CHECK(4);
                const u16 r1 = readU16(pc);
                MV_CUR(2);
                const u16 r2 = readU16(pc);
                MV_CUR(2);
                const Value r1v = vmState->reg(r1);
                Value &r2v = vmState->regRef(r2);
                r2v = r1v.add(r2v).unwrap();
                break;
            }
            case VmOpCode::Sub: {
                CHECK(4);
                const u16 r1 = readU16(pc);
                MV_CUR(2);
                const u16 r2 = readU16(pc);
                MV_CUR(2);
                const Value r1v = vmState->reg(r1);
                Value &r2v = vmState->regRef(r2);
                r2v = r1v.sub(r2v).unwrap();
                break;
            }
            case VmOpCode::Mul: {
                assert(false);
            }
            case VmOpCode::Div: {
                assert(false);
            }
            case VmOpCode::Idiv: {
                assert(false);
            }
            case VmOpCode::Mod: {
                assert(false);
            }
            case VmOpCode::Mov: {
                CHECK(4);
                const u16 r1 = readU16(pc);
                MV_CUR(2);
                const u16 r2 = readU16(pc);
                MV_CUR(2);
                Value srcVal = vmState->reg(r1);
                if(!propObjectSet(*vmState, srcVal, vmState->regRef(r2))) {
                    vmState->regRef(r2) = srcVal;
                }
                break;
            }
            case VmOpCode::DGlobal: {
                CHECK(6);
                const u32 atom = readU32(pc);
                MV_CUR(4);
                const u16 r1 = readU16(pc);
                MV_CUR(2);
                const Value &srcVal = vmState->regRef(r1);
                if(vmState->globalHas(Atom{ atom }) && propObjectSet(*vmState, srcVal, vmState->global(Atom{ atom }))) {
                    break;
                }
                vmState->global(Atom{ atom }, Value{ srcVal });
                break;
            }
            case VmOpCode::GGlobal: {
                CHECK(6);
                const u32 atom = readU32(pc);
                MV_CUR(4);
                const u16 r1 = readU16(pc);
                MV_CUR(2);
                Value srcVal = vmState->global(Atom{ atom });
                propObjectGet(*vmState, srcVal, srcVal);
                vmState->regRef(r1) = srcVal;
                break;
            }
            case VmOpCode::Global: {
                assert(false);
            }
            case VmOpCode::Super: {
                assert(false);
            }
            case VmOpCode::This: {
                assert(false);
            }
            case VmOpCode::ToInt: {
                assert(false);
            }
            case VmOpCode::ToReal: {
                assert(false);
            }
            case VmOpCode::ToString: {
                assert(false);
            }
            case VmOpCode::ChgThis: {
                assert(false);
            }
            case VmOpCode::Inv: {
                assert(false);
            }
            case VmOpCode::ChkInv: {
                assert(false);
            }
            case VmOpCode::ChkIns: {
                assert(false);
            }
            case VmOpCode::Test: {
                CHECK(2);
                const u16 r1 = readU16(pc);
                MV_CUR(2);
                vmState->setZF(vmState->reg(r1).asBool());
                break;
            }
            case VmOpCode::EQ: {
                assert(false);
            }
            case VmOpCode::NEQ: {
                assert(false);
            }
            case VmOpCode::LT: {
                CHECK(4);
                const u16 r1 = readU16(pc);
                MV_CUR(2);
                const u16 r2 = readU16(pc);
                MV_CUR(2);
                const Value r1v = vmState->reg(r1);
                Value &r2v = vmState->regRef(r2);
                const bool r = r1v.littlerThan(r2v).unwrap();
                r2v = Value{ r };
                break;
            }
            case VmOpCode::LE: {
                assert(false);
            }
            case VmOpCode::GT: {
                assert(false);
            }
            case VmOpCode::GE: {
                assert(false);
            }
            case VmOpCode::AbsEQ: {
                assert(false);
            }
            case VmOpCode::AbsNEQ: {
                assert(false);
            }
            case VmOpCode::Jmp: {
                assert(false);
            }
            case VmOpCode::JmpE: {
                assert(false);
            }
            case VmOpCode::JmpNE: {
                CHECK(4);
                const u32 addr = readU32(pc);
                MV_CUR(4);
                if(!vmState->getZF()) {
                    vmState->setPC0(addr);
                    return 0;
                }
                break;
            }
            case VmOpCode::Call: {
                CHECK(4);
                const u16 r1 = readU16(pc);
                MV_CUR(2);
                const u16 r2 = readU16(pc);
                MV_CUR(2);
                auto [imm, len] = decodeSleb128(pc, remaining - count);
                MV_CUR(len);
                const auto &object = vmState->reg(r2).asObject().unwrap();
                vmState->setPC0(vmState->getPC() + count);
                object->call(*vmState, r1, imm);
                return 0;
            }
            case VmOpCode::GProp: {
                assert(false);
            }
            case VmOpCode::DProp: {
                assert(false);
            }
            case VmOpCode::GThis: {
                CHECK(6);
                const u32 atom = readU32(pc);
                MV_CUR(4);
                const u16 r1 = readU16(pc);
                MV_CUR(2);
                Value srcVal = vmState->getThis({ atom });
                propObjectGet(*vmState, srcVal, srcVal);
                vmState->regRef(r1) = srcVal;
                break;
            }
            case VmOpCode::DThis: {
                assert(false);
            }
            case VmOpCode::LNot: {
                assert(false);
            }
            case VmOpCode::LAnd: {
                assert(false);
            }
            case VmOpCode::LOr: {
                assert(false);
            }
            case VmOpCode::BXor: {
                assert(false);
            }
            case VmOpCode::BOr: {
                assert(false);
            }
            case VmOpCode::BAnd: {
                assert(false);
            }
            case VmOpCode::BlShift: {
                assert(false);
            }
            case VmOpCode::BrShift: {
                assert(false);
            }
            case VmOpCode::BurShift: {
                assert(false);
            }
            case VmOpCode::ChgSign: {
                assert(false);
            }
            case VmOpCode::Debugger: {
                assert(false);
            }
            case VmOpCode::Throw: {
                assert(false);
            }
            case VmOpCode::Ret: {
                CHECK(2);
                const u16 r1 = readU16(pc);
                MV_CUR(2);
                const auto value = vmState->reg(r1);
                const auto *frame = vmState->curFrame();
                CLL_ASSERT(frame->ret, "frame.ret val is empty");
                vmState->prevFrame()->getReg(*frame->ret) = value;
                vmState->freeCallFrame();
                return 0;
            }
        }
        return count;
    }
} // namespace cial::vm
