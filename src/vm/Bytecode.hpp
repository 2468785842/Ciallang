//
// Created by LiDong on 2026/1/27.
//
#pragma once

#include "config.h"
#include "gen/Instruction.hpp"
#include "types/Types.hpp"

#define CIAL_BYTECODE_OPCODE_ENUMS(O)                                                                                  \
    O(NOP)                                                                                                             \
    O(Debugger)                                                                                                        \
    O(Load)                                                                                                            \
    O(LoadImm)                                                                                                         \
    O(Push)                                                                                                            \
    O(GLocal)                                                                                                          \
    O(Add)                                                                                                             \
    O(Sub)                                                                                                             \
    O(DLocal)                                                                                                          \
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
    size_t encodeUleb128(u64 value, Vec<u8> &buffer);

    // 解码：从 pc 位置读取 uLEB128，返回值 + 消耗的字节数
    u64 decodeUleb128(const u8 *ptr, u64 &pc, size_t maxLen);

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
    CIAL_INLINE size_t encodeSleb128(const i64 value, Vec<u8> &buffer) {
        return encodeUleb128(zigzagEncode(value), buffer);
    }

    // 解码有符号数
    CIAL_INLINE i64 decodeSleb128(const u8 *ptr, u64 &pc, const size_t maxLen) {
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

    class Bytecode {
        struct Info {
            Vec<u8> code{};
            Map<u32, u32> tacPosToBcPos{};
            Map<u32, u32> jmpTagetPos{};
        };

    public:
        static Vec<u8> compile(const Vec<Box<inter::Instruction>> &instructions) {
            Info info{};
            auto &[code, tacPosToBcPos, jmpTagetPos] = info;
            for(u32 i = 0; i < instructions.size(); ++i) {
                if(tacPosToBcPos.contains(i)) {
                    jmpTagetPos[tacPosToBcPos[i]] = static_cast<u32>(code.size());
                }

                switch(inter::Instruction *inst = instructions[i].get(); inst->opcode()) {
#define ENCODE_CASE(op)                                                                                                \
    case inter::TacOpCode::op:                                                                                         \
        encode(info, dynamic_cast<inter::op *>(inst));                                                                 \
        break;
                    CIAL_TAC_OPCODE_ENUMS(ENCODE_CASE)
#undef ENCODE_CASE
                }
            }

            // update jmp target address
            for(auto &[tp, bp] : jmpTagetPos) {
                std::memcpy(&code[tp], &bp, sizeof(bp));
            }

            return std::move(code);
        }

    private:
#define ENCODE_METHOD(name) static void encode(Info &info, const inter::name *tac);
        CIAL_TAC_OPCODE_ENUMS(ENCODE_METHOD)
#undef ENCODE_METHOD
    };
} // namespace cial::vm
