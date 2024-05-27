/*
 * Copyright (c) 2024/5/8 上午8:08
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
#include <cstdint>

#include "VMTypes.hpp"

namespace Ciallang::VM {
    class CiallangVM {
    public:
        bool initializer();
    private:
        //10M
        uint64_t registers[1024 * 1024 * 10]{};
        uint64_t constants[1024 * 1024 * 10]{};
        Instruction instructions[1024 * 1024 * 10]{};
        uint32_t PC{};
        bool flags{};//跳转标记
    };
}
