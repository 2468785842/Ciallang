//
// Created by LiDong on 2026/1/24.
//
#pragma once

#include <chrono>
#include <string_view>

#include "ControlFlowGraph.hpp"
#include "Instruction.hpp"
#include "vm/Chunk.hpp"

namespace cial::inter {

    struct OptimizerPassStats {
        std::string_view name;
        u64 nanoseconds{};
        size_t instructionsBefore{};
        size_t instructionsAfter{};
        size_t transformations{};
    };

    struct OptimizerStats {
        size_t instructionsBefore{};
        size_t instructionsAfter{};
        size_t instructionsRemovedByCompaction{};
        u64 cfgNanoseconds{};
        u64 compactionNanoseconds{};
        Vec<OptimizerPassStats> passes;
    };

    class Optimizer {
    public:
        virtual size_t optimize(vm::Chunk &chunk, const Vec<BasicBlock> &blocks) = 0;
        [[nodiscard]] virtual std::string_view name() const noexcept = 0;

        virtual ~Optimizer() = default;

    protected:
        static size_t removeNopInst(vm::Chunk &chunk);
        friend class OptimizerManager;
    };

    // 拷贝传播：GLocal/DLocal 是纯寄存器拷贝（VM_CP 语义，不触发 getter/setter），
    // 记录 dst:=src 可用拷贝并在白名单指令（算术/比较/CallV 实参等）的使用点
    // 改写为源寄存器——变量读的家园寄存器拷贝随之变成死代码，由
    // DeadLoadElimination 一并清除。def 精确集合只覆盖拷贝指令自身，
    // 其余指令按"提及即重定义"保守失效；跨块仅沿唯一前驱继承（同整数常量传播）。
    class CopyPropagationOptimizer final : public Optimizer {
    public:
        [[nodiscard]] std::string_view name() const noexcept override { return "copy-propagation"; }
        size_t optimize(vm::Chunk &chunk, const Vec<BasicBlock> &blocks) override;
    };

    // 有符号整数常量传播：块内折叠溢出安全的 Add/Sub/Mul；
    // 块间状态分析——唯一前驱的块继承前驱出口常量状态（回边与多前驱保守放弃）。
    // 不改变可能抛错的动态运算。
    class IntegerConstantPropagationOptimizer final : public Optimizer {
    public:
        [[nodiscard]] std::string_view name() const noexcept override { return "integer-constant-propagation"; }
        size_t optimize(vm::Chunk &chunk, const Vec<BasicBlock> &blocks) override;
    };

    // 仅移除结果未被读取的立即数加载；Load 可能实例化运行时对象，保守保留。
    class DeadLoadEliminationOptimizer final : public Optimizer {
    public:
        explicit DeadLoadEliminationOptimizer(OptReg externallyObserved = {}) :
            externallyObserved(externallyObserved) {}
        [[nodiscard]] std::string_view name() const noexcept override { return "dead-load-elimination"; }
        size_t optimize(vm::Chunk &chunk, const Vec<BasicBlock> &blocks) override;

    private:
        OptReg externallyObserved;
    };

    class CompareTestFusionOptimizer final : public Optimizer {
    public:
        std::string_view name() const noexcept override { return "compare-test-fusion"; }
        size_t optimize(vm::Chunk &chunk, const Vec<BasicBlock> &blocks) override;
    };

    class OptimizerManager {
    public:
        explicit OptimizerManager(vm::Chunk &chunk) : chunk(chunk) {}

        // 添加优化器
        void addOptimizer(Box<Optimizer> optimizer) { optimizers.push_back(std::move(optimizer)); }

        // 执行所有优化
        [[nodiscard]] OptimizerStats applyOptimizations() const {
            using Clock = std::chrono::steady_clock;
            const bool profile = profilingEnabled();
            OptimizerStats stats;
            stats.instructionsBefore = chunk.getInstVec().size();
            stats.instructionsAfter = stats.instructionsBefore;
            if(stats.instructionsBefore == 0 || optimizers.empty())
                return stats;

            auto phaseStart = profile ? Clock::now() : Clock::time_point{};
            ControlFlowGraph cfg;
            const Vec<BasicBlock> blocks = cfg.buildCFG(chunk);
            if(profile)
                stats.cfgNanoseconds = static_cast<u64>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - phaseStart).count());

            for(const auto &optimizer : optimizers) {
                const size_t instructionsBefore = profile ? chunk.getInstVec().size() : 0;
                if(profile)
                    phaseStart = Clock::now();
                const size_t transformations = optimizer->optimize(chunk, blocks);
                if(profile) {
                    const auto duration =
                        std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - phaseStart).count();
                    stats.passes.push_back({ optimizer->name(), static_cast<u64>(duration), instructionsBefore,
                                             chunk.getInstVec().size(), transformations });
                }
            }

            if(profile)
                phaseStart = Clock::now();
            stats.instructionsRemovedByCompaction = Optimizer::removeNopInst(chunk);
            if(profile)
                stats.compactionNanoseconds = static_cast<u64>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - phaseStart).count());
            stats.instructionsAfter = chunk.getInstVec().size();
            return stats;
        }

        [[nodiscard]] static bool profilingEnabled() noexcept;

    private:
        vm::Chunk &chunk;
        Vec<Box<Optimizer>> optimizers;
    };

} // namespace cial::inter
