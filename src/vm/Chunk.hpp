/*
 * Copyright (c) 2024/6/13 下午8:15
 *
 * /\  _` \   __          /\_ \  /\_ \
 * \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
 *  \ \ \/_/_\/\ \  /'__`\  \ \ \  \ \ \   /'__`\  /' _ `\  /'_ `\
 *   \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
 *    \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
 *     \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
 *                                                            /\____/
 *                                                            \_/__/
 *
 */
#pragma once

#include "Instruction.hpp"

namespace Ciallang::Bytecode {
    class Chunk {
    public:
        ~Chunk() {
            for (const auto &inst : _instructions) {
                inst->~Instruction();
            }
            _instructions.clear();
        }
        /**
         * @tparam OP 操作码
         * @tparam Args 指令的值类型
         * @param args 值数组
         * @return 指令在内存的索引
         */
        template <Op::OpCode OP, typename... Args>
        Op::Instruction *emit(Args &&...args) {
            auto *inst = new Op::Instruction(OP, Op::Operand(std::forward<Args>(args))...);
            _instructions.push_back(inst);
            return inst;
        }

        explicit Chunk() = default;

        [[nodiscard]] auto &instructions() const noexcept { return _instructions; }

        void setRegisterCount(const std::uint32_t count) noexcept { _registerCount = count; }
        [[nodiscard]] std::uint32_t getRegCount() const noexcept { return _registerCount; }

        Chunk(const Chunk &) = delete;
        Chunk &operator=(const Chunk &) = delete;

    private:
        std::vector<Op::Instruction*> _instructions{};
        std::uint32_t _registerCount{ 0 };
    };
} // namespace Ciallang::Bytecode
