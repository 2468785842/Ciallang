/*
 * Copyright (c) 2024/6/13 下午8:15
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

#include "Constant.hpp"

#include "gen/Instruction.hpp"

namespace cial::vm {
    class VMState;

    class Chunk : public MarkSweepHeader {
    public:
        struct ThrowHandler {
            u64 tryStart;
            u64 tryEnd;
            u16 exValueReg;
        };

        struct LocalVariable {
            Atom identifier = ATOM_INVALID;
            u16 reg{};
            // when (var.startPC <= inst.pc) you can use
            u64 startPC{}; // Effective start PC
            // when (var.endPC > inst.pc) you can't use
            u64 endPC{}; // Invalid PC (scope ended)
        };
        explicit Chunk() = default;

        explicit Chunk(Vec<LocalVariable> localVars, Vec<Constant> constants, Vec<ThrowHandler> throwHandlers,
                       Vec<u8> code, const u32 regCount) :
            _localVars(std::move(localVars)), _constants(std::move(constants)),
            _throwHandlers(std::move(throwHandlers)), _code(std::move(code)), _regCount(regCount) {}

        Chunk(const Chunk &) = delete;
        Chunk &operator=(const Chunk &) = delete;

        Chunk(Chunk &&chunk) = default;
        Chunk &operator=(Chunk &&chunk) = default;

        void addThrowHandler(const ThrowHandler &tHandler) { _throwHandlers.push_back(tHandler); }

        [[nodiscard]] Opt<ThrowHandler> findThrowHandler(const u64 pc) const {
            for(const auto &h : _throwHandlers) {
                if(h.tryStart <= pc && pc < h.tryEnd) {
                    return h;
                }
            }
            return {};
        }

        void setRegCount(const u32 count) noexcept { _regCount = count; }
        [[nodiscard]] u32 getRegCount() const noexcept { return _regCount; }

        template <typename... Args>
        ConstIdx addConstant(Args &&...args) {
            const Constant value{ std::forward<Args>(args)... };
            return addConstant(value);
        }

        ConstIdx addConstant(Constant value) {
            if(_constants.size() >= std::numeric_limits<u16>::max()) {
                throw std::runtime_error("too many constants");
            }
            const u16 size = static_cast<u16>(_constants.size());
            for(u16 i = 1; i < size; ++i) {
                if(_constants[i] == value) {
                    return ConstIdx{ i };
                }
            }
            _constants.emplace_back(value);
            return ConstIdx{ size };
        }

        [[nodiscard]] const Constant &getConstant(const ConstIdx index) const { return _constants[index.index()]; }

        // [[nodiscard]] String dumpInstructions() const;

        // [[nodiscard]] String dumpInstructions(const VMState *vmState) const;

        void marked() noexcept override {
            MarkSweepHeader::marked();
            for(auto &inst : _constants) {
                if(inst.type() == ConstantType::FuncMeta) {
                    inst.value<FuncMeta *>()->marked();
                } else if(inst.type() == ConstantType::ClassMeta) {
                    inst.value<ClassMeta *>()->marked();
                }
            }
        }

        [[nodiscard]] const Vec<u8> &code() const { return _code; }

        [[nodiscard]] const Vec<LocalVariable> &getLocalVars() const { return _localVars; }

        [[nodiscard]] const Vec<Constant> &getConstants() noexcept { return _constants; }

    private:
        Vec<LocalVariable> _localVars;
        Vec<Constant> _constants{};
        Vec<ThrowHandler> _throwHandlers{};
        Vec<u8> _code{};
        u32 _regCount{};
    };
} // namespace cial::vm
