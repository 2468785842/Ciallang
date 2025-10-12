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

#include "pch.h"

#include "Instruction.hpp"

namespace Ciallang::Bytecode {
    class Chunk {
    public:
        ~Chunk() noexcept;

        template <Op::OpCode OP, typename... Args>
        auto *emit(Args &&...args) {
            auto *ins = new Op::Instruction{ OP, Op::Operand(std::forward<Args>(args))... };
            _instructions.push_back(ins);
            return ins;
        }

        [[nodiscard]] auto &instructions() const noexcept { return _instructions; }

        void setRegisterCount(const std::uint32_t count) noexcept { _registerCount = count; }
        [[nodiscard]] std::uint32_t getRegisterCount() const noexcept { return _registerCount; }

    private:
        std::vector<Op::Instruction *> _instructions{};
        std::uint32_t _registerCount{ 0 };
    };
} // namespace Ciallang::Bytecode
