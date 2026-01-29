//
// Created by LiDong on 2026/1/27.
//
#pragma once

#include "config.h"
#include "gen/Instruction.hpp"
#include "types/Types.hpp"

#define CIAL_BYTECODE_OPCODE_ENUMS(O)                                                                                  \
    O(NOP)                                                                                                             \
    O(Load)                                                                                                            \
    O(LoadImm)                                                                                                         \
    O(Push)                                                                                                            \
    O(CP)                                                                                                              \
    O(Add)                                                                                                             \
    O(Sub)                                                                                                             \
    O(Mov)                                                                                                             \
    O(DGlobal)                                                                                                         \
    O(GGlobal)                                                                                                         \
    O(Test)                                                                                                            \
    O(LT)                                                                                                              \
    O(JmpNE)                                                                                                           \
    O(Call)                                                                                                            \
    O(GThis)                                                                                                           \
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
    inline u64 decodeUleb128(const u8 *ptr, u64 &pc, const size_t maxLen) {
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

    // ---------------------- 有符号 LEB128 (sLEB128) ----------------------

    // ZigZag 编码
    CIAL_INLINE u64 zigzagEncode(const i64 v) noexcept {
        return (static_cast<u64>(v) << 1) ^ static_cast<u64>(v >> 63);
    }

    // ZigZag 解码
    CIAL_INLINE i64 zigzagDecode(const u64 v) noexcept {
        return (v & 1) ? -(static_cast<i64>(v >> 1)) : static_cast<i64>(v >> 1);
    }

    // 编码有符号数
    inline size_t encodeSleb128(const i64 value, Vec<u8> &buffer) { return encodeUleb128(zigzagEncode(value), buffer); }

    // 解码有符号数
    inline i64 decodeSleb128(const u8 *ptr, u64 &pc, const size_t maxLen) {
        const u64 uval = decodeUleb128(ptr, pc, maxLen);
        return zigzagDecode(uval);
    }

    template <typename T>
    inline constexpr bool is_number_v = sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 6 || sizeof(T) == 8;

    template <typename T>
        requires is_number_v<T>
    CIAL_INLINE void write(Vec<u8> &buffer, const T value) noexcept {
        const size_t oldSize = buffer.size();
        buffer.resize(oldSize + sizeof(T));
        std::memcpy(&buffer[oldSize], &value, sizeof(T));
    }

    template <typename T>
        requires is_number_v<T>
    CIAL_INLINE T read(const u8 *ptr, u64 &pc) noexcept {
        T v;
        std::memcpy(&v, ptr + pc, sizeof(T));
        pc += sizeof(T);
        return v;
    }

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
                std::memcpy(&_code[tp], &bp, sizeof(bp));
            }
        }

        [[nodiscard]] size_t getSize() const { return _code.size(); }
        [[nodiscard]] const Vec<u8> &code() const { return _code; }

    private:
        Vec<u8> _code{};
        Map<u32, u32> _tacPosToBcPos{};
        Map<u32, u32> _jmpTagetPos{};
    };
} // namespace cial::vm
