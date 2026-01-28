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

    DecodedInst Bytecode::decode(u64 &pc) const {
#define CHECK(n)                                                                                                       \
    do {                                                                                                               \
        if(remaining < n)                                                                                              \
            throw std::runtime_error("truncated");                                                                     \
    } while(false)

        const size_t remaining{ _code.size() - pc };

        DecodedInst inst{ .op = static_cast<VmOpCode>(_code[pc++]) };

        switch(inst.op) {
            case VmOpCode::NOP:
            case VmOpCode::Debugger:
                break;
            case VmOpCode::PopN:
                inst.imm = decodeSleb128(_code, pc, remaining - 1);
                break;

            case VmOpCode::Push:
            case VmOpCode::Global:
            case VmOpCode::Super:
            case VmOpCode::This:
            case VmOpCode::Test:
            case VmOpCode::Inv:
            case VmOpCode::Throw:
            case VmOpCode::Ret:
                CHECK(2);
                inst.r1 = readU16(_code, pc);
                break;

            case VmOpCode::Jmp:
            case VmOpCode::JmpE:
            case VmOpCode::JmpNE:
                CHECK(4);
                inst.target = readU32(_code, pc);
                break;

            case VmOpCode::Load:
                CHECK(4);
                inst.r1 = readU16(_code, pc);
                inst.cIdx = readU16(_code, pc);
                break;

            case VmOpCode::LoadImm:
                CHECK(2);
                inst.r1 = readU16(_code, pc);
                inst.imm = decodeSleb128(_code, pc, remaining - 3);
                break;

            case VmOpCode::DGlobal:
            case VmOpCode::GGlobal:
            case VmOpCode::GThis:
            case VmOpCode::DThis:
                CHECK(6);
                inst.atom = readU32(_code, pc);
                inst.r1 = readU16(_code, pc);
                break;

            case VmOpCode::ToInt:
            case VmOpCode::ToReal:
            case VmOpCode::ToString:
            case VmOpCode::LNot:
            case VmOpCode::ChgSign:
            case VmOpCode::ChgThis:
            case VmOpCode::ChkInv:
            case VmOpCode::ChkIns:
            case VmOpCode::Mov:
            case VmOpCode::CP:
            case VmOpCode::Add:
            case VmOpCode::Sub:
            case VmOpCode::Mul:
            case VmOpCode::Div:
            case VmOpCode::Idiv:
            case VmOpCode::Mod:
            case VmOpCode::BXor:
            case VmOpCode::BOr:
            case VmOpCode::BAnd:
            case VmOpCode::BlShift:
            case VmOpCode::BrShift:
            case VmOpCode::BurShift:
            case VmOpCode::EQ:
            case VmOpCode::NEQ:
            case VmOpCode::AbsEQ:
            case VmOpCode::AbsNEQ:
            case VmOpCode::LT:
            case VmOpCode::LE:
            case VmOpCode::GT:
            case VmOpCode::GE:
            case VmOpCode::LAnd:
            case VmOpCode::LOr:
                CHECK(4);
                inst.r1 = readU16(_code, pc);
                inst.r2 = readU16(_code, pc);
                break;

            case VmOpCode::Call:
                CHECK(4);
                inst.r1 = readU16(_code, pc);
                inst.r2 = readU16(_code, pc);
                inst.imm = decodeSleb128(_code, pc, remaining - 5);
                break;

            case VmOpCode::GProp:
            case VmOpCode::DProp:
                CHECK(4);
                inst.r1 = readU16(_code, pc);
                inst.r2 = readU16(_code, pc);
                inst.r3 = readU16(_code, pc);
                break;
        }
        return inst;
    }

    void Bytecode::dispatch(const DecodedInst &decodedInst, VMState *vmState) {
        // auto instTable = vmState->curFrame()->chunk->dumpInstructions().toStdStr();
        switch(decodedInst.op) {
            case VmOpCode::NOP: {
                break;
            }
            case VmOpCode::Debugger: {
                assert(false);
                break;
            }
            case VmOpCode::Push: {
                vmState->push(vmState->reg(decodedInst.r1));
                break;
            }
            case VmOpCode::PopN: {
                assert(false);
                break;
            }
            case VmOpCode::Global: {
                assert(false);
                break;
            }
            case VmOpCode::Super: {
                assert(false);
                break;
            }
            case VmOpCode::This: {
                assert(false);
                break;
            }
            case VmOpCode::Test: {
                vmState->setZF(vmState->reg(decodedInst.r1).asBool());
                break;
            }
            case VmOpCode::Inv: {
                assert(false);
                break;
            }
            case VmOpCode::Throw: {
                assert(false);
                break;
            }
            case VmOpCode::Ret: {
                const auto value = vmState->reg(decodedInst.r1);
                const auto *frame = vmState->curFrame();
                CLL_ASSERT(frame->ret, "frame.ret val is empty");
                vmState->prevFrame()->getReg(*frame->ret) = value;
                vmState->freeCallFrame();
                break;
            }
            case VmOpCode::Jmp: {
                assert(false);
                break;
            }
            case VmOpCode::JmpE: {
                assert(false);
                break;
            }
            case VmOpCode::JmpNE: {
                if(!vmState->getZF()) {
                    vmState->setPC0(decodedInst.target);
                }
                break;
            }
            case VmOpCode::ToInt: {
                assert(false);
                break;
            }
            case VmOpCode::ToReal: {
                assert(false);
                break;
            }
            case VmOpCode::ToString: {
                assert(false);
                break;
            }
            case VmOpCode::LNot: {
                assert(false);
                break;
            }
            case VmOpCode::ChgSign: {
                assert(false);
                break;
            }
            case VmOpCode::ChgThis: {
                assert(false);
                break;
            }
            case VmOpCode::ChkInv: {
                assert(false);
                break;
            }
            case VmOpCode::ChkIns: {
                assert(false);
                break;
            }
            case VmOpCode::Load: {
                vmState->regRef(decodedInst.r1) =
                    vmState->curFrame()->chunk->getConstant(ConstIdx{ decodedInst.cIdx }).createValue(&vmState->rt);
                break;
            }
            case VmOpCode::LoadImm: {
                vmState->regRef(decodedInst.r1) = Value{ decodedInst.imm };
                break;
            }
            case VmOpCode::DGlobal: {
                Atom a{ decodedInst.atom };
                const Value &srcVal = vmState->regRef(decodedInst.r1);
                if(vmState->globalHas(a) && propObjectSet(*vmState, srcVal, vmState->global(a))) {
                    break;
                }
                vmState->global(a, Value{ srcVal });
                break;
            }
            case VmOpCode::GGlobal: {
                Value srcVal = vmState->global(Atom{ decodedInst.atom });
                propObjectGet(*vmState, srcVal, srcVal);
                vmState->regRef(decodedInst.r1) = srcVal;
                break;
            }
            case VmOpCode::GThis: {
                Value srcVal = vmState->getThis({ decodedInst.atom });
                propObjectGet(*vmState, srcVal, srcVal);
                vmState->regRef(decodedInst.r1) = srcVal;
                break;
            }
            case VmOpCode::DThis: {
                assert(false);
                break;
            }
            case VmOpCode::Mov: {
                Value srcVal = vmState->reg(decodedInst.r1);
                if(!propObjectSet(*vmState, srcVal, vmState->regRef(decodedInst.r2))) {
                    vmState->regRef(decodedInst.r2) = srcVal;
                }
                break;
            }
            case VmOpCode::CP: {
                Value srcVal = vmState->reg(decodedInst.r1);
                propObjectGet(*vmState, srcVal, srcVal);
                vmState->regRef(decodedInst.r2) = srcVal;
                break;
            }
            case VmOpCode::Add: {
                const Value r1v = vmState->reg(decodedInst.r1);
                Value &r2v = vmState->regRef(decodedInst.r2);
                r2v = r1v.add(r2v).unwrap();
                break;
            }
            case VmOpCode::Sub: {
                const Value r1v = vmState->reg(decodedInst.r1);
                Value &r2v = vmState->regRef(decodedInst.r2);
                r2v = r1v.sub(r2v).unwrap();
                break;
            }
            case VmOpCode::Mul: {
                assert(false);
                break;
            }
            case VmOpCode::Div: {
                assert(false);
                break;
            }
            case VmOpCode::Idiv: {
                assert(false);
                break;
            }
            case VmOpCode::Mod: {
                assert(false);
                break;
            }
            case VmOpCode::BXor: {
                assert(false);
                break;
            }
            case VmOpCode::BOr: {
                assert(false);
                break;
            }
            case VmOpCode::BAnd: {
                assert(false);
                break;
            }
            case VmOpCode::BlShift: {
                assert(false);
                break;
            }
            case VmOpCode::BrShift: {
                assert(false);
                break;
            }
            case VmOpCode::BurShift: {
                assert(false);
                break;
            }
            case VmOpCode::EQ: {
                assert(false);
                break;
            }
            case VmOpCode::NEQ: {
                assert(false);
                break;
            }
            case VmOpCode::AbsEQ: {
                assert(false);
                break;
            }
            case VmOpCode::AbsNEQ: {
                assert(false);
                break;
            }
            case VmOpCode::LT: {
                const Value r1v = vmState->reg(decodedInst.r1);
                Value &r2v = vmState->regRef(decodedInst.r2);
                const bool r = r1v.littlerThan(r2v).unwrap();
                r2v = Value{ r };
                break;
            }
            case VmOpCode::LE: {
                assert(false);
                break;
            }
            case VmOpCode::GT: {
                assert(false);
                break;
            }
            case VmOpCode::GE: {
                assert(false);
                break;
            }
            case VmOpCode::LAnd: {
                assert(false);
                break;
            }
            case VmOpCode::LOr: {
                assert(false);
                break;
            }
            case VmOpCode::Call: {
                const auto &object = vmState->reg(decodedInst.r2).asObject().unwrap();
                object->call(*vmState, decodedInst.r1, decodedInst.imm);
                break;
            }
            case VmOpCode::GProp: {
                assert(false);
                break;
            }
            case VmOpCode::DProp: {
                assert(false);
                break;
            }
        }
    }
} // namespace cial::vm
