//
// Created by LiDong on 2026/1/24.
//
#pragma once

#include <cstddef>

#include "Instruction.hpp"

namespace cial::inter {
    class BasicBlock {
    public:
        vm::Chunk *parentChunk{};
        size_t start{}; // 包含的起始 TAC 下标
        size_t end{}; // 不包含的结束 TAC 下标
        bool hasSideEffect{}; // 是否有副作用（如函数调用）
        Vec<size_t> successors; // 后继基本块索引

        explicit BasicBlock();
        explicit BasicBlock(vm::Chunk *chunk, const size_t start, const size_t end) :
            parentChunk(chunk), start(start), end(end) {}

        BasicBlock(BasicBlock &&rhs) noexcept = default;
        BasicBlock &operator=(BasicBlock &&rhs) noexcept = default;

        BasicBlock(const BasicBlock &rhs) noexcept = default;
        BasicBlock &operator=(const BasicBlock &rhs) noexcept = default;
    };


    class ControlFlowGraph {
    public:
        // 构建 CFG，返回基本块
        Vec<BasicBlock> buildCFG(vm::Chunk &chunk);

        // 获取所有的基本块
        const Vec<BasicBlock> &getBasicBlocks() const { return _basicBlocks; }

    private:
        Vec<BasicBlock> _basicBlocks; // 所有基本块
    };

} // namespace cial::inter
