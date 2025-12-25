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

#include "ConstIndex.hpp"
#include "Instruction.hpp"

namespace Cial::Bytecode {
    class Chunk {
    public:
        ~Chunk() {
            for(const auto &inst : _instructions) {
                inst->~Instruction();
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

        explicit Chunk() = default;

        [[nodiscard]] auto &getInstVec() const noexcept { return _instructions; }

        void setRegisterCount(const std::uint32_t count) noexcept { _registerCount = count; }
        [[nodiscard]] std::uint32_t getRegCount() const noexcept { return _registerCount; }

        ConstIndex addConstant(const Value &value) {
            for(size_t i = 0; i < _constants.size(); ++i) {
                if(_constants[i] == value) {
                    return ConstIndex{ i };
                }
            }
            _constants.push_back(value);
            return ConstIndex{ _constants.size() - 1 };
        }

        [[nodiscard]] const Value &getConstant(const ConstIndex index) const {
            CLL_ASSERT(index.index() < _constants.size(), "constant index out of range");
            return _constants[index.index()];
        }

        Vec<Value> getConstants() noexcept { return _constants; }

        Chunk(const Chunk &) = delete;
        Chunk &operator=(const Chunk &) = delete;

    private:
        Vec<Op::Instruction *> _instructions{};
        std::uint32_t _registerCount{ 0 };
        Vec<Value> _constants{};
    };
} // namespace Cial::Bytecode
