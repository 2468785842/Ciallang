//
// Created by LiDong on 2026/1/27.
//

#include <fmt/format.h>
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
            // shift 的单位是 bit（不是字节），u64 上限 64 bit
            if(shift >= sizeof(u64) * 8) {
                throw std::runtime_error("ULEB128 too large for u64");
            }
        }
        pc += bytes;
        return result;
    }

    // ── 无操作数 ─────────────────────────────────────────────
    void Bytecode::encode(Info &info, const inter::NOP *) { info.code.push_back(static_cast<u8>(VmOpCode::NOP)); }

    void Bytecode::encode(Info &info, const inter::Debugger *) {
        info.code.push_back(static_cast<u8>(VmOpCode::Debugger));
    }

    // ── 栈操作 ───────────────────────────────────────────────
    void Bytecode::encode(Info &info, const inter::Push *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::Push));
        write<u16>(info.code, tac->src().index());
    }

    void Bytecode::encode(Info &info, const inter::PopN *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::PopN));
        encodeSleb128(tac->count(), info.code);
    }

    // ── 上下文对象 ───────────────────────────────────────────
    void Bytecode::encode(Info &info, const inter::Global *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::Global));
        write<u16>(info.code, tac->dst().index());
    }

    void Bytecode::encode(Info &info, const inter::Super *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::Super));
        write<u16>(info.code, tac->dst().index());
    }

    void Bytecode::encode(Info &info, const inter::This *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::This));
        write<u16>(info.code, tac->dst().index());
    }

    // ── 标志/失效 ────────────────────────────────────────────
    void Bytecode::encode(Info &info, const inter::Test *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::Test));
        write<u16>(info.code, tac->src().index());
    }

    void Bytecode::encode(Info &info, const inter::Inv *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::Inv));
        write<u16>(info.code, tac->src().index());
    }

    void Bytecode::encode(Info &info, const inter::Throw *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::Throw));
        write<u16>(info.code, tac->src().index());
    }

    void Bytecode::encode(Info &info, const inter::Ret *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::Ret));
        write<u16>(info.code, tac->src().index());
    }

    // ── 跳转（目标 u32 占位，编译结束后回填） ────────────────
    void Bytecode::encode(Info &info, const inter::Jmp *tac) {
        encodeJmp(info, VmOpCode::Jmp, tac->label().address());
    }

    void Bytecode::encode(Info &info, const inter::JmpE *tac) {
        encodeJmp(info, VmOpCode::JmpE, tac->label().address());
    }

    void Bytecode::encode(Info &info, const inter::JmpNE *tac) {
        encodeJmp(info, VmOpCode::JmpNE, tac->label().address());
    }

    // ── 类型转换/单目 ─────────────────────────────────────────
    void Bytecode::encode(Info &info, const inter::ToInt *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::ToInt));
        write<u16>(info.code, tac->src().index());
        write<u16>(info.code, tac->dst().index());
    }

    void Bytecode::encode(Info &info, const inter::ToReal *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::ToReal));
        write<u16>(info.code, tac->src().index());
        write<u16>(info.code, tac->dst().index());
    }

    void Bytecode::encode(Info &info, const inter::ToString *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::ToString));
        write<u16>(info.code, tac->src().index());
        write<u16>(info.code, tac->dst().index());
    }

    void Bytecode::encode(Info &info, const inter::Asc *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::Asc));
        write<u16>(info.code, tac->src().index());
        write<u16>(info.code, tac->dst().index());
    }

    void Bytecode::encode(Info &info, const inter::Chr *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::Chr));
        write<u16>(info.code, tac->src().index());
        write<u16>(info.code, tac->dst().index());
    }

    void Bytecode::encode(Info &info, const inter::TypeOf *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::TypeOf));
        write<u16>(info.code, tac->src().index());
        write<u16>(info.code, tac->dst().index());
    }

    void Bytecode::encode(Info &info, const inter::LNot *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::LNot));
        write<u16>(info.code, tac->src().index());
        write<u16>(info.code, tac->dst().index());
    }

    void Bytecode::encode(Info &info, const inter::ChgSign *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::ChgSign));
        write<u16>(info.code, tac->src().index());
        write<u16>(info.code, tac->dst().index());
    }

    void Bytecode::encode(Info &info, const inter::ChgThis *tac) {
        // 注意操作数顺序：op1=函数寄存器(dst)，op2=新 this(src)
        info.code.push_back(static_cast<u8>(VmOpCode::ChgThis));
        write<u16>(info.code, tac->dst().index());
        write<u16>(info.code, tac->src().index());
    }

    void Bytecode::encode(Info &info, const inter::ChkInv *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::ChkInv));
        write<u16>(info.code, tac->src().index());
        write<u16>(info.code, tac->dst().index());
    }

    void Bytecode::encode(Info &info, const inter::ChkIns *tac) {
        // op1=dst（同时是输入对象与输出 bool），op2=src（类名字符串）
        info.code.push_back(static_cast<u8>(VmOpCode::ChkIns));
        write<u16>(info.code, tac->dst().index());
        write<u16>(info.code, tac->src().index());
    }

    void Bytecode::encode(Info &info, const inter::TestVoid *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::TestVoid));
        write<u16>(info.code, tac->src().index());
        write<u16>(info.code, tac->dst().index());
    }

    // ── 装载 ─────────────────────────────────────────────────
    void Bytecode::encode(Info &info, const inter::Load *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::Load));
        write<u16>(info.code, tac->dst().index());
        write<u16>(info.code, tac->value().index());
    }

    void Bytecode::encode(Info &info, const inter::LoadImm *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::LoadImm));
        write<u16>(info.code, tac->dst().index());
        encodeSleb128(tac->value(), info.code);
    }

    // ── 全局/this/局部变量访问 ────────────────────────────────
    void Bytecode::encode(Info &info, const inter::DGlobal *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::DGlobal));
        write<u32>(info.code, static_cast<u32>(tac->atom().v));
        write<u16>(info.code, tac->src().index());
    }

    void Bytecode::encode(Info &info, const inter::GGlobal *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::GGlobal));
        write<u32>(info.code, static_cast<u32>(tac->atom().v));
        write<u16>(info.code, tac->dst().index());
    }

    void Bytecode::encode(Info &info, const inter::GThis *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::GThis));
        write<u32>(info.code, static_cast<u32>(tac->atom().v));
        write<u16>(info.code, tac->dst().index());
    }

    void Bytecode::encode(Info &info, const inter::DThis *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::DThis));
        write<u32>(info.code, static_cast<u32>(tac->atom().v));
        write<u16>(info.code, tac->src().index());
    }

    void Bytecode::encode(Info &info, const inter::GLocal *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::GLocal));
        write<u16>(info.code, tac->src().index());
        write<u16>(info.code, tac->dst().index());
    }

    void Bytecode::encode(Info &info, const inter::DLocal *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::DLocal));
        write<u16>(info.code, tac->src().index());
        write<u16>(info.code, tac->dst().index());
    }

    // ── 算术/位运算/比较：统一三寄存器原生编码 ────────────────
