//
// Created by LiDong on 2026/1/27.
//

#include "Bytecode.hpp"

#include "VMState.hpp"

namespace cial::vm {
    void Bytecode::visit(const inter::NOP *) { _code.push_back(static_cast<uint8_t>(VmOpCode::NOP)); }

    void Bytecode::visit(const inter::Debugger *) { _code.push_back(static_cast<uint8_t>(VmOpCode::Debugger)); }

    void Bytecode::visit(const inter::Load *tac) {}
    void Bytecode::visit(const inter::LoadImm *tac) {}

    void Bytecode::visit(const inter::Push *tac) {
        const uint16_t dst = tac->getOp1Val<inter::Register>().index();
        _code.push_back(static_cast<uint8_t>(VmOpCode::Push));
        writeU16(_code, dst);
    }

    void Bytecode::visit(const inter::PopN *tac) {
        const i64 imm = tac->getOp1Val<Integer>();
        _code.push_back(static_cast<uint8_t>(VmOpCode::PopN));
        encodeSleb128(imm, _code);
    }

    void Bytecode::visit(const inter::CP *tac) {}
    void Bytecode::visit(const inter::Add *tac) {}
    void Bytecode::visit(const inter::Sub *tac) {}
    void Bytecode::visit(const inter::Mul *tac) {}
    void Bytecode::visit(const inter::Div *tac) {}
    void Bytecode::visit(const inter::Idiv *tac) {}
    void Bytecode::visit(const inter::Mod *tac) {}
    void Bytecode::visit(const inter::Mov *tac) {}
    void Bytecode::visit(const inter::DGlobal *tac) {}
    void Bytecode::visit(const inter::GGlobal *tac) {}
    void Bytecode::visit(const inter::Global *tac) {}
    void Bytecode::visit(const inter::Super *tac) {}
    void Bytecode::visit(const inter::This *tac) {}
    void Bytecode::visit(const inter::ToInt *tac) {}
    void Bytecode::visit(const inter::ToReal *tac) {}
    void Bytecode::visit(const inter::ToString *tac) {}
    void Bytecode::visit(const inter::ChgThis *tac) {}
    void Bytecode::visit(const inter::Inv *tac) {}
    void Bytecode::visit(const inter::ChkInv *tac) {}
    void Bytecode::visit(const inter::ChkIns *tac) {}
    void Bytecode::visit(const inter::Test *tac) {}
    void Bytecode::visit(const inter::EQ *tac) {}
    void Bytecode::visit(const inter::NEQ *tac) {}
    void Bytecode::visit(const inter::LT *tac) {}
    void Bytecode::visit(const inter::LE *tac) {}
    void Bytecode::visit(const inter::GT *tac) {}
    void Bytecode::visit(const inter::GE *tac) {}
    void Bytecode::visit(const inter::AbsEQ *tac) {}
    void Bytecode::visit(const inter::AbsNEQ *tac) {}
    void Bytecode::visit(const inter::Jmp *tac) {}
    void Bytecode::visit(const inter::JmpE *tac) {}
    void Bytecode::visit(const inter::JmpNE *tac) {}
    void Bytecode::visit(const inter::Call *tac) {}
    void Bytecode::visit(const inter::GProp *tac) {}
    void Bytecode::visit(const inter::DProp *tac) {}
    void Bytecode::visit(const inter::GThis *tac) {}
    void Bytecode::visit(const inter::DThis *tac) {}
    void Bytecode::visit(const inter::LNot *tac) {}
    void Bytecode::visit(const inter::LAnd *tac) {}
    void Bytecode::visit(const inter::LOr *tac) {}
    void Bytecode::visit(const inter::BXor *tac) {}
    void Bytecode::visit(const inter::BOr *tac) {}
    void Bytecode::visit(const inter::BAnd *tac) {}
    void Bytecode::visit(const inter::BlShift *tac) {}
    void Bytecode::visit(const inter::BrShift *tac) {}
    void Bytecode::visit(const inter::BurShift *tac) {}
    void Bytecode::visit(const inter::ChgSign *tac) {}
    void Bytecode::visit(const inter::Throw *tac) {}
    void Bytecode::visit(const inter::Ret *tac) {}

