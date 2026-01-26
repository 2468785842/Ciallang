//
// Created by LiDong on 2026/1/24.
//
#pragma once

#include "Instruction.hpp"

namespace cial::Inter {
    class BasicBlock {
    public:
        Bytecode::Chunk *parentChunk{};
        int start{}; // 该基本块的开始指令索引
        int end{}; // 该基本块的结束指令索引
        bool hasSideEffect{}; // 是否有副作用（如函数调用）

        explicit BasicBlock();
        explicit BasicBlock(Bytecode::Chunk *chunk, const int start, const int end) :
            parentChunk(chunk), start(start), end(end) {}

        BasicBlock(BasicBlock &&rhs) noexcept = default;
        BasicBlock &operator=(BasicBlock &&rhs) noexcept = default;

        BasicBlock(const BasicBlock &rhs) noexcept = default;
        BasicBlock &operator=(const BasicBlock &rhs) noexcept = default;
    };


    class ControlFlowGraph {
    public:
        // 构建 CFG，返回基本块
        Vec<BasicBlock> buildCFG(Bytecode::Chunk &chunk);

        // 获取所有的基本块
        const Vec<BasicBlock> &getBasicBlocks() const { return _basicBlocks; }

    private:
        Vec<BasicBlock> _basicBlocks; // 所有基本块
    };

} // namespace cial::Inter