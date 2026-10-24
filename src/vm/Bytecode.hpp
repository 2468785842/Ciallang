//
// Created by LiDong on 2026/1/27.
//
#pragma once

#include "config.h"
#include "gen/Instruction.hpp"
#include "types/Types.hpp"

// 扁平字节码指令集：与 TAC 指令集(CIAL_TAC_OPCODE_ENUMS)一一对应，
// 共 55 条。编码约定：
//   - 寄存器操作数：u16
//   - Atom：u32（Atom.v 是顺序索引，实际范围远小于 2^32）
//   - 立即数：sLEB128（变长）
//   - 跳转目标：u32 占位，编译结束后按 tacIdx→bcPos 映射统一回填
#define CIAL_BYTECODE_OPCODE_ENUMS(O)                                                                                  \
    O(NOP)                                                                                                             \
    O(Debugger)                                                                                                        \
    O(Push)                                                                                                            \
    O(PopN)                                                                                                            \
    O(Global)                                                                                                          \
    O(Super)                                                                                                           \
    O(This)                                                                                                            \
    O(Test)                                                                                                            \
    O(Inv)                                                                                                             \
    O(Throw)                                                                                                           \
    O(Ret)                                                                                                             \
    O(Jmp)                                                                                                             \
    O(JmpE)                                                                                                            \
    O(JmpNE)                                                                                                           \
    O(ToInt)                                                                                                           \
    O(ToReal)                                                                                                          \
    O(Asc)                                                                                                             \
    O(Chr)                                                                                                              \
    O(ToString)                                                                                                        \
    O(TypeOf)                                                                                                           \
    O(LNot)                                                                                                            \
    O(ChgSign)                                                                                                         \
    O(ChgThis)                                                                                                         \
    O(ChkInv)                                                                                                          \
    O(ChkIns)                                                                                                          \
    O(TestVoid)                                                                                                        \
    O(Load)                                                                                                            \
    O(LoadImm)                                                                                                         \
    O(DGlobal)                                                                                                         \
    O(GGlobal)                                                                                                         \
    O(GThis)                                                                                                           \
    O(DThis)                                                                                                           \
    O(GLocal)                                                                                                          \
    O(DLocal)                                                                                                          \
    O(Add)                                                                                                             \
    O(Sub)                                                                                                             \
    O(Mul)                                                                                                             \
    O(Div)                                                                                                             \
    O(Idiv)                                                                                                            \
    O(Mod)                                                                                                             \
    O(BXor)                                                                                                            \
    O(BOr)                                                                                                             \
    O(BAnd)                                                                                                            \
    O(BlShift)                                                                                                         \
    O(BrShift)                                                                                                         \
    O(BurShift)                                                                                                        \
    O(EQ)                                                                                                              \
    O(NEQ)                                                                                                             \
    O(AbsEQ)                                                                                                           \
    O(AbsNEQ)                                                                                                          \
    O(LT)                                                                                                              \
    O(LE)                                                                                                              \
    O(GT)                                                                                                              \
    O(GE)                                                                                                              \
    O(LAnd)                                                                                                            \
    O(LOr)                                                                                                             \
    O(Call)                                                                                                            \
    O(GProp)                                                                                                           \
    O(DProp)                                                                                                           \
    O(DelProp)                                                                                                         \
    O(GPropRaw)                                                                                                        \
    O(GRefRaw)                                                                                                         \
    O(DPropRaw)                                                                                                        \
    O(DerefGet)                                                                                                        \
    O(DerefSet)

namespace cial::vm {
    class VMState;

    enum class VmOpCode : u8 {
#define OPCODE_ENUM_CLASS(OP) OP,
        CIAL_BYTECODE_OPCODE_ENUMS(OPCODE_ENUM_CLASS)
#undef OPCODE_ENUM_CLASS
    };

    // try 区间表（pc 语义由使用者决定：TAC 下标或扁平字节码偏移）
    struct ThrowHandler {
        u64 tryStart;
        u64 tryEnd;
        u16 exValueReg;
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
    // 标准公式 (v >> 1) ^ -(v & 1)。旧实现 -(v >> 1) 对所有奇数编码值
    // (负数)差 1:zigzag(-1)=1 会被解成 0,LoadImm 负立即数全错
    CIAL_INLINE i64 zigzagDecode(const u64 v) noexcept {
        return static_cast<i64>(v >> 1) ^ -(static_cast<i64>(v & 1));
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
            // tacIdx -> 该 TAC 指令（编码后）在扁平字节码中的起始偏移；
            // 大小 = 指令数 + 1，末项为整体末尾（供指向 chunk 尾部的标签使用）
            Vec<u32> tacStartPos{};
            // (占位符在 code 中的偏移, 跳转目标的 tacIdx)，编译结束后统一回填。
            // 多条跳转可以指向同一目标（switch 的多个 break 等），必须用列表而不是 1:1 map
            Vec<std::pair<u32, u32>> jmpPatches{};
        };

    public:
        struct CompileResult {
            Vec<u8> code{};
            Vec<ThrowHandler> handlers{}; // 已换算到扁平字节码 pc 空间的 try 区间表
        };

        static CompileResult compile(const Vec<Box<inter::Instruction>> &instructions,
                                     const Vec<ThrowHandler> &tacHandlers) {
            Info info{};
            auto &[code, tacStartPos, jmpPatches] = info;

            tacStartPos.resize(instructions.size() + 1);

            for(u32 i = 0; i < instructions.size(); ++i) {
                tacStartPos[i] = static_cast<u32>(code.size());

                switch(inter::Instruction *inst = instructions[i].get(); inst->opcode()) {
#define ENCODE_CASE(op)                                                                                                \
    case inter::TacOpCode::op:                                                                                         \
        encode(info, dynamic_cast<inter::op *>(inst));                                                                 \
        break;
                    CIAL_TAC_OPCODE_ENUMS(ENCODE_CASE)
#undef ENCODE_CASE
                }
            }
            tacStartPos[instructions.size()] = static_cast<u32>(code.size());

            // 回填所有跳转目标（前向/后向统一处理）
            for(const auto &[patchPos, tacIdx] : jmpPatches) {
                const u32 target = tacStartPos[tacIdx];
                std::memcpy(&code[patchPos], &target, sizeof(target));
            }

            CompileResult result{};
            result.code = std::move(code);
            for(const auto &h : tacHandlers) {
                result.handlers.push_back(ThrowHandler{ tacStartPos[static_cast<u32>(h.tryStart)],
                                                        tacStartPos[static_cast<u32>(h.tryEnd)], h.exValueReg });
            }
            return result;
        }

    private:
#define ENCODE_METHOD(name) static void encode(Info &info, const inter::name *tac);
        CIAL_TAC_OPCODE_ENUMS(ENCODE_METHOD)
#undef ENCODE_METHOD

        // 跳转类指令公共编码：opcode + u32 占位符，占位符登记进回填表
        static void encodeJmp(Info &info, const VmOpCode op, const size_t targetTacIdx) {
            info.code.push_back(static_cast<u8>(op));
            info.jmpPatches.emplace_back(static_cast<u32>(info.code.size()), static_cast<u32>(targetTacIdx));
            write<u32>(info.code, 0);
        }
    };
} // namespace cial::vm
