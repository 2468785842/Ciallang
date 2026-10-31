//
// Created by LiDong on 2026/1/24.
//

#include "ControlFlowGraph.hpp"

#include <algorithm>
#include <limits>
#include <sstream>
#include <stdexcept>

#include "vm/Chunk.hpp"

namespace cial::inter {
    using namespace vm;

    namespace {
        bool isConditionalJump(const TacOpCode opcode) {
            return opcode == TacOpCode::JmpE || opcode == TacOpCode::JmpNE;
        }

        bool isJump(const TacOpCode opcode) { return opcode == TacOpCode::Jmp || isConditionalJump(opcode); }

        bool isTerminator(const TacOpCode opcode) { return opcode == TacOpCode::Ret || opcode == TacOpCode::Throw; }

        bool hasSideEffects(const TacOpCode opcode) {
            // These are the only instructions whose execution is unconditionally
            // non-observable and non-throwing. Everything else is a barrier.
            return opcode != TacOpCode::NOP && opcode != TacOpCode::Load && opcode != TacOpCode::LoadImm;
        }

        void addSuccessor(BasicBlock &block, const size_t successor) {
            if(std::ranges::find(block.successors, successor) == block.successors.end())
                block.successors.push_back(successor);
        }
    } // namespace

    Vec<BasicBlock> ControlFlowGraph::buildCFG(Chunk &chunk) {
        _basicBlocks.clear();
        const auto &code = chunk.getInstVec();
        const size_t codeSize = code.size();
        if(codeSize == 0)
            return _basicBlocks;

        bool singleBlock = chunk.getThrowHandlers().empty();
        for(size_t pc = 0; singleBlock && pc < codeSize; ++pc) {
            const TacOpCode opcode = code[pc]->opcode();
            if(isJump(opcode) || (isTerminator(opcode) && pc + 1 < codeSize))
                singleBlock = false;
        }
        if(singleBlock) {
            BasicBlock block{ &chunk, 0, codeSize };
            for(const auto &instruction : code) {
                if(hasSideEffects(instruction->opcode())) {
                    block.hasSideEffect = true;
                    break;
                }
            }
            _basicBlocks.push_back(std::move(block));
            _loops.clear();
            return _basicBlocks;
        }

        Vec<u8> isLeader(codeSize, 0);
        isLeader[0] = 1;
        const auto addLeader = [&](const size_t address) {
            if(address > codeSize)
                throw std::runtime_error("control-flow target exceeds TAC instruction count");
            if(address < codeSize)
                isLeader[address] = 1;
        };

        for(size_t pc = 0; pc < codeSize; ++pc) {
            const TacOpCode opcode = code[pc]->opcode();
            if(isJump(opcode)) {
                const Label target = code[pc]->getOp1().value<Label>();
                addLeader(target.address());
                addLeader(pc + 1);
            } else if(isTerminator(opcode)) {
                addLeader(pc + 1);
            }
        }

        // Keep protected and unprotected code in separate blocks. Optimizers can
        // then conservatively preserve all state across implicit exception edges.
        for(const auto &handler : chunk.getThrowHandlers()) {
            addLeader(static_cast<size_t>(handler.tryStart));
            addLeader(static_cast<size_t>(handler.tryEnd));
        }

        Vec<size_t> leaderList;
        for(size_t pc = 0; pc < codeSize; ++pc) {
            if(isLeader[pc])
                leaderList.push_back(pc);
        }

        const size_t invalidBlock = std::numeric_limits<size_t>::max();
        Vec<size_t> blockAt(codeSize, invalidBlock);
        _basicBlocks.reserve(leaderList.size());
        for(size_t i = 0; i < leaderList.size(); ++i) {
            const size_t start = leaderList[i];
            const size_t end = i + 1 < leaderList.size() ? leaderList[i + 1] : codeSize;
            if(start >= end)
                continue;

            const size_t blockIndex = _basicBlocks.size();
            std::fill(blockAt.begin() + static_cast<std::ptrdiff_t>(start),
                      blockAt.begin() + static_cast<std::ptrdiff_t>(end), blockIndex);
            BasicBlock block{ &chunk, start, end };
            for(size_t pc = start; pc < end; ++pc) {
                if(hasSideEffects(code[pc]->opcode())) {
                    block.hasSideEffect = true;
                    break;
                }
            }
            _basicBlocks.push_back(std::move(block));
        }

        for(size_t i = 0; i < _basicBlocks.size(); ++i) {
            auto &block = _basicBlocks[i];
            const size_t lastPc = block.end - 1;
            const TacOpCode opcode = code[lastPc]->opcode();

            if(opcode == TacOpCode::Jmp || isConditionalJump(opcode)) {
                const size_t target = code[lastPc]->getOp1().value<Label>().address();
                if(target < codeSize && blockAt[target] != invalidBlock)
                    addSuccessor(block, blockAt[target]);
                if(isConditionalJump(opcode) && i + 1 < _basicBlocks.size())
                    addSuccessor(block, i + 1);
            } else if(!isTerminator(opcode) && i + 1 < _basicBlocks.size()) {
                addSuccessor(block, i + 1);
            }
        }

        for(const auto &handler : chunk.getThrowHandlers()) {
            if(handler.tryStart > handler.tryEnd)
                throw std::runtime_error("try handler range is inverted");
            if(handler.tryEnd >= codeSize || blockAt[handler.tryEnd] == invalidBlock)
                continue;
            for(auto &block : _basicBlocks) {
                if(block.start < handler.tryEnd && handler.tryStart < block.end)
                    addSuccessor(block, blockAt[handler.tryEnd]);
            }
        }

        detectLoops();
        return _basicBlocks;
    }

