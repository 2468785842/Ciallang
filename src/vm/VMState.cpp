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
#include "VMState.hpp"

#include "Instruction.hpp"
#include "types/Class.hpp"

namespace cial::Bytecode {

    void VMState::run() {
        std::uint64_t &pc = _currentFrame->pc;
        const auto &instList = instructions();
        const size_t curStackTop = _stackTop;
// #define CLL_COMPUTED_GOTO
#ifdef CLL_COMPUTED_GOTO
        // 标签数组
        static void *labels[] = {
#define HANDLE_OPCODE(OP) &&label_##OP,
            OPCODE_ENUMS(HANDLE_OPCODE)
#undef HANDLE_OPCODE
        };

        goto label_Dispatch;

    label_Dispatch: {
        if(!(pc < instList.size() && _stackTop != 0 && curStackTop == _stackTop)) {
            return;
        }
        goto *labels[static_cast<size_t>(instList[pc]->opcode)];
    }

#define HANDLE_OPCODE(OP)                                                                                              \
    label_##OP : {                                                                                                     \
        OP::execute(*instList[pc++], *this);                                                                           \
        goto label_Dispatch;                                                                                           \
    }

        OPCODE_ENUMS(HANDLE_OPCODE)
#undef HANDLE_OPCODE
#else

        while(pc < instList.size() && _stackTop != 0 && curStackTop == _stackTop) {
            const auto *instruction = instList[pc++];
            // fmt::println("{}\n", Instruction::dump(*instruction, *this, true));
            Instruction::execute(*instruction, *this);
        }
#endif
    }

    [[nodiscard]] bool VMState::globalHas(const Atom atom) const { return context.global()->hasProp(atom); }

    [[nodiscard]] bool VMState::globalHas(const std::string &name) const {
        const auto atom = rt.atomTable.intern(name.c_str(), name.length());
        return context.global()->hasProp(atom);
    }

    [[nodiscard]] Value VMState::global(const Atom atom) const { return context.global()->getProp(atom); }

    void VMState::global(const Atom atom, const Value &value) const { context.global()->setProp(atom, value); }

    [[nodiscard]] Value VMState::global(const std::string &name) const {
        const auto atom = rt.atomTable.intern(name.c_str(), name.length());
        return context.global()->getProp(atom);
    }

    void VMState::global(const std::string &name, const Value &value) const {
        const auto atom = rt.atomTable.intern(name.c_str(), name.length());
        context.global()->setProp(atom, value);
    }

    void VMState::reg(const Register &reg, const Value &value) const { _currentFrame->getReg(reg) = value; }

    Value VMState::reg(const Register reg) const { return _currentFrame->getReg(reg); }

    Value &VMState::regRef(const Register reg) const { return _currentFrame->getReg(reg); }

    bool VMState::hasThis(const Atom atom) const {

        // current thisObj
        if(_currentFrame->thisObj.isObject()) {
            if(auto *instanceObject = dynamic_cast<DataObject *>(_currentFrame->thisObj.asObject().value())) {
                if(instanceObject->hasProp(atom)) {
                    return true;
                }
            }
        }

        if(auto *callFrame = _currentFrame->closure; callFrame) {
            if(callFrame->funcMeta) {
                for(const auto &localVar : callFrame->funcMeta->localVars) {
                    if(localVar.endPC > callFrame->pc)
                        continue;
                    if(localVar.identifier == atom)
                        return true;
                }
            }

            // prev context
            // if(callFrame->thisObj.isObject()) {
            //     if(auto *instanceObject = dynamic_cast<InstanceObject *>(_currentFrame->thisObj.asObject().value()))
            //     {
            //         return instanceObject->getProp(atom);
            //     }
            // }
        }

        // global
        return globalHas(atom);
    }

    Value VMState::getThis(const Atom atom) const {

        // current thisObj
        if(_currentFrame->thisObj.isObject()) {
            if(auto *instanceObject = dynamic_cast<DataObject *>(_currentFrame->thisObj.asObject().value())) {
                if(instanceObject->hasProp(atom)) {
                    return instanceObject->getProp(atom);
                }
            }
        }

        if(auto *callFrame = _currentFrame->closure; callFrame) {
            if(callFrame->funcMeta) {
                for(const auto &localVar : callFrame->funcMeta->localVars) {
                    if(localVar.endPC > callFrame->pc)
                        continue;
                    if(localVar.identifier == atom)
                        return callFrame->getReg(localVar.reg);
                }
            }

            // prev context
            // if(callFrame->thisObj.isObject()) {
            //     if(auto *instanceObject = dynamic_cast<InstanceObject *>(_currentFrame->thisObj.asObject().value()))
            //     {
            //         return instanceObject->getProp(atom);
            //     }
            // }
        }

        // global
        // TODO: check is exist
        return global(atom);
    }

    void VMState::setThis(const Atom atom, const Value &v) const {
        // current thisObj
        if(_currentFrame->thisObj.isObject()) {
            if(auto *instanceObject = dynamic_cast<DataObject *>(_currentFrame->thisObj.asObject().value())) {
                if(instanceObject->hasProp(atom)) {
                    instanceObject->setProp(atom, v);
                    return;
                }
            }
        }

        if(auto *callFrame = _currentFrame->closure; callFrame) {
            if(callFrame->funcMeta) {
                for(const auto &localVar : callFrame->funcMeta->localVars) {
                    if(localVar.endPC > callFrame->pc)
                        continue;
                    if(localVar.identifier == atom)
                        callFrame->getReg(localVar.reg) = v;
                }
            }
        }

        // global
        return global(atom, v);
    }

    Value VMState::getUpVal(const Atom atom) const {

        // current context
        if(_currentFrame->thisObj.isObject()) {
            if(auto *instanceObject = dynamic_cast<DataObject *>(_currentFrame->thisObj.asObject().value())) {
                return instanceObject->getProp(atom);
            }
        }

        for(std::uint16_t i = _stackTop - 1; i > 0; --i) {
            const auto &callFrame = _callStack[i - 1];
            // prev local scope
            if(callFrame.funcMeta) {
                for(const auto &localVar : callFrame.funcMeta->localVars) {
                    if(localVar.endPC > callFrame.pc)
                        continue;
                    if(localVar.identifier == atom)
                        return callFrame.getReg(localVar.reg);
                }
            }

            // prev context
            if(callFrame.thisObj.isObject()) {
                if(auto *instanceObject = dynamic_cast<DataObject *>(_currentFrame->thisObj.asObject().value())) {
                    return instanceObject->getProp(atom);
                }
            }
        }

        // global
        // TODO: check is exist
        return global(atom);
    }
} // namespace cial::Bytecode
