//
// Created by LiDong on 2026/1/24.
//
#pragma once

#include <algorithm>
#include <map>
#include <set>
#include <vector>

#include "ControlFlowGraph.hpp"
#include "Instruction.hpp"
#include "TacChunk.hpp"

namespace cial::inter {

    // int main() {
    //     Vec<Instruction> code = {
    //         Instruction(Instruction::Op::LoadConst, 1, -1, -1, 1),  // load const 1
    //         Instruction(Instruction::Op::Sub, 2, 1, 1),              // sub r2, r1
    //         Instruction(Instruction::Op::Ret, 2)                      // ret r2
    //     };
    //
    //     OptimizerManager optimizerManager(code);
    //     optimizerManager.addOptimizer(std::make_unique<LoadSubOptimizer>());
    //     optimizerManager.applyOptimizations();
    //
    //     for (const auto& instr : code) {
    //         std::cout << static_cast<int>(instr.op) << " ";
    //     }
    //
    //     return 0;
    // }

    class Optimizer {
    public:
        virtual void optimize(TacChunk &chunk, const Vec<BasicBlock> &blocks) = 0;

        virtual ~Optimizer() = default;

    protected:
        static void removeNopInst(Vec<Box<Instruction>> &instVec);
    };

    class OptimizerManager {
    public:
        explicit OptimizerManager(TacChunk &chunk) : chunk(chunk) {}

        // 添加优化器
        void addOptimizer(Box<Optimizer> optimizer) { optimizers.push_back(std::move(optimizer)); }

        // 执行所有优化
        void applyOptimizations() const {
            ControlFlowGraph cfg;
            const Vec<BasicBlock> blocks = cfg.buildCFG(chunk);

            for(const auto &optimizer : optimizers) {
                optimizer->optimize(chunk, blocks);
            }
        }

    private:
        TacChunk &chunk;
        Vec<Box<Optimizer>> optimizers;
    };

    struct Interval {
        Register vReg; // 虚拟寄存器 ID
        int start; // 起始指令索引
        int end; // 结束指令索引
        Register pReg{ -1 }; // 分配后的物理寄存器 ID

        bool operator<(const Interval &other) const { return start < other.start; }
    };

    class RegisterAllocator : public Optimizer {
    public:
        void optimize(TacChunk &chunk, const Vec<BasicBlock> &blocks) override {
            // 1. 计算活跃区间
            auto intervals = computeIntervals(chunk);

            // 2. 线性扫描分配
            allocate(intervals);

            // 3. 重写指令中的寄存器 ID
            rewriteInstructions(chunk, intervals);

            removeNopInst(chunk.getInstVec());
        }

    private:
        // 计算每个寄存器的生存周期 [first_seen, last_seen]
        Vec<Interval> computeIntervals(TacChunk &chunk);

        void allocate(Vec<Interval> &intervals);

        void rewriteInstructions(TacChunk &chunk, const Vec<Interval> &intervals);
    };
} // namespace cial::inter
