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
#include "Runtime.hpp"
#include "gen/IRGenerator.hpp"

#include "types/Value.hpp"

#include "vm/Register.hpp"

namespace Cial::Bytecode {
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

        explicit VMState(Runtime &rt) : rt(rt) {}

        void run();

        void reg(const Register &reg, const Value &value) const;

        [[nodiscard]] Value reg(Register reg) const;

        [[nodiscard]] Value global(const Atom atom) const { return rt.gObj.contains(atom) ? rt.gObj[atom] : Value{}; }

        void global(const Atom atom, const Value &value) const { rt.gObj[atom] = value; }

        [[nodiscard]] Value global(const std::string &name) const {
            const auto atom = rt.atomTable.intern(name.c_str(), name.length());
            Value v = rt.gObj.contains(atom) ? rt.gObj[atom] : Value{};
            return v;
        }

        void global(const std::string &name, const Value &value) const {
            const auto atom = rt.atomTable.intern(name.c_str(), name.length());
            rt.gObj[atom] = value;
        }

        void setZF(const bool zf) { _zf = zf; }

        [[nodiscard]] bool getZF() const { return _zf; }

        void setPC(const Label &label) const { _callStack[_stackTop - 1].pc = label.address(); }

        [[nodiscard]] size_t getPC() const { return _currentFrame->pc; }

        void allocCallFrame(const Chunk *chunk, const std::optional<Register> &ret = {}) {
            if(_stackTop >= Runtime::maxCallDepth)
                throw std::runtime_error("Call stack overflow");
            _currentFrame = new(&_callStack[_stackTop++]) CallFrame{ chunk, ret, rt.regPool };
        }

        void freeCallFrame() {
            if(_stackTop == 0)
                throw std::runtime_error("Call stack underflow");
            _currentFrame = _stackTop > 0 ? &_callStack[--_stackTop - 1] : nullptr;
            _callStack[_stackTop].~CallFrame();
            new(&_callStack[_stackTop]) CallFrame{};
        }

        [[nodiscard]] const std::vector<Op::Instruction *> &instructions() const noexcept {
            return _currentFrame->chunk->getInstVec();
        }

        [[nodiscard]] CallFrame *curFrame() noexcept { return _currentFrame; }
        [[nodiscard]] const CallFrame *curFrame() const noexcept { return _currentFrame; }

        [[nodiscard]] CallFrame *prev() noexcept { return &_callStack[_stackTop - 2]; }
        [[nodiscard]] const CallFrame *prev() const noexcept { return &_callStack[_stackTop - 2]; }

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

        [[nodiscard]] std::string dumpInstruction(const Chunk &chunk) const {
            std::stringstream ss{};
            // size_t pc{};
            // std::vector<Function *> functions{};
            // while(pc < chunk.getInstVec().size()) {
            //     const auto &instruction = chunk.getInstVec()[pc];
            //
            //     ss << fmt::format("{: <6}: {}\n", Label{ pc }, Op::Instruction::dump(*instruction, *this, false));
            //
            //     if(instruction->opcode == Op::OpCode::Load) {
            //         if(auto value = curFrame()->chunk->getConstant(Op::Load::value(*instruction)); value.isObject())
            //         {
            //             if(auto fun = dynamic_cast<Function *>(value.toObject())) {
            //                 functions.push_back(fun);
            //             }
            //         }
            //     }
            //
            //     ++pc;
            // }
            //
            // for(const auto &fun : functions) {
            //     ss << fmt::format("{:=^30}\n", fmt::format(" function {} ", fun->getName()))
            //        << dumpInstruction(*fun->chunk());
            // }

            return ss.str();
        }

        void pushVoid(const size_t n) const {
            const size_t base = rt.regPool.allocFrame(n);
            for(std::size_t i = 0; i < n; i++) {
                *rt.regPool.ptrAt(base) = Value{};
            }
        }

        void push(const Value &v) const { *rt.regPool.ptrAt(rt.regPool.allocFrame(1)) = v; }

        void pop(const size_t count) const { rt.regPool.freeFrame(count); }

        [[nodiscard]] size_t getRegPoolTop() const { return rt.regPool.used(); }

    private:
        CallFrame *_currentFrame{ rt.callStack };
        CallFrame *_callStack{ rt.callStack };
        size_t _stackTop{ rt.stackTop }; // callFrame count
        bool _zf{ false };
    };
} // namespace Cial::Bytecode