    void ControlFlowGraph::detectLoops() {
        _loops.clear();
        const size_t blockCount = _basicBlocks.size();
        if(blockCount == 0)
            return;

        // 前驱表
        Vec<Vec<size_t>> predecessors(blockCount);
        for(size_t i = 0; i < blockCount; ++i) {
            for(const size_t successor : _basicBlocks[i].successors)
                predecessors[successor].push_back(i);
        }

        // 入口可达性：不可达块不参与支配与循环判定
        Vec<bool> reachable(blockCount, false);
        Vec<size_t> reachableStack{ 0 };
        reachable[0] = true;
        while(!reachableStack.empty()) {
            const size_t current = reachableStack.back();
            reachableStack.pop_back();
            for(const size_t successor : _basicBlocks[current].successors) {
                if(!reachable[successor]) {
                    reachable[successor] = true;
                    reachableStack.push_back(successor);
                }
            }
        }

        // 迭代数据流求支配集：dom[b] = {b} ∪ ⋂ dom[p]（可达前驱）。
        // 块数量为函数级规模，不动点迭代代价可忽略。
        Vec<Vec<bool>> dominators(blockCount);
        dominators[0].assign(blockCount, false);
        dominators[0][0] = true;
        for(size_t i = 1; i < blockCount; ++i)
            dominators[i].assign(blockCount, reachable[i]);

        bool changed = true;
        while(changed) {
            changed = false;
            for(size_t i = 1; i < blockCount; ++i) {
                if(!reachable[i])
                    continue;
                Vec<bool> next(blockCount, false);
                bool hasReachablePredecessor = false;
                for(const size_t predecessor : predecessors[i]) {
                    if(!reachable[predecessor])
                        continue;
                    if(!hasReachablePredecessor) {
                        next = dominators[predecessor];
                        hasReachablePredecessor = true;
                    } else {
                        for(size_t k = 0; k < blockCount; ++k)
                            next[k] = next[k] && dominators[predecessor][k];
                    }
                }
                next[i] = true;
                if(next != dominators[i]) {
                    dominators[i] = std::move(next);
                    changed = true;
                }
            }
        }

        // 回边 source→target：边终点 target 支配边起点 source，target 即循环头；
        // 同表头的多条回边合并为一个自然循环
        Map<size_t, size_t> headerToLoop;
        for(size_t target = 0; target < blockCount; ++target) {
            if(!reachable[target])
                continue;
            for(const size_t source : predecessors[target]) {
                if(!reachable[source] || !dominators[source][target])
                    continue;

                size_t loopIndex;
                const auto existing = headerToLoop.find(target);
                if(existing == headerToLoop.end()) {
                    loopIndex = _loops.size();
                    headerToLoop.emplace(target, loopIndex);
                    _loops.push_back(LoopInfo{ target, {} });
                } else {
                    loopIndex = existing->second;
                }

                Vec<bool> inLoop(blockCount, false);
                Vec<size_t> worklist;
                inLoop[target] = true;
                // 合并同表头循环：既有 body 的前驱已收集过，只需扩散新回边的 source
                for(const size_t block : _loops[loopIndex].body)
                    inLoop[block] = true;
                if(!inLoop[source]) {
                    inLoop[source] = true;
                    worklist.push_back(source);
                }
                while(!worklist.empty()) {
                    const size_t node = worklist.back();
                    worklist.pop_back();
                    for(const size_t predecessor : predecessors[node]) {
                        if(!reachable[predecessor] || inLoop[predecessor])
                            continue;
                        inLoop[predecessor] = true;
                        worklist.push_back(predecessor);
                    }
                }
                auto &body = _loops[loopIndex].body;
                body.clear();
                for(size_t i = 0; i < blockCount; ++i) {
                    if(inLoop[i])
                        body.push_back(i);
                }
            }
        }

        std::ranges::sort(_loops, [](const LoopInfo &lhs, const LoopInfo &rhs) { return lhs.header < rhs.header; });
    }

