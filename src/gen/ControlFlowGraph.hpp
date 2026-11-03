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

    // 自然循环：回边 t→h（h 支配 t）确定的单表头循环，body 含 header、升序排列。
    struct LoopInfo {
        size_t header{};
        Vec<size_t> body;
    };


    class ControlFlowGraph {
    public:
        // 构建 CFG，返回基本块
        Vec<BasicBlock> buildCFG(vm::Chunk &chunk);

        // 获取所有的基本块
        const Vec<BasicBlock> &getBasicBlocks() const { return _basicBlocks; }

        // 自然循环集合（buildCFG 时计算；不可达块不参与，同表头回边合并）
        const Vec<LoopInfo> &getLoops() const { return _loops; }

        // 支配集（buildCFG 时计算；dominators[b] 位集，b ∈ dom[b]；不可达块仅含自身）
        const Vec<Vec<bool>> &getDominators() const { return _dominators; }

        // 后支配集（buildCFG 时计算；反向可达性，出口 = Ret/Throw 块；不可达块
        // 视作自出口：pdom 仅含自身，供反编译归约保守回退）
        const Vec<Vec<bool>> &getPostDominators() const { return _postDominators; }

        // DOT 格式控制流图：节点=基本块（TAC 区间+指令文本），边按终结指令标注
        // jmp/cond/fall/exc，回边标红，循环头高亮。指令文本取自 chunk.dumpInstructions()。
        [[nodiscard]] std::string dumpDot(vm::Chunk &chunk) const;

    private:
        void detectLoops();
        void computePostDominators(const Vec<Vec<size_t>> &predecessors, const Vec<bool> &reachable);

        Vec<BasicBlock> _basicBlocks; // 所有基本块
        Vec<LoopInfo> _loops; // 自然循环（按 header 升序）
        Vec<Vec<bool>> _dominators;
        Vec<Vec<bool>> _postDominators;
    };

} // namespace cial::inter
