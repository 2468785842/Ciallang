//
// Created by LiDong on 2026/1/27.
//
#pragma once

#include "gen/Instruction.hpp"
#include "types/Types.hpp"

#define CIAL_BYTECODE_OPCODE_ENUMS(O)                                                                                  \
    O(NOP)                                                                                                             \
    O(Load)                                                                                                            \
    O(LoadImm)                                                                                                         \
    O(Push)                                                                                                            \
    O(PopN)                                                                                                            \
    O(CP)                                                                                                              \
    O(Add)                                                                                                             \
    O(Sub)                                                                                                             \
    O(Mul)                                                                                                             \
    O(Div)                                                                                                             \
    O(Idiv)                                                                                                            \
    O(Mod)                                                                                                             \
    O(Mov)                                                                                                             \
    O(DGlobal)                                                                                                         \
    O(GGlobal)                                                                                                         \
    O(Global)                                                                                                          \
    O(Super)                                                                                                           \
    O(This)                                                                                                            \
    O(ToInt)                                                                                                           \
    O(ToReal)                                                                                                          \
    O(ToString)                                                                                                        \
    O(ChgThis)                                                                                                         \
    O(Inv)                                                                                                             \
    O(ChkInv)                                                                                                          \
    O(ChkIns)                                                                                                          \
    O(Test)                                                                                                            \
    O(EQ)                                                                                                              \
    O(NEQ)                                                                                                             \
    O(LT)                                                                                                              \
    O(LE)                                                                                                              \
    O(GT)                                                                                                              \
    O(GE)                                                                                                              \
    O(AbsEQ)                                                                                                           \
    O(AbsNEQ)                                                                                                          \
    O(Jmp)                                                                                                             \
    O(JmpE)                                                                                                            \
    O(JmpNE)                                                                                                           \
    O(Call)                                                                                                            \
    O(GProp)                                                                                                           \
    O(DProp)                                                                                                           \
    O(GThis)                                                                                                           \
    O(DThis)                                                                                                           \
    O(LNot)                                                                                                            \
    O(LAnd)                                                                                                            \
    O(LOr)                                                                                                             \
    O(BXor)                                                                                                            \
    O(BOr)                                                                                                             \
    O(BAnd)                                                                                                            \
    O(BlShift)                                                                                                         \
    O(BrShift)                                                                                                         \
    O(BurShift)                                                                                                        \
    O(ChgSign)                                                                                                         \
    O(Debugger)                                                                                                        \
    O(Throw)                                                                                                           \
    O(Ret)

namespace cial::vm {
    class VMState;

    enum class VmOpCode : u8 {
#define OPCODE_ENUM_CLASS(OP) OP,
        CIAL_BYTECODE_OPCODE_ENUMS(OPCODE_ENUM_CLASS)
#undef OPCODE_ENUM_CLASS
    };

    // ---------------------- 无符号 LEB128 (uLEB128) ----------------------

    // 编码：把 uint64_t 值写到 vector<uint8_t> 的末尾，返回写入的字节数
    inline size_t encodeUleb128(u64 value, Vec<u8> &buffer) {
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

    // 解码：从 pc 位置读取 uLEB128，返回值 + 消耗的字节数
    // 用 pair 返回 {value, bytes_read}
    inline std::pair<u64, size_t> decodeUleb128(const u8 *ptr, const size_t max_len) {
        u64 result = 0;
        size_t shift = 0;
        size_t bytes = 0;

        while(true) {
            if(bytes >= max_len) {
                throw std::runtime_error("ULEB128 overflow or truncated");
            }
            const u8 byte = ptr[bytes++];
            result |= static_cast<u64>(byte & 0x7F) << shift;
            if((byte & 0x80) == 0)
                break;
            shift += 7;
            if(shift >= 64) {
                throw std::runtime_error("ULEB128 too large for uint64_t");
            }
        }
        return { result, bytes };
    }

    // ---------------------- 有符号 LEB128 (sLEB128) ----------------------

    // ZigZag 编码：把 int64_t 转为无符号表示（负数变奇数，正数变偶数）
    inline u64 zigzagEncode(const i64 value) {
        if(value >= 0)
            return static_cast<u64>(value) << 1;
        return (static_cast<u64>(-value) << 1) | 1;
    }

    // ZigZag 解码
    inline i64 zigzagDecode(const u64 value) {
        return (value & 1) ? -(static_cast<i64>(value >> 1)) : static_cast<i64>(value >> 1);
    }

    // 编码有符号数
    inline size_t encodeSleb128(const i64 value, Vec<u8> &buffer) { return encodeUleb128(zigzagEncode(value), buffer); }

    // 解码有符号数
    inline std::pair<i64, size_t> decodeSleb128(const u8 *ptr, const size_t max_len) {
        auto [uval, bytes] = decodeUleb128(ptr, max_len);
        return { zigzagDecode(uval), bytes };
    }

    // 写 u16（小端）
    inline void writeU16(Vec<u8> &buffer, const u16 value) {
        buffer.push_back(static_cast<u8>(value & 0xFF));
        buffer.push_back(static_cast<u8>(value >> 8));
    }

    // 读 u16（小端）
    inline u16 readU16(const u8 *ptr) { return static_cast<u16>(ptr[0]) | (static_cast<u16>(ptr[1]) << 8); }

    class Bytecode : public inter::Instruction::Visitor {
#define DECLARE_TAC_OPCODE_VISIT(name) void visit(const inter::name *tac);
        CIAL_TAC_OPCODE_ENUMS(DECLARE_TAC_OPCODE_VISIT)
#undef DECLARE_TAC_OPCODE_VISIT
        size_t dispatch(const VMState *vmState) const;

    private:
        Vec<u8> _code{};
    };
} // namespace cial::vm
