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

#include <atomic>
#include <limits>
#include <ranges>

#include <fmt/format.h>
#include "Bytecode.hpp"
#include "Rlc.hpp"
#include "Constant.hpp"

#include "gen/Instruction.hpp"
#include "logging/Logger.hpp"

namespace cial::vm {
    class VMState;

    class Chunk : public MarkSweepHeader {
    public:
        explicit Chunk() {
            _constants.emplace_back(); // Void
        }

        void addThrowHandler(const ThrowHandler &tHandler) { _throwHandlers.push_back(tHandler); }
        [[nodiscard]] Vec<ThrowHandler> &getThrowHandlers() noexcept { return _throwHandlers; }
        [[nodiscard]] const Vec<ThrowHandler> &getThrowHandlers() const noexcept { return _throwHandlers; }

        // TAC pc 空间的 try 区间表（供指令 dump 等使用）
        [[nodiscard]] Opt<ThrowHandler> findThrowHandler(const u64 pc) const {
            for(const auto &h : _throwHandlers) {
                if(h.tryStart <= pc && pc < h.tryEnd) {
                    return h;
                }
            }
            return {};
        }

        // 扁平字节码 pc 空间的 try 区间表（run0 使用）
        [[nodiscard]] Opt<ThrowHandler> findBcThrowHandler(const u64 pc) const {
            for(const auto &h : _bcThrowHandlers) {
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

        // 非 Operand 操作数指令（如 CallV 的寄存器列表）的原生发射
        template <typename T, typename... Args>
            requires std::is_base_of_v<inter::Instruction, T>
        size_t emitRaw(Args &&...args) {
            const size_t index = _instructions.size();
            _instructions.push_back(std::make_unique<T>(std::forward<Args>(args)...));
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
            if(_marked)
                return;
            MarkSweepHeader::marked();
            for(auto &inst : _constants) {
                if(inst.type() == ConstantType::FuncMeta) {
                    inst.value<FuncMeta *>()->marked();
                } else if(inst.type() == ConstantType::ClassMeta) {
                    inst.value<ClassMeta *>()->marked();
                } else if(inst.type() == ConstantType::PropMeta) {
                    inst.value<PropMeta *>()->marked();
                }
            }
        }

        void toBytecode() {
            if(_isBytecode)
                return;

            auto result = Bytecode::compile(_instructions, _throwHandlers);
            _code = std::move(result.code);
            _bcThrowHandlers = std::move(result.handlers);
            // 行号表换算：IRGen 记录的是 TAC 下标，run0 的 pc 是 bc 偏移
            for(auto &[idx, loc] : _rlc.rawEntries())
                idx = result.tacStartPos[idx];
            _isBytecode = true;
        }

        // 语句级行号记录（IRGen 各语句 generate 入口调用；Rlc 内部合并连续同位置）
        void addLine(const Common::SourceLocation &loc) {
            _rlc.addBytecodeLine(_instructions.size(), loc);
        }

        // bc 偏移 -> 源码位置（toBytecode 后有效；无记录返回空 Opt）
        [[nodiscard]] Opt<Common::SourceLocation> lineAtBc(const u64 bcPc) const {
            if(_rlc.empty() || bcPc < _rlc.rawEntries().front().first)
                return {};
            return _rlc.find(static_cast<size_t>(bcPc));
        }

        [[nodiscard]] Rlc &rlc() noexcept { return _rlc; }

        // 仅在启用执行记录时分配身份；记录器保存值快照，不持有 GC 对象指针。
        [[nodiscard]] u64 executionTraceId() {
            static std::atomic<u64> next{ 1 };
            if(!_executionTraceId)
                _executionTraceId = next.fetch_add(1, std::memory_order_relaxed);
            return _executionTraceId;
        }

        [[nodiscard]] const Vec<u8> &code() const {
            if(!_isBytecode) {
                throw std::runtime_error("chunk is not compiled to bytecode");
            }
            return _code;
        }

    private:
        Vec<Box<inter::Instruction>> _instructions{};
        Rlc _rlc{};
        Vec<Constant> _constants{};
        Vec<ThrowHandler> _throwHandlers{};
        Vec<ThrowHandler> _bcThrowHandlers{}; // try 区间已换算到扁平字节码 pc 空间
        Vec<u8> _code{};
        u32 _registerCount{};
        bool _isBytecode{ false };
        u64 _executionTraceId{};
    };
} // namespace cial::vm
