//
// Created by LiDong on 2026/1/24.
//
#pragma once

#include "Instruction.hpp"
#include "TacChunk.hpp"

namespace cial::inter {
    class BasicBlock {
    public:
        std::vector<i64> successors;
        TacChunk *parentChunk{};
        i64 startIdx{}; // 该基本块的开始指令索引
        i64 endIdx{}; // 该基本块的结束指令索引
        bool hasSideEffect{}; // 是否有副作用（如函数调用）

        explicit BasicBlock();
        explicit BasicBlock(TacChunk *chunk, const i64 start, const i64 end) :
            parentChunk(chunk), startIdx(start), endIdx(end) {}

        BasicBlock(BasicBlock &&rhs) noexcept = default;
        BasicBlock &operator=(BasicBlock &&rhs) noexcept = default;

        BasicBlock(const BasicBlock &rhs) noexcept = default;
        BasicBlock &operator=(const BasicBlock &rhs) noexcept = default;
    };


    class ControlFlowGraph {
    public:
        // 构建 CFG，返回基本块
        Vec<BasicBlock> &buildCFG(TacChunk &chunk);

        // 获取所有的基本块
        [[nodiscard]] const Vec<BasicBlock> &getBasicBlocks() const { return _basicBlocks; }

    private:
        Vec<BasicBlock> _basicBlocks; // 所有基本块
    };

    class CFGVisualizer {
    public:
        static void exportToDot(const std::string &filename, const Vec<BasicBlock> &blocks);

    private:
        static std::string escapeForDot(const std::string &s) {
            std::string result;
            result.reserve(s.size() + 16);
            for(const char c : s) {
                switch(c) {
                    case '"':
                        result += "\\\"";
                        break;
                    case '\\':
                        result += "\\\\";
                        break;
                    case '\n':
                        result += "\\l";
                        break;
                    case '\r':
                        break; // 忽略
                    default:
                        result += c;
                }
            }
            return result;
        }

        // 修改：findBlockByLabel 现在比较 start_idx（假设 Label.address() 返回索引）
        static i64 findBlockByLabel(const Vec<BasicBlock> &blocks, const Label &targetLabel) {
            for(i64 i = 0; i < static_cast<i64>(blocks.size()); ++i) {
                if(blocks[i].startIdx == targetLabel.address()) {
                    return i;
                }
            }
            return -1;
        }

        // 新增：根据指令计算边属性（颜色、label）
        static std::string getEdgeAttributes(size_t from, size_t to, const Vec<BasicBlock> &blocks,
                                             const Vec<Box<Instruction>> &instructions) {
            if(blocks[from].endIdx >= instructions.size())
                return "";

            const auto &lastInst = instructions[blocks[from].endIdx];
            if(!lastInst)
                return "";

            const auto op = lastInst->opcode();
            if(op == TacOpCode::Jmp) {
                return " [color=\"blue\", penwidth=2]";
            }
            if(op == TacOpCode::JmpE || op == TacOpCode::JmpNE) {
                const i64 target = findBlockByLabel(blocks, lastInst->ops()[0].value<Label>());
                if(target == static_cast<i64>(to)) {
                    return R"( [label="T", color="green"])";
                }
                return R"( [label="F", color="red"])";
            }
            // 默认 fall-through 或其他
            return " [color=\"black\"]";
        }
    };

} // namespace cial::inter