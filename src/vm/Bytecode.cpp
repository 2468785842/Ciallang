//
// Created by LiDong on 2026/1/27.
//

#include "Bytecode.hpp"

#include "VMState.hpp"
#include "types/Property.hpp"

namespace cial::vm {

    size_t encodeUleb128(u64 value, Vec<u8> &buffer) {
        const size_t start = buffer.size();
        do {
            u8 byte = value & 0x7F;
            value >>= 7;
            if(value != 0)
                byte |= 0x80; // 延续位
            buffer.push_back(byte);
        } while(value != 0);
        return buffer.size() - start;
    }

    u64 decodeUleb128(const u8 *ptr, u64 &pc, const size_t maxLen) {
        u64 result = 0;
        size_t shift = 0;
        size_t bytes = 0;
        const u8 *p = ptr + pc;
        while(true) {
            if(bytes >= maxLen) {
                throw std::runtime_error("ULEB128 overflow or truncated");
            }
            const u8 byte = p[bytes++];
            result |= static_cast<u64>(byte & 0x7F) << shift;
            if((byte & 0x80) == 0)
                break;
            shift += 7;
            if(shift >= sizeof(u64)) {
                throw std::runtime_error("ULEB128 too large for u64");
            }
        }
        pc += bytes;
        return result;
    }

    void Bytecode::encode(Info &info, const inter::NOP *) { info.code.push_back(static_cast<u8>(VmOpCode::NOP)); }

    void Bytecode::encode(Info &info, const inter::Debugger *) {
        info.code.push_back(static_cast<u8>(VmOpCode::Debugger));
    }

    void Bytecode::encode(Info &info, const inter::Push *tac) {
        const u16 dst = tac->src().index();
        info.code.push_back(static_cast<u8>(VmOpCode::Push));
        write<u16>(info.code, dst);
    }

    void Bytecode::encode(Info &info, const inter::PopN *tac) {
        assert(false);
        // const i64 imm = tac->getOp1Val<Integer>();
        // info.code.push_back(static_cast<u8>(VmOpCode::PopN));
        // encodeSleb128(imm, info.code);
    }

    void Bytecode::encode(Info &info, const inter::Global *tac) { assert(false); }
    void Bytecode::encode(Info &info, const inter::Super *tac) { assert(false); }
    void Bytecode::encode(Info &info, const inter::This *tac) { assert(false); }

    void Bytecode::encode(Info &info, const inter::Test *tac) {
        const u16 r = tac->src().index();
        info.code.push_back(static_cast<u8>(VmOpCode::Test));
        write<u16>(info.code, r);
    }

    void Bytecode::encode(Info &info, const inter::Inv *tac) { assert(false); }
    void Bytecode::encode(Info &info, const inter::Throw *tac) { assert(false); }

    void Bytecode::encode(Info &info, const inter::Ret *tac) {
        const u16 r1 = tac->src().index();
        info.code.push_back(static_cast<u8>(VmOpCode::Ret));
        write<u16>(info.code, r1);
    }

    void Bytecode::encode(Info &info, const inter::Jmp *tac) { assert(false); }
    void Bytecode::encode(Info &info, const inter::JmpE *tac) { assert(false); }

