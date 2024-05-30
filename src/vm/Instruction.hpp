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

#define PRINT_LINE(fmtMsg) (fmtMsg << fmt::format("{:>5}", rlc->firstAppear(offset) ? std::to_string(rlc->find(offset) + 1) : "~"))

#include <sstream>

#include <frozen/unordered_map.h>
#include <fmt/format.h>

#include "Rlc.hpp"

namespace Ciallang::VM {
    enum class Opcodes : uint8_t {
        Const = 1,
        Ret
    };

    struct Instruction {
        static const Instruction* instance(uint8_t bytecodes);

        const char* name{ nullptr };
        const size_t offset;

        constexpr Instruction(const char* name, const size_t offset)
            : name(name), offset(offset) {
        }

        virtual std::string disassemble(const uint8_t* bytecodes,
                                        const TjsValue* constants,
                                        const Rlc* rlc,
                                        size_t& offset) const = 0;

        virtual ~Instruction() = default;
    };

    //////////////////////////////////////////////////////////////////////
    struct RetInstruction final : Instruction {
        constexpr RetInstruction():
            Instruction(S_InstName, 1) {
        }

        [[nodiscard]] std::string disassemble(const uint8_t* bytecodes,
                                              const TjsValue* constants,
                                              const Rlc* rlc,
                                              size_t& offset) const override {
            std::stringstream fmtMsg;

            fmtMsg << fmt::format("{:04d}\t{:>8}", offset, S_InstName);
            PRINT_LINE(fmtMsg);
            offset += this->offset;
            return fmtMsg.str();
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
                                              const Rlc* rlc, size_t& offset) const override {
            std::stringstream fmtMsg;

            fmtMsg << fmt::format("{:04d}\t{:>8}", offset, S_InstName);
            PRINT_LINE(fmtMsg);
            fmtMsg << " // " << constants[bytecodes[offset + 1]].asReal() << " ;";

            offset += this->offset;
            return fmtMsg.str();
        }

    private:
        static constexpr char S_InstName[] = "const";
    };

    static constexpr RetInstruction S_RetInst{};
    static constexpr ConstInstruction S_ConstInst{};

    static constinit frozen::unordered_map<Opcodes, const Instruction*, static_cast<size_t>(Opcodes::Ret)> instructions{
            { Opcodes::Const, &S_ConstInst },
            { Opcodes::Ret, &S_RetInst },
    };

    inline const Instruction* Instruction::instance(uint8_t bytecodes) {
        return instructions.at(static_cast<Opcodes>(bytecodes));
    }
}

#undef PRINT_LINE
