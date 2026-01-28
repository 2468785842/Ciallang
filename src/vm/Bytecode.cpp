//
// Created by LiDong on 2026/1/27.
//

#include "Bytecode.hpp"

#include "VMState.hpp"
#include "types/Property.hpp"

namespace cial::vm {
    void Bytecode::visit(const inter::NOP *) { _code.push_back(static_cast<u8>(VmOpCode::NOP)); }

    void Bytecode::visit(const inter::Debugger *) { _code.push_back(static_cast<u8>(VmOpCode::Debugger)); }

    void Bytecode::visit(const inter::Push *tac) {
        const u16 dst = tac->src().index();
        _code.push_back(static_cast<u8>(VmOpCode::Push));
        write<u16>(_code, dst);
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
        write<u16>(_code, r);
    }

    void Bytecode::visit(const inter::Inv *tac) { assert(false); }
    void Bytecode::visit(const inter::Throw *tac) { assert(false); }

    void Bytecode::visit(const inter::Ret *tac) {
        const u16 r1 = tac->src().index();
        _code.push_back(static_cast<u8>(VmOpCode::Ret));
        write<u16>(_code, r1);
    }

    void Bytecode::visit(const inter::Jmp *tac) { assert(false); }
    void Bytecode::visit(const inter::JmpE *tac) { assert(false); }

    void Bytecode::visit(const inter::JmpNE *tac) {
        _code.push_back(static_cast<u8>(VmOpCode::JmpNE));
        _tacPosToBcPos[static_cast<u32>(tac->label().address())] = _code.size();
        write<u32>(_code, 0);
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
        write<u16>(_code, dst);
        write<u16>(_code, val);
    }

    void Bytecode::visit(const inter::LoadImm *tac) {
        const u16 dst = tac->dst().index();
        const i64 val = tac->value();
        _code.push_back(static_cast<u8>(VmOpCode::LoadImm));
        write<u16>(_code, dst);
        encodeSleb128(val, _code);
    }

    void Bytecode::visit(const inter::DGlobal *tac) {
        const u32 atom = static_cast<u32>(tac->atom().v);
        const u16 dst = tac->src().index();
        _code.push_back(static_cast<u8>(VmOpCode::DGlobal));
        write<u32>(_code, atom);
        write<u16>(_code, dst);
    }

    void Bytecode::visit(const inter::GGlobal *tac) {
        const u32 atom = static_cast<u32>(tac->atom().v);
        const u16 dst = tac->dst().index();
        _code.push_back(static_cast<u8>(VmOpCode::GGlobal));
        write<u32>(_code, atom);
        write<u16>(_code, dst);
    }

    void Bytecode::visit(const inter::GThis *tac) {
        const u32 atom = static_cast<u32>(tac->atom().v);
        const u16 dst = tac->dst().index();
        _code.push_back(static_cast<u8>(VmOpCode::GThis));
        write<u32>(_code, atom);
        write<u16>(_code, dst);
    }
    void Bytecode::visit(const inter::DThis *tac) { assert(false); }

    void Bytecode::visit(const inter::Mov *tac) { assert(false); }

    void Bytecode::visit(const inter::CP *tac) {
        const u16 r1 = tac->src().index();
        const u16 r2 = tac->dst().index();
        _code.push_back(static_cast<u8>(VmOpCode::CP));
        write<u16>(_code, r1);
        write<u16>(_code, r2);
    }

    void Bytecode::visit(const inter::Add *tac) {
        const u16 r1 = tac->src1().index();
        const u16 r2 = tac->src2().index();
        const u16 r3 = tac->dst().index();
        if(r2 == r3) {
            _code.push_back(static_cast<u8>(VmOpCode::Add));
            write<u16>(_code, r1);
            write<u16>(_code, r3);
        } else {
            _code.push_back(static_cast<u8>(VmOpCode::Mov));
            write<u16>(_code, r2);
            write<u16>(_code, r3);

            _code.push_back(static_cast<u8>(VmOpCode::Add));
            write<u16>(_code, r1);
            write<u16>(_code, r3);
        }
    }

    void Bytecode::visit(const inter::Sub *tac) {
        const u16 r1 = tac->src1().index();
        const u16 r2 = tac->src2().index();
        const u16 r3 = tac->dst().index();
        if(r2 != r3) {
            _code.push_back(static_cast<u8>(VmOpCode::Mov));
            write<u16>(_code, r2);
            write<u16>(_code, r3);
        }
        _code.push_back(static_cast<u8>(VmOpCode::Sub));
        write<u16>(_code, r1);
        write<u16>(_code, r3);
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
            write<u16>(_code, r1);
            write<u16>(_code, r3);
        } else {
            _code.push_back(static_cast<u8>(VmOpCode::Mov));
            write<u16>(_code, r2);
            write<u16>(_code, r3);

            _code.push_back(static_cast<u8>(VmOpCode::LT));
            write<u16>(_code, r1);
            write<u16>(_code, r3);
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
        write<u16>(_code, r1);
        write<u16>(_code, r2);
        encodeSleb128(imm, _code);
    }

    void Bytecode::visit(const inter::GProp *tac) { assert(false); }
    void Bytecode::visit(const inter::DProp *tac) { assert(false); }
} // namespace cial::vm
