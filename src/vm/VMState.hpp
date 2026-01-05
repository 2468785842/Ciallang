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

#include <fmt/format.h>

#include "Chunk.hpp"
#include "FastRegisterPool.hpp"
#include "runtime/Context.hpp"
#include "runtime/Runtime.hpp"

#include "types/Value.hpp"

#include "vm/Register.hpp"

namespace cial::Bytecode {
    // enum class ContextType {
    //     TopLevel,
    //     Function,
    //     ExprFunction,
    //     Property,
    //     PropertySetter,
    //     PropertyGetter,
    //     Class,
    //     SuperClassGetter,
    // };

    class VMState {
    public:
        Runtime &rt;
        Context &context;

        explicit VMState(Context &context) : rt(context.rt), context(context) {}

        void run();

        void runFlat();

        void reg(const Register &reg, const Value &value) const;

        [[nodiscard]] Value reg(Register reg) const;

        [[nodiscard]] Value &regRef(Register reg) const;

        [[nodiscard]] Value global(const Atom atom) const {
            return context.gObj.contains(atom) ? context.gObj[atom] : Value{};
        }

        void global(const Atom atom, const Value &value) const { context.gObj[atom] = value; }

        [[nodiscard]] Value global(const std::string &name) const {
            const auto atom = rt.atomTable.intern(name.c_str(), name.length());
            Value v = context.gObj.contains(atom) ? context.gObj[atom] : Value{};
            return v;
        }

        void global(const std::string &name, const Value &value) const {
            const auto atom = rt.atomTable.intern(name.c_str(), name.length());
            context.gObj[atom] = value;
        }

        void setZF(const bool zf) { _zf = zf; }

        [[nodiscard]] bool getZF() const { return _zf; }

        void setPC(const Label &label) const { _currentFrame->pc = label.address(); }

        [[nodiscard]] size_t getPC() const { return _currentFrame->pc; }

        template <typename T>
        void allocCallFrame(T *arg, const OptReg &ret = {}) {
            if(_stackTop >= Context::maxCallDepth)
                throw std::runtime_error("Call stack overflow");
            _currentFrame = new(&_callStack[_stackTop++]) CallFrame{ arg, ret, context.regPool };
        }

        void freeCallFrame() {
            if(_stackTop == 0)
                throw std::runtime_error("Call stack underflow");
            _currentFrame = _stackTop > 0 ? &_callStack[--_stackTop - 1] : nullptr;
            _callStack[_stackTop].~CallFrame();
        }

        [[nodiscard]] const Vec<Op::Instruction *> &instructions() const noexcept {
            return _currentFrame->chunk->getInstVec();
        }

        [[nodiscard]] CallFrame *curFrame() noexcept { return _currentFrame; }
        [[nodiscard]] const CallFrame *curFrame() const noexcept { return _currentFrame; }

        [[nodiscard]] CallFrame *prev() noexcept { return _currentFrame - 1; }
        [[nodiscard]] const CallFrame *prev() const noexcept { return _currentFrame - 1; }

        [[nodiscard]] Value getThis(Atom atom) const;
        [[nodiscard]] Value getUpVal(Atom atom) const;

        [[nodiscard]] std::string dumpRegisters() const {
            std::stringstream ss{};
            for(size_t i = 0; i < _stackTop; i++) {
                const auto &call = _callStack[i];
                for(size_t j = 0; j < call.chunk->getRegCount(); j++) {
                    ss << fmt::format("(%{}): {}\n", j, call.getReg(Register{ j }));
                }
            }
            return ss.str();
        }

        void pushVoid(const size_t n) const {
            const size_t base = context.regPool.allocFrame(n);
            for(std::size_t i = 0; i < n; i++) {
                *context.regPool.ptrAt(base) = Value{};
            }
        }

        void push(const Value &v) const { *context.regPool.ptrAt(context.regPool.allocFrame(1)) = v; }

        void pop(const size_t count) const { context.regPool.freeFrame(count); }

        [[nodiscard]] size_t getRegPoolTop() const { return context.regPool.used(); }

    private:
        CallFrame *_currentFrame{ context.callStack };
        CallFrame *_callStack{ context.callStack };
        size_t &_stackTop{ context.stackTop }; // callFrame count
        bool _zf{ false };
    };
} // namespace cial::Bytecode
