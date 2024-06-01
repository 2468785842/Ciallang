/*
 * Copyright (c) 2024/5/30 上午8:07
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

#include <sstream>

#include <frozen/unordered_map.h>
#include <fmt/format.h>

#include "Interpreter.hpp"
#include "Rlc.hpp"
#include "VMChunk.hpp"

namespace Ciallang::VM {
    enum class Opcodes : uint8_t {
        Const = 1,
        Ret
    };

    struct Instruction {
        static const Instruction* instance(Opcodes opcode);

        const char* name{ nullptr };
        const size_t offset;

        constexpr Instruction(const char* name, const size_t offset)
            : name(name), offset(offset) {
        }

        virtual void execute(Interpreter* interpreter) const {
        }

        std::string disassemble(const Interpreter* interpreter, const Opcodes offset) const {
            return disassemble(
                interpreter->_vm.chunk->bytecodes(),
                interpreter->_vm.chunk->constants(),
                interpreter->_vm.chunk->rlc(),
                static_cast<uint8_t>(offset)
            );
        }

        virtual std::string disassemble(const uint8_t* bytecodes,
                                        const TjsValue* constants,
                                        const Rlc* rlc, size_t offset) const = 0;

        virtual ~Instruction() = default;
    };

    //////////////////////////////////////////////////////////////////////
#define START (std::ostringstream{}
#define PRINT_NAME fmt::format("{:04d}\t{:>8}", offset, S_InstName)
#define PRINT_LINE fmt::format("{:>5}", rlc->firstAppear(offset) ? std::to_string(rlc->find(offset) + 1) : "~")
#define PRINT_CONSTANT fmt::format(" // {};", constants[bytecodes[offset + 1]])
#define END '').str()

    struct RetInstruction final : Instruction {
        constexpr RetInstruction():
            Instruction(S_InstName, 1) {
        }

        void execute(Interpreter* interpreter) const override {
        }

        [[nodiscard]] std::string disassemble(const uint8_t* bytecodes,
                                              const TjsValue* constants,
                                              const Rlc* rlc,
                                              size_t offset) const override {
            return START << PRINT_NAME << PRINT_LINE << END;
        }

    private:
        static constexpr char S_InstName[] = "ret";
    };

    //////////////////////////////////////////////////////////////////////
    struct ConstInstruction final : Instruction {
        constexpr ConstInstruction():
            Instruction(S_InstName, 2) {
        }

        [[nodiscard]] std::string disassemble(const uint8_t* bytecodes,
                                              const TjsValue* constants,
                                              const Rlc* rlc, size_t offset) const override {
            return START << PRINT_NAME << PRINT_LINE << PRINT_CONSTANT << END;
        }

    private:
        static constexpr char S_InstName[] = "const";
    };

#undef START
#undef PRINT_NAME
#undef PRINT_LINE
#undef PRINT_CONSTANT
#undef END

    static constexpr RetInstruction S_RetInst{};
    static constexpr ConstInstruction S_ConstInst{};

    static constinit auto instructions =
            frozen::make_unordered_map<Opcodes, const Instruction*>({
                    { Opcodes::Const, &S_ConstInst },
                    { Opcodes::Ret, &S_RetInst },
            });

    inline const Instruction* Instruction::instance(const Opcodes opcode) {
        return instructions.at(opcode);
    }
}