    void Bytecode::encode(Info &info, const inter::JmpNE *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::JmpNE));
        info.tacPosToBcPos[tac->label()] = info.code.size();
        write<u32>(info.code, 0);
    }

    void Bytecode::encode(Info &info, const inter::ToInt *tac) { assert(false); }
    void Bytecode::encode(Info &info, const inter::ToReal *tac) { assert(false); }
    void Bytecode::encode(Info &info, const inter::ToString *tac) { assert(false); }

    void Bytecode::encode(Info &info, const inter::LNot *tac) { assert(false); }

    void Bytecode::encode(Info &info, const inter::ChgSign *tac) { assert(false); }
    void Bytecode::encode(Info &info, const inter::ChgThis *tac) { assert(false); }

    void Bytecode::encode(Info &info, const inter::ChkInv *tac) { assert(false); }
    void Bytecode::encode(Info &info, const inter::ChkIns *tac) { assert(false); }

    void Bytecode::encode(Info &info, const inter::Load *tac) {
        const u16 dst = tac->dst().index();
        const u16 val = tac->value().index();
        info.code.push_back(static_cast<u8>(VmOpCode::Load));
        write<u16>(info.code, dst);
        write<u16>(info.code, val);
    }

    void Bytecode::encode(Info &info, const inter::LoadImm *tac) {
        const u16 dst = tac->dst().index();
        const i64 val = tac->value();
        info.code.push_back(static_cast<u8>(VmOpCode::LoadImm));
        write<u16>(info.code, dst);
        encodeSleb128(val, info.code);
    }

    void Bytecode::encode(Info &info, const inter::DGlobal *tac) {
        const u32 atom = static_cast<u32>(tac->atom().v);
        const u16 dst = tac->src().index();
        info.code.push_back(static_cast<u8>(VmOpCode::DGlobal));
        write<u32>(info.code, atom);
        write<u16>(info.code, dst);
    }

    void Bytecode::encode(Info &info, const inter::GGlobal *tac) {
        const u32 atom = static_cast<u32>(tac->atom().v);
        const u16 dst = tac->dst().index();
        info.code.push_back(static_cast<u8>(VmOpCode::GGlobal));
        write<u32>(info.code, atom);
        write<u16>(info.code, dst);
    }

    void Bytecode::encode(Info &info, const inter::GThis *tac) {
        const u32 atom = static_cast<u32>(tac->atom().v);
        const u16 dst = tac->dst().index();
        info.code.push_back(static_cast<u8>(VmOpCode::GThis));
        write<u32>(info.code, atom);
        write<u16>(info.code, dst);
    }

    void Bytecode::encode(Info &info, const inter::DThis *tac) { assert(false); }

    void Bytecode::encode(Info &info, const inter::DLocal *tac) { assert(false); }

    void Bytecode::encode(Info &info, const inter::GLocal *tac) {
        const u16 r1 = tac->src().index();
        const u16 r2 = tac->dst().index();
        info.code.push_back(static_cast<u8>(VmOpCode::GLocal));
        write<u16>(info.code, r1);
        write<u16>(info.code, r2);
    }

    void Bytecode::encode(Info &info, const inter::Add *tac) {
        const u16 r1 = tac->src1().index();
        const u16 r2 = tac->src2().index();
        const u16 r3 = tac->dst().index();
        if(r2 == r3) {
            info.code.push_back(static_cast<u8>(VmOpCode::Add));
            write<u16>(info.code, r1);
            write<u16>(info.code, r3);
        } else {
            info.code.push_back(static_cast<u8>(VmOpCode::DLocal));
            write<u16>(info.code, r2);
            write<u16>(info.code, r3);

            info.code.push_back(static_cast<u8>(VmOpCode::Add));
            write<u16>(info.code, r1);
            write<u16>(info.code, r3);
        }
    }

    void Bytecode::encode(Info &info, const inter::Sub *tac) {
        const u16 r1 = tac->src1().index();
        const u16 r2 = tac->src2().index();
        const u16 r3 = tac->dst().index();
        if(r2 != r3) {
            info.code.push_back(static_cast<u8>(VmOpCode::DLocal));
            write<u16>(info.code, r2);
            write<u16>(info.code, r3);
        }
        info.code.push_back(static_cast<u8>(VmOpCode::Sub));
        write<u16>(info.code, r1);
        write<u16>(info.code, r3);
    }

    void Bytecode::encode(Info &info, const inter::Mul *tac) { assert(false); }
    void Bytecode::encode(Info &info, const inter::Div *tac) { assert(false); }
    void Bytecode::encode(Info &info, const inter::Idiv *tac) { assert(false); }
    void Bytecode::encode(Info &info, const inter::Mod *tac) { assert(false); }

    void Bytecode::encode(Info &info, const inter::BXor *tac) { assert(false); }
    void Bytecode::encode(Info &info, const inter::BOr *tac) { assert(false); }
    void Bytecode::encode(Info &info, const inter::BAnd *tac) { assert(false); }
    void Bytecode::encode(Info &info, const inter::BlShift *tac) { assert(false); }
    void Bytecode::encode(Info &info, const inter::BrShift *tac) { assert(false); }
    void Bytecode::encode(Info &info, const inter::BurShift *tac) { assert(false); }

    void Bytecode::encode(Info &info, const inter::EQ *tac) { assert(false); }
    void Bytecode::encode(Info &info, const inter::NEQ *tac) { assert(false); }
    void Bytecode::encode(Info &info, const inter::AbsEQ *tac) { assert(false); }
    void Bytecode::encode(Info &info, const inter::AbsNEQ *tac) { assert(false); }

    void Bytecode::encode(Info &info, const inter::LT *tac) {
        const u16 r1 = tac->src1().index();
        const u16 r2 = tac->src2().index();
        const u16 r3 = tac->dst().index();
        if(r2 == r3) {
            info.code.push_back(static_cast<u8>(VmOpCode::LT));
            write<u16>(info.code, r1);
            write<u16>(info.code, r3);
        } else {
            info.code.push_back(static_cast<u8>(VmOpCode::DLocal));
            write<u16>(info.code, r2);
            write<u16>(info.code, r3);

            info.code.push_back(static_cast<u8>(VmOpCode::LT));
            write<u16>(info.code, r1);
            write<u16>(info.code, r3);
        }
    }

    void Bytecode::encode(Info &info, const inter::LE *tac) { assert(false); }
    void Bytecode::encode(Info &info, const inter::GT *tac) { assert(false); }
    void Bytecode::encode(Info &info, const inter::GE *tac) { assert(false); }
    void Bytecode::encode(Info &info, const inter::LAnd *tac) { assert(false); }
    void Bytecode::encode(Info &info, const inter::LOr *tac) { assert(false); }

    void Bytecode::encode(Info &info, const inter::Call *tac) {
        const u16 r1 = tac->dst().index();
        const u16 r2 = tac->memberReg().index();
        const i64 imm = tac->argCount();
        info.code.push_back(static_cast<u8>(VmOpCode::Call));
        write<u16>(info.code, r1);
        write<u16>(info.code, r2);
        encodeSleb128(imm, info.code);
    }

    void Bytecode::encode(Info &info, const inter::GProp *tac) { assert(false); }
    void Bytecode::encode(Info &info, const inter::DProp *tac) { assert(false); }
} // namespace cial::vm
