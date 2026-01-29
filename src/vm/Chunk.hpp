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

#include "Bytecode.hpp"
#include "Constant.hpp"

#include "gen/Instruction.hpp"
#include "logging/Logger.hpp"

namespace cial::vm {
    class VMState;

    struct ThrowHandler {
        u64 tryStart;
        u64 tryEnd;
        u16 exValueReg;
    };

    class Chunk : public MarkSweepHeader {
    public:
        explicit Chunk() {
            _constants.emplace_back(); // Void
        }

        void addThrowHandler(const ThrowHandler &tHandler) { _throwHandlers.push_back(tHandler); }

        [[nodiscard]] Opt<ThrowHandler> findThrowHandler(const u64 pc) const {
            for(const auto &h : _throwHandlers) {
                if(h.tryStart <= pc && pc < h.tryEnd) {
                    return h;
                }
            }
            return {};
        }

        /**
         * @tparam Args 指令的值类型
         * @param args 值数组
         * @return 指令在内存的索引
         */
        template <typename T, typename... Args>
            requires std::is_base_of_v<inter::Instruction, T>
        size_t emit(Args &&...args) {
            const size_t index = _instructions.size();
            _instructions.push_back(std::make_unique<T>(inter::Operand(std::forward<Args>(args))...));
            return index;
        }

        template <typename T, typename... Args>
            requires std::is_base_of_v<inter::Instruction, T>
        void repl(const size_t index, Args &&...args) {
            if(index > _instructions.size()) {
                throw std::runtime_error("instruction index out of range");
            }
            _instructions[index] = std::make_unique<T>(inter::Operand(std::forward<Args>(args))...);
        }

        [[nodiscard]] inter::Instruction *inst(const size_t index) const { return _instructions[index].get(); }

        Chunk(const Chunk &) = delete;
        Chunk &operator=(const Chunk &) = delete;

        Chunk(Chunk &&chunk) = default;
        Chunk &operator=(Chunk &&chunk) = default;

        [[nodiscard]] auto &getInstVec() noexcept { return _instructions; }

        void setRegCount(const u32 count) noexcept { _registerCount = count; }
        [[nodiscard]] u32 getRegCount() const noexcept { return _registerCount; }

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

        [[nodiscard]] const Constant &getConstant(const ConstIdx index) const {
            CLL_ASSERT(index.index() < _constants.size() && "constant index out of range",
                       this->dumpInstructions().getData());
            return _constants[index.index()];
        }

        Vec<Constant> &getConstants() noexcept { return _constants; }

        [[nodiscard]] String dumpConstant(const Runtime *rt, ConstIdx idx) const;

        [[nodiscard]] String dumpConstants(const Runtime *rt) const;

        [[nodiscard]] String dumpInstructions() const;

        [[nodiscard]] String dumpInstructions(const VMState *vmState) const;

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

        void toBytecode() {
            if(_isBytecode)
                return;

            _code = Bytecode::compile(_instructions);
            _isBytecode = true;
        }

        [[nodiscard]] const Vec<u8> &code() const {
            if(!_isBytecode) {
                throw std::runtime_error("chunk is not compiled to bytecode");
            }
            return _code;
        }

    private:
        Vec<Box<inter::Instruction>> _instructions{};
        Vec<Constant> _constants{};
        Vec<ThrowHandler> _throwHandlers{};
        Vec<u8> _code{};
        u32 _registerCount{};
        bool _isBytecode{ false };
    };
} // namespace cial::vm
