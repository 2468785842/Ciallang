//
// Created by LiDong on 2026/1/24.
//

#include "ControlFlowGraph.hpp"

#include <unordered_set>

#include "vm/Chunk.hpp"

namespace cial::inter {
    static i64 findBlockIndexByStart(const Vec<BasicBlock> &blocks, const i64 pc) {
        const auto it = std::lower_bound(blocks.begin(), blocks.end(), pc,
                                         [](const BasicBlock &b, const i64 addr) { return b.startIdx < addr; });

        if(it != blocks.end() && it->startIdx == pc) {
            return std::distance(blocks.begin(), it);
        }
        return -1;
    }

    Vec<BasicBlock> &ControlFlowGraph::buildCFG(TacChunk &chunk) {
        const auto &code = chunk.getInstVec();
        if(code.empty())
            return _basicBlocks;

        std::unordered_set<i64> leaders;
        leaders.emplace(0);

        // 第一遍：收集所有 leader（基本块起始位置）
        for(size_t i = 0; i < code.size(); ++i) {
            const auto &ins = code[i];

            // 任何可能导致控制流转移的指令
            if(ins->opcode() == TacOpCode::Jmp || ins->opcode() == TacOpCode::JmpE ||
               ins->opcode() == TacOpCode::JmpNE) { // 建议在 Instruction 里加个虚函数 isBranch()
                leaders.emplace(ins->ops()[0].value<Label>().address());
                leaders.emplace(i + 1); // 可能的 fall-through
            }

            // 返回指令之后也可能是新块起点（虽然很多时候没用，但防御性好）
            if(ins->opcode() == TacOpCode::Ret) {
                leaders.emplace(i + 1);
            }
        }

        // 排序，得到有序的 leader 列表
        std::vector<i64> leaderList{ leaders.begin(), leaders.end() };
        std::ranges::sort(leaderList);

        // 第二遍：创建所有基本块
        _basicBlocks.clear();

        for(size_t i = 0; i < leaderList.size(); ++i) {
            const i64 startIdx = leaderList[i];
            const i64 endIdx = (i + 1 < leaderList.size()) ? leaderList[i + 1] - 1 : static_cast<i64>(code.size()) - 1;

            // 边界保护
            if(startIdx > endIdx || startIdx >= code.size())
                continue;

            BasicBlock bb{ &chunk, startIdx, endIdx };

            // 扫描副作用（可以保留）
            for(u64 pc = startIdx; pc <= endIdx; ++pc) {
                if(code[pc]->opcode() == TacOpCode::Call) {
                    bb.hasSideEffect = true;
                    break;
                }
            }

            _basicBlocks.push_back(std::move(bb));
        }

        // ────────────────────────────────────────────────
        // 第三遍：关键！计算每个基本块的 successors
        // ────────────────────────────────────────────────
        for(i64 bbIdx = 0; bbIdx < _basicBlocks.size(); ++bbIdx) {
            auto &block = _basicBlocks[bbIdx];

            if(block.endIdx >= code.size())
                continue;

            const auto &lastInst = code[block.endIdx];

            // 情况 1：返回、退出、不可达等 → 无后继
            if(lastInst->opcode() == TacOpCode::Ret /* || 其他终止指令 */) {
                continue;
            }

            // 情况 2：无条件跳转（只有一条后继）
            if(const auto *jmp = dynamic_cast<Jmp *>(lastInst.get())) {
                const i64 targetPC = jmp->label().address();
                i64 targetBB = findBlockIndexByStart(_basicBlocks, targetPC);
                if(targetBB != static_cast<size_t>(-1)) {
                    block.successors.push_back(targetBB);
                }
                continue; // 无 fall-through
            }

            // 情况 3：条件跳转（两条后继：true + fall-through）
            if(lastInst->opcode() == TacOpCode::JmpE || lastInst->opcode() == TacOpCode::JmpNE) { // 建议加个虚函数

                // true 分支
                const i64 targetPC = lastInst->ops()[0].value<Label>().address();
                i64 targetBB = findBlockIndexByStart(_basicBlocks, targetPC);
                if(targetBB != -1) {
                    block.successors.push_back(targetBB);
                }

                // false 分支（fall-through）
                if(bbIdx + 1 < _basicBlocks.size()) {
                    // 验证下一个块确实是紧接着的（防御性）
                    if(_basicBlocks[bbIdx + 1].startIdx == block.endIdx + 1) {
                        block.successors.push_back(bbIdx + 1);
                    }
                }
                continue;
            }

            // 情况 4：普通 fall-through（最常见）
            if(bbIdx + 1 < _basicBlocks.size()) {
                if(_basicBlocks[bbIdx + 1].startIdx == block.endIdx + 1) {
                    block.successors.push_back(bbIdx + 1);
                }
            }
        }

        return _basicBlocks;
    }

    void CFGVisualizer::exportToDot(const std::string &filename, const Vec<BasicBlock> &blocks) {
        if(blocks.empty())
            return;

        std::ofstream file(filename);
        if(!file.is_open())
            return;

        file << "digraph CFG {\n";
        file << "    rankdir=TB;\n"; // 优化：从上到下布局，更像反汇编窗口
        file << "    node [shape=box, fontname=\"Consolas\", fontsize=10, style=filled, fillcolor=\"#f9f9f9\", "
                "peripheries=1];\n";
        file << "    edge [fontname=\"Consolas\", fontsize=9, arrowhead=vee];\n\n";

        for(size_t i = 0; i < blocks.size(); ++i) {
            const auto &block = blocks[i];
            const auto &instructions = block.parentChunk->getInstVec();

            // --- 1. 渲染块内容 ---
            std::stringstream instList;
            instList << "BLOCK " << i << " (Idx: " << block.startIdx << " - " << block.endIdx << ")\\l";
            instList << "--------------------------\\l";

            // 修改：使用明确的 start_idx 到 end_idx，避免 address() 误用
            for(size_t j = block.startIdx; j <= block.endIdx && j < instructions.size(); ++j) {
                if(auto &inst = instructions[j]) { // 安全检查
                    std::string s = inst->dump(nullptr);
                    instList << "[" << j << "] " << escapeForDot(s) << "\\l";
                }
            }

            // 优化：根据块类型调整样式（如返回块用不同颜色）
            std::string extraStyle;
            if(!instructions.empty() && block.endIdx < instructions.size()) {
                if(const auto &lastInst = instructions[block.endIdx];
                   lastInst && lastInst->opcode() == TacOpCode::Ret) {
                    extraStyle = ", fillcolor=\"#ffdddd\""; // 浅红表示返回
                }
            }

            file << "    block_" << i << " [label=\"" << instList.str() << "\", align=left" << extraStyle << "];\n";

            // --- 2. 渲染连线 (Edges) ---
            // 修改：使用 successors 列表，避免假设 i+1 是 fall-through
            for(size_t succ : block.successors) {
                // 根据边类型添加属性（label, color 等）
                std::string edgeAttr = getEdgeAttributes(i, succ, blocks, instructions);
                file << "    block_" << i << " -> block_" << succ << edgeAttr << ";\n";
            }
        }

        file << "}\n";
        file.close();
    }
} // namespace cial::inter