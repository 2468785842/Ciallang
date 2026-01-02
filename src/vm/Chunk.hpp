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

#include "Constant.hpp"

#include "Instruction.hpp"

namespace Cial::Bytecode {

    class Chunk : public MarkSweepHeader {
    public:
        ~Chunk() override {
            for(const auto &inst : _instructions) {
                delete inst;
            }
            _instructions.clear();
        }
        /**
         * @tparam OP 操作码
         * @tparam Args 指令的值类型
         * @param args 值数组
         * @return 指令在内存的索引
         */
        template <Op::OpCode OP, typename... Args>
        Op::Instruction *emit(Args &&...args) {
            auto *inst = new Op::Instruction(OP, Op::Operand(std::forward<Args>(args))...);
            _instructions.push_back(inst);
            return inst;
        }

        explicit Chunk() {
            _constants.emplace_back(); // Void
        }

        Chunk(const Chunk &) = delete;
        Chunk &operator=(const Chunk &) = delete;

        Chunk(Chunk &&chunk) = default;
        Chunk &operator=(Chunk &&chunk) = default;

        [[nodiscard]] auto &getInstVec() const noexcept { return _instructions; }

        void setRegCount(const std::uint32_t count) noexcept { _registerCount = count; }
        [[nodiscard]] std::uint32_t getRegCount() const noexcept { return _registerCount; }

        ConstIdx addConstant(Constant &&value) {
            for(size_t i = 1; i < _constants.size(); ++i) {
                if(_constants[i] == value) {
                    return ConstIdx{ i };
                }
            }
            _constants.push_back(std::move(value));
            return ConstIdx{ _constants.size() - 1 };
        }

        [[nodiscard]] const Constant &getConstant(const ConstIdx index) const {
            CLL_ASSERT(index.index() < _constants.size(), "constant index out of range");
            return _constants[index.index()];
        }

        Vec<Constant> &getConstants() noexcept { return _constants; }

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

        [[nodiscard]] String dumpInstruction() const {
            std::stringstream ss{};
            size_t pc{};
            while(pc < _instructions.size()) {
                const auto &instruction = _instructions[pc];
                ss << fmt::format("{: <6}: {}", Label{ pc }, Op::Instruction::dump(*instruction, nullptr));
                if(pc != _instructions.size() - 1) {
                    ss << '\n';
                }
                ++pc;
            }
            const std::string_view &sv = ss.view();
            return String{ sv.data(), static_cast<std::uint32_t>(sv.length()) };
        }

    private:
        Vec<Op::Instruction *> _instructions{};
        Vec<Constant> _constants{};
        std::uint32_t _registerCount{ 0 };
    };
} // namespace Cial::Bytecode
