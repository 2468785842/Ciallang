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

#include <cstdlib>
#include <fmt/format.h>

#include "CallFrame.hpp"
#include "Chunk.hpp"
#include "FastRegisterPool.hpp"
#include "runtime/Context.hpp"
#include "runtime/Runtime.hpp"

#include "types/Value.hpp"

#include "gen/Register.hpp"

namespace cial::vm {

    enum class PendingCF { None, Throw };

    class VMState {
    public:
        Runtime &rt;
        Context &context;

        explicit VMState(Context &context) : rt(context.rt()), context(context) {}

        void run();
        void run0();

        [[nodiscard]] Value reg(u16 reg) const;

        [[nodiscard]] Value &regRef(u16 reg) const;

        [[nodiscard]] bool globalHas(Atom atom) const;
        [[nodiscard]] bool globalHas(const std::string &name) const;
        [[nodiscard]] Value global(Atom atom) const;

        void global(Atom atom, const Value &value) const;

        [[nodiscard]] Value global(const std::string &name) const;

        void global(const std::string &name, const Value &value) const;
        [[nodiscard]] bool hasThis(Atom atom) const;
        [[nodiscard]] Value getThis(Atom atom) const;
        void setThis(Atom atom, const Value &v) const;
        [[nodiscard]] Value getUpVal(Atom atom) const;

        void setZF(const bool zf) { _zf = zf; }

        [[nodiscard]] bool getZF() const { return _zf; }

        void setPC(const u64 pc) const {
            // because run() pc will auto plus one so - 1
            _currentFrame->pc = pc - 1;
        }

        [[nodiscard]] u64 getPC() const { return _currentFrame->pc; }

        void pushVoid(const size_t n) const {
            const size_t base = context.regPool().allocFrame(n);
            for(size_t i = 0; i < n; i++) {
                *context.regPool().ptrAt(base + i) = Value{};
            }
        }

        void push(const Value &v) const { *context.regPool().ptrAt(context.regPool().allocFrame(1)) = v; }

        void pop(const size_t count) const { context.regPool().freeFrame(count); }

        void makeClosure() { _currentFrame->closure = _stackTop > 1 ? prevFrame() : nullptr; }

        template <typename T>
        void allocCallFrame(T *arg, const Opt<u16> ret = {}) {
            if(_stackTop >= Context::maxCallDepth)
                throw std::runtime_error("Call stack overflow");
            _currentFrame = new(&_callStack[_stackTop++]) CallFrame{ arg, ret, context.regPool() };
        }

        void freeCallFrame() {
            if(_stackTop == 0)
                throw std::runtime_error("Call stack underflow");
            _currentFrame = _stackTop > 0 ? &_callStack[--_stackTop - 1] : nullptr;
            _callStack[_stackTop].~CallFrame();
        }

        [[nodiscard]] size_t getRegPoolTop() const { return context.regPool().used(); }

        [[nodiscard]] CallFrame *curFrame() noexcept { return _currentFrame; }
        [[nodiscard]] const CallFrame *curFrame() const noexcept { return _currentFrame; }

        [[nodiscard]] CallFrame *prevFrame() noexcept { return _currentFrame - 1; }
        [[nodiscard]] const CallFrame *prevFrame() const noexcept { return _currentFrame - 1; }

        void unwind();

        void throwException(const Value &v) {
            _pending = PendingCF::Throw;
            _exValue = v;
            if(_exValue.isObject()) {
                context.rt().addHandleVal(_exValue.asObject().value());
            }
        }

        void clearException() {
            if(_exValue.isObject()) {
                context.rt().removeHandleVal(_exValue.asObject().value());
            }
            _pending = PendingCF::None;
            _exValue = Value{};
        }

        [[nodiscard]] std::string dumpCurConstants() const {
            return _currentFrame->chunk->dumpConstants(&context.rt()).toStdStr();
        }

        [[nodiscard]] std::string dumpCurInstructions() const {
            return _currentFrame->chunk->dumpInstructions(this).toStdStr();
        }

        [[nodiscard]] std::string dumpCurLocalVars() const {
            std::stringstream ss{};
            for(size_t i = 0; i < _stackTop; i++) {
                const auto &call = _callStack[i];
                if(!call.funcMeta)
                    continue;

                for(const auto &[identifier, reg, startPC, endPC] : call.funcMeta->localVars) {
                    if(startPC.address() <= getPC() && getPC() < endPC.address())
                        continue;
                    const auto *entry = context.rt().atomTable.get(identifier);
                    auto varName = "?unknow_var_name?"_str;
                    if(entry)
                        varName = *entry->str;
                    ss << fmt::format("{} = {}\n", varName, call.getReg(reg.index()));
                }
            }
            return ss.str();
        }

        [[nodiscard]] std::string dumpCurRegisters() const {
            std::stringstream ss{};
            for(u32 j = 0; j < _currentFrame->chunk->getRegCount(); j++) {
                ss << fmt::format("(%{}): {}\n", j, _currentFrame->getReg(j));
            }
            return ss.str();
        }

    private:
        CallFrame *_currentFrame{ context.callStack() };
        CallFrame *_callStack{ context.callStack() };
        size_t &_stackTop{ context.stackTop() }; // callFrame count
        bool _zf{ false };
        PendingCF _pending{};
        Value _exValue{};
    };
} // namespace cial::vm
