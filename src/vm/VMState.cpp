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

namespace Cial::Bytecode {

    void VMState::run() {
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
        const auto &instList = instructions();
        const Op::Instruction *instruction = instList[_currentFrame->pc];
        if(_currentFrame->pc >= instList.size() || _stackTop == 0) {
            freeCallFrame();
            return;
        }
        goto *labels[static_cast<size_t>(instruction->opcode)];
    }

#define HANDLE_OPCODE(OP)                                                                                              \
    label_##OP : {                                                                                                     \
        const Op::Instruction *instruction = instructions()[_currentFrame->pc];                                        \
        ++_currentFrame->pc;                                                                                           \
        Op::OP::execute(*instruction, *this);                                                                          \
        goto label_Dispatch;                                                                                           \
    }

        OPCODE_ENUMS(HANDLE_OPCODE)
#undef HANDLE_OPCODE
#else
        for(;;) {
            std::uint64_t &pc = _currentFrame->pc;
            const auto &instList = instructions();

            if(pc >= instList.size())
                break;

            if(_stackTop == 0)
                break;

            const auto &instruction = instList[pc];
            ++pc;
            // fmt::println("{}\n", Op::Instruction::dump(*instruction, *this, true));
            Op::Instruction::execute(*instruction, *this);
        }
        // freeCallFrame();
#endif
    }

    void VMState::reg(const Register &reg, const Value &value) const { _currentFrame->getReg(reg) = value; }

    Value VMState::reg(const Register reg) const { return _currentFrame->getReg(reg); }

    Value VMState::getUpVal(const Atom atom) const {

        // current context
        if(_currentFrame->context) {
            if(const auto *instanceObject = dynamic_cast<const InstanceObject *>(_currentFrame->context)) {
                return instanceObject->getField(atom);
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
            if(callFrame.context) {
                if(const auto *instanceObject = dynamic_cast<const InstanceObject *>(callFrame.context)) {
                    return instanceObject->getField(atom);
                }
            }
        }

        // global
        // TODO: check is exist
        return global(atom);
    }
} // namespace Cial::Bytecode
