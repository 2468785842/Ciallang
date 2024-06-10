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
#include "../common/ConstExpr.hpp"
#include "../types/TjsValue.hpp"
#include "../common/SourceFile.hpp"
#include "../types/TjsFunction.hpp"

#define STACK_MAX 256

namespace Ciallang::VM {
    class VMChunk;

    enum class InterpretResult {
        OK,            // 正常结束
        CONTINUE,      // 继续解释
        COMPILE_ERROR, // 编译错误
        RUNTIME_ERROR  // 运行错误
    };

    struct CallFrame {
        const VMChunk* vmChunk;
        size_t ip;
        TjsValue* slots;
        bool sub;
    };

    class Interpreter {
        template <Common::Name, size_t, Opcode>
        friend struct MakeInstruction;

    public:
        explicit Interpreter(Common::SourceFile& sourceFile, VMChunk&& chunk);

        InterpretResult run(Common::Result& r);

        void error(
            Common::Result& r,
            const std::string& message) const {
            _sourceFile.error(r, message,
                chunk()->rlc()->find(
                    chunk()->bytecodes(callFrame().ip)
                )
            );
        }

        void ip(const int16_t relativeIdx) noexcept {
            _vm.frames[_vm.frameCount].ip += relativeIdx;
        }

        [[nodiscard]] size_t ip() const noexcept {
            return callFrame().ip;
        }

        [[nodiscard]] const VMChunk* chunk() const {
            return callFrame().vmChunk;
        }

        [[nodiscard]] uint8_t readByte();

        [[nodiscard]] const TjsValue& readConstant(size_t index) const;

        [[nodiscard]] const TjsValue& peek(size_t distance) const;

        void pushVoid();

        void push(TjsValue&& value);

        TjsValue popN(size_t n);

        TjsValue pop();

        void putStack(size_t slot, TjsValue&& value);

        [[nodiscard]] const TjsValue& getStack(size_t slot) const;

        void putGlobal(std::string&& key, TjsValue&& value);

        [[nodiscard]] TjsValue* getGlobal(const std::string_view& key) const;

        void printStack();
        void printGlobal();

        ~Interpreter() noexcept {
            for(auto& val : _vm.globals | std::views::values) {
                delete val;
            }
            delete _main;
        }

        bool call(const TjsFunction* const fun) {

            _vm.frames[++_vm.frameCount] = {
                    fun->chunk().get(), 0, _vm.sp - 1, false
            };

            // default parameter;
            for(size_t i = 0; i < fun->parameters()->size(); i++) {
                _vm.frames[++_vm.frameCount] = {
                        fun->parameters()->at(i).get(), 0, _vm.sp + i - 1, true
                };
            }

            return true;
        }

        void resovleParameter(TjsValue&& val) const {
            if(_vm.frames[_vm.frameCount].slots->type() == TjsValueType::Void) {
                *_vm.frames[_vm.frameCount].slots = std::move(val);
            }
        }

        [[nodiscard]] bool isSubCallFrame() const { return callFrame().sub; }

        [[nodiscard]] const CallFrame& callFrame() const {
            DCHECK_GE(_vm.frameCount, 0) << "call frames underflow";
            return _vm.frames[_vm.frameCount];
        }

        void callFrameRet() { --_vm.frameCount; }

        [[nodiscard]] bool isCallFrameTop() const { return _vm.frameCount == 0; }

    private:
        TjsFunction* _main;
        Common::SourceFile& _sourceFile;

        struct {
            std::unordered_map<std::string, TjsValue*> globals{};

            CallFrame frames[64];
            size_t frameCount{ 0 };
            TjsValue stack[STACK_MAX];
            TjsValue* sp = stack;
        } _vm{};

    public:
        [[nodiscard]] std::string disassembleLine() const {
            auto& [line, column] =
                    chunk()->rlc()->find(ip()).start();

            return !chunk()->rlc()->firstAppear(ip())
                   ? fmt::format("{: <14}", fmt::format("@{}:{}", line + 1, column + 1))
                   : fmt::format("{: <14}", "~");
        }

        void dump();
    };
}
