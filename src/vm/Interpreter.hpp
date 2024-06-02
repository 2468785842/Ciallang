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

#include "VMChunk.hpp"
#include "../types/TjsValue.hpp"
#define STACK_MAX 256

namespace Ciallang::VM {
    class VMChunk;

    enum class InterpretResult {
        OK, // 正常结束
        CONTINUE, // 继续解释
        COMPILE_ERROR, // 编译错误
        RUNTIME_ERROR // 运行错误
    };

    class Interpreter {
        friend struct Instruction;

    public:
        explicit Interpreter(VMChunk*);

        InterpretResult run();

        TjsValue readConstant();

        void push(TjsValue&& value);
        TjsValue pop();
        void printStack();

    private:
        struct {
            VMChunk* chunk;
            uint8_t* ip;
            TjsValue stack[STACK_MAX];
            TjsValue* sp = stack;

            bool flags = false; /* 跳转标记 */
        } _vm;
    };
}
