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

#include <ranges>

#include "VMChunk.hpp"
#include "../types/TjsValue.hpp"
#include "../common/SourceFile.hpp"

#define STACK_MAX 256

namespace Ciallang::VM {
    class VMChunk;

    enum class InterpretResult {
        OK,            // 正常结束
        CONTINUE,      // 继续解释
        COMPILE_ERROR, // 编译错误
        RUNTIME_ERROR  // 运行错误
    };

    class Interpreter {
        friend struct Instruction;

    public:
        explicit Interpreter(Common::SourceFile& sourceFile, VMChunk*);

        InterpretResult run(Common::Result& r);

        void error(
            Common::Result& r,
            const std::string& message) const {
            _sourceFile.error(r, message,
                _vm.chunk->rlc()->find(
                    _vm.chunk->bytecodes()[_vm.ip]
                )
            );
        }


        [[nodiscard]] uint8_t readByte();
        [[nodiscard]] TjsValue readConstant();
        [[nodiscard]] TjsValue& peek(size_t distance) const;

        void pushVoid();

        void push(TjsValue&& value);

        TjsValue& popN(size_t n);

        TjsValue& pop();

        void putStack(size_t slot, TjsValue&& value);

        TjsValue& getStack(size_t slot);

        void putGlobal(std::string&& key, TjsValue&& value);

        [[nodiscard]] TjsValue* getGlobal(const std::string_view &key) const;

        void printStack();
        void printGlobal();

        ~Interpreter() noexcept {
            for(auto& val : _vm.globals | std::views::values) {
                delete val;
            }
        }

    private:
        Common::SourceFile& _sourceFile;

        struct VM {
            std::unordered_map<std::string, TjsValue*> globals{};

            VMChunk* chunk;
            size_t ip{ 0 }; // 栈是动态的, 满了重新分配,地址可能改变,不能用指针
            TjsValue stack[STACK_MAX];
            TjsValue* sp = stack;

            bool flags = false; /* 跳转标记 */
        } _vm;
    };
}