    size_t Bytecode::dispatch(const VMState *vmState) const {
        u16 count = 0;
        const u8 *pc = _code.data() + vmState->getPC();
        size_t remaining{ _code.size() - vmState->getPC() };
        const auto op = static_cast<VmOpCode>(*pc++);
        --remaining;

        auto cursor = [&](const size_t n) {
            count += n;
            pc += n;
            remaining -= n;
        };

        switch(op) {
            case VmOpCode::NOP:
                break;

            case VmOpCode::Load: {
                break;
            }
            case VmOpCode::LoadImm: {
                break;
            }
            case VmOpCode::Push: {
                if(remaining < 4)
                    throw std::runtime_error("truncated");
                const u16 r1 = readU16(pc);
                cursor(2);
                vmState->push(vmState->reg(r1));
                break;
            }
            case VmOpCode::PopN: {
                break;
            }
            case VmOpCode::CP: {
                break;
            }

            case VmOpCode::Add: {
                if(remaining < 6)
                    throw std::runtime_error("truncated");
                const u16 r1 = readU16(pc);
                cursor(2);
                const u16 r2 = readU16(pc);
                cursor(2);
                const Value r1v = vmState->reg(r1);
                Value &r2v = vmState->regRef(r2);
                r2v = r1v.add(r2v).unwrap();
                break;
            }
            case VmOpCode::Sub: {
                break;
            }
            case VmOpCode::Mul: {
                break;
            }
            case VmOpCode::Div: {
                break;
            }
            case VmOpCode::Idiv: {
                break;
            }
            case VmOpCode::Mod: {
                break;
            }
            case VmOpCode::Mov: {
                break;
            }
            case VmOpCode::DGlobal: {
                break;
            }
            case VmOpCode::GGlobal: {
                break;
            }
            case VmOpCode::Global: {
                break;
            }
            case VmOpCode::Super: {
                break;
            }
            case VmOpCode::This: {
                break;
            }
            case VmOpCode::ToInt: {
                break;
            }
            case VmOpCode::ToReal: {
                break;
            }
            case VmOpCode::ToString: {
                break;
            }
            case VmOpCode::ChgThis: {
                break;
            }
            case VmOpCode::Inv: {
                break;
            }
            case VmOpCode::ChkInv: {
                break;
            }
            case VmOpCode::ChkIns: {
                break;
            }
            case VmOpCode::Test: {
                break;
            }
            case VmOpCode::EQ: {
                break;
            }
            case VmOpCode::NEQ: {
                break;
            }
            case VmOpCode::LT: {
                break;
            }
            case VmOpCode::LE: {
                break;
            }
            case VmOpCode::GT: {
                break;
            }
            case VmOpCode::GE: {
                break;
            }
            case VmOpCode::AbsEQ: {
                break;
            }
            case VmOpCode::AbsNEQ: {
                break;
            }
            case VmOpCode::Jmp: {
                break;
            }
            case VmOpCode::JmpE: {
                break;
            }
            case VmOpCode::JmpNE: {
                break;
            }
            case VmOpCode::Call: {
                break;
            }
            case VmOpCode::GProp: {
                break;
            }
            case VmOpCode::DProp: {
                break;
            }
            case VmOpCode::GThis: {
                break;
            }
            case VmOpCode::DThis: {
                break;
            }
            case VmOpCode::LNot: {
                break;
            }
            case VmOpCode::LAnd: {
                break;
            }
            case VmOpCode::LOr: {
                break;
            }
            case VmOpCode::BXor: {
                break;
            }
            case VmOpCode::BOr: {
                break;
            }
            case VmOpCode::BAnd: {
                break;
            }
            case VmOpCode::BlShift: {
                break;
            }
            case VmOpCode::BrShift: {
                break;
            }
            case VmOpCode::BurShift: {
                break;
            }
            case VmOpCode::ChgSign: {
                break;
            }
            case VmOpCode::Debugger: {
                break;
            }
            case VmOpCode::Throw: {
                break;
            }
            case VmOpCode::Ret: {
                break;
            }
        }
        return count;
    }
} // namespace cial::vm
