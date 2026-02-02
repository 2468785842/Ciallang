//
// Created by LiDong on 2026/1/29.
//
#pragma once

#include <ranges>

#include "Instruction.hpp"
#include "logging/Logger.hpp"

namespace cial::inter {

    class TacChunk {
    public:
        struct ThrowHandler {
            Label tryStart;
            Label tryEnd;
            Register exValueReg;
        };

        struct LocalVariable {
            Atom identifier = ATOM_INVALID;
            Register reg{};
            // when (var.startPC <= inst.pc) you can use
            Label startPC{}; // Effective start PC
            // when (var.endPC > inst.pc) you can't use
            Label endPC{}; // Invalid PC (scope ended)
        };

        explicit TacChunk() = default;

        /**
         * @tparam Args 指令的值类型
         * @param args 值数组
         * @return 指令在内存的索引
         */
        template <typename T, typename... Args>
            requires std::is_base_of_v<Instruction, T>
        size_t emit(Args &&...args) {
            const size_t index = _instructions.size();
            _instructions.push_back(std::make_unique<T>(Operand(std::forward<Args>(args))...));
            return index;
        }

        template <typename T, typename... Args>
            requires std::is_base_of_v<Instruction, T>
        void repl(const size_t index, Args &&...args) {
            if(index > _instructions.size()) {
                throw std::runtime_error("instruction index out of range");
            }
            _instructions[index] = std::make_unique<T>(Operand(std::forward<Args>(args))...);
        }

        [[nodiscard]] Instruction *inst(const size_t index) const { return _instructions[index].get(); }

        TacChunk(const TacChunk &) = delete;
        TacChunk &operator=(const TacChunk &) = delete;

        TacChunk(TacChunk &&chunk) = default;
        TacChunk &operator=(TacChunk &&chunk) = default;

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

        template <typename... Args>
        void addLocalVar(Args &&...args) {
            _localVars.emplace_back(std::forward<Args>(args)...);
        }

        LocalVariable *resolveLocalVariable(const Atom identifier) {
            for(auto &var : std::ranges::reverse_view(_localVars)) {
                if(var.endPC.address() == 0 && var.identifier.v == identifier.v) {
                    return &var;
                }
            }
            return nullptr;
        }

        template <typename... Args>
        void addThrowHandler(Args &&...args) {
            _throwHandlers.emplace_back(std::forward<Args>(args)...);
        }

        Vec<Box<Instruction>> &getInstVec() noexcept { return _instructions; }
        Vec<LocalVariable> &getLocalVars() noexcept { return _localVars; }
        Vec<Constant> &getConstants() noexcept { return _constants; }
        Vec<ThrowHandler> &getThrowHandlers() noexcept { return _throwHandlers; }

        [[nodiscard]] const Vec<Box<Instruction>> &getInstVec() const noexcept { return _instructions; }
        [[nodiscard]] const Vec<LocalVariable> &getLocalVars() const noexcept { return _localVars; }
        [[nodiscard]] const Vec<Constant> &getConstants() const noexcept { return _constants; }
        [[nodiscard]] const Vec<ThrowHandler> &getThrowHandlers() const noexcept { return _throwHandlers; }

        [[nodiscard]] String dumpInstructions() const {
            std::stringstream ss{ "" };
            i64 pc{};
            while(pc < _instructions.size()) {
                const auto &instruction = _instructions[pc];
                ss << fmt::format("{: <6}: {}", Label{ pc }, instruction->dump(nullptr));
                if(pc != _instructions.size() - 1) {
                    ss << '\n';
                }
                ++pc;
            }
            return String{ ss.str() };
        }

    private:
        Vec<LocalVariable> _localVars{};
        Vec<Box<Instruction>> _instructions{};
        Vec<ThrowHandler> _throwHandlers{};
        Vec<Constant> _constants{ Constant{} }; // Void
        u32 _regCount{};
    };
} // namespace cial::inter