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

        template <typename T, typename... Args>
            requires std::is_base_of_v<Op::Instruction, T>
        T* emit(Args&&... args) {
            T* ins = new T{ std::forward<Args>(args)... };
            _instructions.push_back(ins);
            return ins;
        }

        auto& instructions() const {
            return _instructions;
        }

    private:
        std::vector<Op::Instruction*> _instructions{};
    };
}
