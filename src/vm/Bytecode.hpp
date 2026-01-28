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
    inline u64 decodeUleb128(const Vec<u8> &ptr, u64 &pc, const size_t maxLen) {
        u64 result = 0;
        size_t shift = 0;
        size_t bytes = 0;
        while(true) {
            if(bytes >= maxLen) {
                throw std::runtime_error("ULEB128 overflow or truncated");
            }
            const u8 byte = ptr[pc++];
            ++bytes;
            result |= static_cast<u64>(byte & 0x7F) << shift;
            if((byte & 0x80) == 0)
                break;
            shift += 7;
            if(shift >= 64) {
                throw std::runtime_error("ULEB128 too large for u64");
            }
        }
        return result;
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
    inline i64 decodeSleb128(const Vec<u8> &ptr, u64 &pc, const size_t maxLen) {
        const u64 uval = decodeUleb128(ptr, pc, maxLen);
        return zigzagDecode(uval);
    }

    // LE
    inline void writeU16(Vec<u8> &buffer, const u16 value) {
        buffer.push_back(static_cast<u8>(value & 0xFF));
        buffer.push_back(static_cast<u8>(value >> 8));
    }

    inline u16 readU16(const Vec<u8> &ptr, u64 &pc) {
        u16 v{ ptr[pc++] };
        v |= (static_cast<u16>(ptr[pc++]) << 8);
        return v;
    }

    inline void writeU32(Vec<u8> &buffer, const u32 value) {
        buffer.push_back(static_cast<u8>(value & 0xFF));
        buffer.push_back(static_cast<u8>((value >> 8) & 0xFF));
        buffer.push_back(static_cast<u8>((value >> 16) & 0xFF));
        buffer.push_back(static_cast<u8>((value >> 24) & 0xFF));
    }

    inline u32 readU32(const Vec<u8> &ptr, u64 &pc) {
        u16 v{ ptr[pc++] };
        v |= (static_cast<u16>(ptr[pc++]) << 8);
        v |= (static_cast<u16>(ptr[pc++]) << 16);
        v |= (static_cast<u16>(ptr[pc++]) << 24);
        return v;
    }
    // LE END

    struct DecodedInst {
        VmOpCode op{};
        u16 r1{};
        u16 r2{};
        union {
            u16 r3{};
            u16 cIdx;
            i64 imm;
            u32 target;
            u32 atom;
        };
    };

    class Bytecode : public inter::Instruction::Visitor {
#define DECLARE_TAC_OPCODE_VISIT(name) void visit(const inter::name *tac);
        CIAL_TAC_OPCODE_ENUMS(DECLARE_TAC_OPCODE_VISIT)
#undef DECLARE_TAC_OPCODE_VISIT
    public:
        void compile(const Vec<Box<inter::Instruction>> &instructions) {
            for(u32 i = 0; i < instructions.size(); ++i) {
                if(_tacPosToBcPos.contains(i)) {
                    _jmpTagetPos[_tacPosToBcPos[i]] = static_cast<u32>(_code.size());
                }
                instructions[i]->accept(this);
            }

            // update jmp target address
            for(auto &[tp, bp] : _jmpTagetPos) {
                // LE
                _code[tp] = static_cast<u8>(bp & 0xFF);
                _code[tp + 1] = static_cast<u8>((bp >> 8) & 0xFF);
                _code[tp + 2] = static_cast<u8>((bp >> 16) & 0xFF);
                _code[tp + 3] = static_cast<u8>((bp >> 24) & 0xFF);
            }
        }

        DecodedInst decode(u64 &pc) const;
        static void dispatch(const DecodedInst &decodedInst, VMState *vmState);

        [[nodiscard]] size_t getSize() const { return _code.size(); }

    private:
        Vec<u8> _code{};
        Map<u32, u32> _tacPosToBcPos{};
        Map<u32, u32> _jmpTagetPos{};
    };
} // namespace cial::vm
