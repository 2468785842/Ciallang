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
#include "common/Defer.hpp"
#include "types/Class.hpp"

namespace Cial::Bytecode {

    void VMState::runFlat() {
        const size_t sourceSP = _currentFrame->_sp;
        DEFER { _currentFrame->_sp = sourceSP; };
        _currentFrame->_sp = prev()->_sp;
        run();
    }

    void VMState::run() {
        std::uint64_t &pc = _currentFrame->pc;
        const auto &instList = instructions();
        const size_t curStackTop = _stackTop;
#define CLL_COMPUTED_GOTO
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
        Op::OP::execute(*instList[pc++], *this);                                                                       \
        goto label_Dispatch;                                                                                           \
    }

        OPCODE_ENUMS(HANDLE_OPCODE)
#undef HANDLE_OPCODE
#else

        while(pc < instList.size() && _stackTop != 0 && curStackTop == _stackTop) {
            const auto *instruction = instList[pc++];
            // fmt::println("{}\n", Op::Instruction::dump(*instruction, *this, true));
            Op::Instruction::execute(*instruction, *this);
        }
#endif
    }

    void VMState::reg(const Register &reg, const Value &value) const { _currentFrame->getReg(reg) = value; }

    Value VMState::reg(const Register reg) const { return _currentFrame->getReg(reg); }

    Value &VMState::regRef(const Register reg) const { return _currentFrame->getReg(reg); }

    Value VMState::getThis(const Atom atom) const {

        // current thisObj
        if(_currentFrame->thisObj) {
            if(auto *instanceObject = dynamic_cast<InstanceObject *>(_currentFrame->thisObj)) {
                if(instanceObject->hasProp(atom)) {
                    return instanceObject->getProp(atom);
                }
            }
        }

        // global
        // TODO: check is exist
        return global(atom);
    }

    Value VMState::getUpVal(const Atom atom) const {

        // current context
        if(_currentFrame->thisObj) {
            if(auto *instanceObject = dynamic_cast<InstanceObject *>(_currentFrame->thisObj)) {
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
            if(callFrame.thisObj) {
                if(auto *instanceObject = dynamic_cast<InstanceObject *>(_currentFrame->thisObj)) {
                    return instanceObject->getProp(atom);
                }
            }
        }

        // global
        // TODO: check is exist
        return global(atom);
    }
} // namespace Cial::Bytecode
