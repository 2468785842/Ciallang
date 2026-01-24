//
// Created by LiDong on 2026/1/24.
//
#pragma once

#include "ControlFlowGraph.hpp"
#include "vm/Chunk.hpp"
#include "vm/Instruction.hpp"

namespace cial::Inter {

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
        virtual void optimize(Bytecode::Chunk &chunk, const Vec<BasicBlock> &blocks) = 0;

        virtual ~Optimizer() = default;

    protected:
        static void removeNopInst(Vec<Bytecode::Instruction> &instVec);

        bool usingRegister(Bytecode::Instruction &inst);
    };

    class OptimizerManager {
    public:
        explicit OptimizerManager(Bytecode::Chunk &chunk) : chunk(chunk) {}

        // 添加优化器
        void addOptimizer(std::unique_ptr<Optimizer> optimizer) { optimizers.push_back(std::move(optimizer)); }

        // 执行所有优化
        void applyOptimizations() const {
            ControlFlowGraph cfg;
            const Vec<BasicBlock> blocks = cfg.buildCFG(chunk);

            for(const auto &optimizer : optimizers) {
                optimizer->optimize(chunk, blocks);
            }
        }

    private:
        Bytecode::Chunk &chunk;
        Vec<std::unique_ptr<Optimizer>> optimizers;
    };

    class LoadSubOptimizer : public Optimizer {
    public:
        void optimize(Bytecode::Chunk &chunk, const Vec<BasicBlock> &blocks) override;
    };

} // namespace cial::Inter