    std::string ControlFlowGraph::dumpDot(vm::Chunk &chunk) const {
        const auto &code = chunk.getInstVec();

        // dumpInstructions 每行形如 "@<idx>    : <text>"，按行前缀切出单条指令文本
        Vec<std::string> instText(code.size());
        std::istringstream dumpStream{ chunk.dumpInstructions().toStdStr() };
        std::string line;
        while(std::getline(dumpStream, line)) {
            if(line.size() < 2 || line[0] != '@')
                continue;
            size_t pos = 1;
            size_t index = 0;
            while(pos < line.size() && line[pos] >= '0' && line[pos] <= '9') {
                index = index * 10 + static_cast<size_t>(line[pos] - '0');
                ++pos;
            }
            if(pos == 1 || index >= code.size())
                continue;
            const size_t separator = line.find(':', pos);
            if(separator == std::string::npos)
                continue;
            size_t begin = separator + 1;
            while(begin < line.size() && line[begin] == ' ')
                ++begin;
            instText[index] = line.substr(begin);
        }

        auto escape = [](const std::string &text) {
            std::string out;
            out.reserve(text.size());
            for(const char ch : text) {
                if(ch == '"')
                    out += "\\\"";
                else if(ch == '\\')
                    out += "\\\\";
                else
                    out += ch;
            }
            return out;
        };

        auto isLoopHeader = [&](const size_t block) {
            return std::ranges::any_of(_loops, [&](const LoopInfo &loop) { return loop.header == block; });
        };
        // 边 u→v 是回边当且仅当 v 是某循环的 header 且 u 在该循环体内
        auto isBackEdge = [&](const size_t source, const size_t target) {
            return std::ranges::any_of(_loops, [&](const LoopInfo &loop) {
                return loop.header == target && std::ranges::find(loop.body, source) != loop.body.end();
            });
        };

        std::ostringstream out;
        out << "digraph CFG {\n  node [shape=box fontname=monospace fontsize=10];\n";
        for(size_t i = 0; i < _basicBlocks.size(); ++i) {
            const auto &block = _basicBlocks[i];
            out << "  B" << i << " [label=\"B" << i << " [" << block.start << ".." << block.end << ")";
            if(isLoopHeader(i))
                out << " (loop header)";
            out << "\\n";
            for(size_t pc = block.start; pc < block.end; ++pc) {
                out << escape(instText[pc]);
                if(pc + 1 < block.end)
                    out << "\\n";
            }
            out << "\"";
            if(isLoopHeader(i))
                out << " fillcolor=lightblue style=filled";
            out << "];\n";
        }

        for(size_t i = 0; i < _basicBlocks.size(); ++i) {
            const auto &block = _basicBlocks[i];
            if(block.successors.empty())
                continue;
            // 按终结指令给边标注类别；try handler 的额外后继排在尾部，标 exc
            const TacOpCode lastOp = code[block.end - 1]->opcode();
            const char *kinds[2] = { "fall", "fall" };
            size_t expected = 1;
            if(lastOp == TacOpCode::Jmp)
                kinds[0] = "jmp";
            else if(isConditionalJump(lastOp)) {
                kinds[0] = "cond";
                kinds[1] = "fall";
                expected = 2;
            }
            for(size_t successorIndex = 0; successorIndex < block.successors.size(); ++successorIndex) {
                const size_t successor = block.successors[successorIndex];
                const char *kind = successorIndex < expected ? kinds[successorIndex] : "exc";
                out << "  B" << i << " -> B" << successor;
                if(isBackEdge(i, successor))
                    out << " [color=red label=\"back " << kind << "\"]";
                else
                    out << " [label=\"" << kind << "\"]";
                out << ";\n";
            }
        }
        out << "}\n";
        return out.str();
    }

} // namespace cial::inter