#define ENCODE_RRR(op, getter1, getter2, getter3)                                                                       \
    void Bytecode::encode(Info &info, const inter::op *tac) {                                                           \
        info.code.push_back(static_cast<u8>(VmOpCode::op));                                                             \
        write<u16>(info.code, tac->getter1().index());                                                                  \
        write<u16>(info.code, tac->getter2().index());                                                                  \
        write<u16>(info.code, tac->getter3().index());                                                                  \
    }

    ENCODE_RRR(Add, src1, src2, dst)
    ENCODE_RRR(Sub, src1, src2, dst)
    ENCODE_RRR(Mul, src1, src2, dst)
    ENCODE_RRR(Div, src1, src2, dst)
    ENCODE_RRR(Idiv, src1, src2, dst)
    ENCODE_RRR(Mod, src1, src2, dst)

    ENCODE_RRR(BXor, src1, src2, dst)
    ENCODE_RRR(BOr, src1, src2, dst)
    ENCODE_RRR(BAnd, src1, src2, dst)
    ENCODE_RRR(BlShift, src1, src2, dst)
    ENCODE_RRR(BrShift, src1, src2, dst)
    ENCODE_RRR(BurShift, src1, src2, dst)

    ENCODE_RRR(EQ, src1, src2, dst)
    ENCODE_RRR(NEQ, src1, src2, dst)
    ENCODE_RRR(AbsEQ, src1, src2, dst)
    ENCODE_RRR(AbsNEQ, src1, src2, dst)
    ENCODE_RRR(LT, src1, src2, dst)
    ENCODE_RRR(LE, src1, src2, dst)
    ENCODE_RRR(GT, src1, src2, dst)
    ENCODE_RRR(GE, src1, src2, dst)
    ENCODE_RRR(LAnd, src1, src2, dst)
    ENCODE_RRR(LOr, src1, src2, dst)

    // ── 调用/属性 ────────────────────────────────────────────
    void Bytecode::encode(Info &info, const inter::Call *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::Call));
        write<u16>(info.code, tac->dst().index());
        write<u16>(info.code, tac->memberReg().index());
        encodeSleb128(tac->argCount(), info.code);
    }

    void Bytecode::encode(Info &info, const inter::GProp *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::GProp));
        write<u16>(info.code, tac->obj().index());
        write<u16>(info.code, tac->memberReg().index());
        write<u16>(info.code, tac->dst().index());
    }

    void Bytecode::encode(Info &info, const inter::DProp *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::DProp));
        write<u16>(info.code, tac->obj().index());
        write<u16>(info.code, tac->memberReg().index());
        write<u16>(info.code, tac->src().index());
    }

    void Bytecode::encode(Info &info, const inter::DelProp *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::DelProp));
        write<u16>(info.code, tac->obj().index());
        write<u16>(info.code, tac->memberReg().index());
        write<u16>(info.code, tac->dst().index());
    }

    void Bytecode::encode(Info &info, const inter::GPropRaw *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::GPropRaw));
        write<u16>(info.code, tac->obj().index());
        write<u16>(info.code, tac->memberReg().index());
        write<u16>(info.code, tac->dst().index());
    }

    void Bytecode::encode(Info &info, const inter::GRefRaw *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::GRefRaw));
        write<u32>(info.code, static_cast<u32>(tac->atom().v));
        write<u16>(info.code, tac->dst().index());
    }

    void Bytecode::encode(Info &info, const inter::DPropRaw *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::DPropRaw));
        write<u16>(info.code, tac->obj().index());
        write<u16>(info.code, tac->memberReg().index());
        write<u16>(info.code, tac->src().index());
    }

    void Bytecode::encode(Info &info, const inter::DerefGet *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::DerefGet));
        write<u16>(info.code, tac->prop().index());
        write<u16>(info.code, tac->dst().index());
    }

    void Bytecode::encode(Info &info, const inter::DerefSet *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::DerefSet));
        write<u16>(info.code, tac->prop().index());
        write<u16>(info.code, tac->src().index());
    }

    void Bytecode::encode(Info &info, const inter::NewArray *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::NewArray));
        write<u16>(info.code, tac->dst().index());
        encodeSleb128(tac->count(), info.code);
    }

    void Bytecode::encode(Info &info, const inter::NewDict *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::NewDict));
        write<u16>(info.code, tac->dst().index());
    }

    void Bytecode::encode(Info &info, const inter::GetIndex *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::GetIndex));
        write<u16>(info.code, tac->obj().index());
        write<u16>(info.code, tac->idxReg().index());
        write<u16>(info.code, tac->dst().index());
    }

    void Bytecode::encode(Info &info, const inter::SetIndex *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::SetIndex));
        write<u16>(info.code, tac->obj().index());
        write<u16>(info.code, tac->idxReg().index());
        write<u16>(info.code, tac->src().index());
    }

    void Bytecode::encode(Info &info, const inter::WithPush *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::WithPush));
        write<u16>(info.code, tac->src().index());
    }

    void Bytecode::encode(Info &info, const inter::WithPop *) {
        info.code.push_back(static_cast<u8>(VmOpCode::WithPop));
    }

    void Bytecode::encode(Info &info, const inter::GetWith *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::GetWith));
        write<u16>(info.code, tac->dst().index());
    }

    void Bytecode::encode(Info &info, const inter::NewRegExp *tac) {
        info.code.push_back(static_cast<u8>(VmOpCode::NewRegExp));
        write<u16>(info.code, tac->dst().index());
        write<u16>(info.code, tac->pattern().index());
    }
} // namespace cial::vm
