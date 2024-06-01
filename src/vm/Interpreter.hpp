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

#include "../types/TjsValue.hpp"

namespace Ciallang::VM {
    class VMChunk;

    enum class InterpretResult {
        OK,
        COMPILE_ERROR,
        RUNTIME_ERROR
    };

    class Interpreter {

    public:
        explicit Interpreter(VMChunk*);

        InterpretResult run();

        TjsValue readConstant();

    private:
        struct {
            VMChunk* chunk;
            uint8_t* ip;
            bool flags; //跳转标记*/
        } _vm;
    };
}
