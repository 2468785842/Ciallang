//
// Created by LiDong on 2026/1/24.
//

#include "ControlFlowGraph.hpp"

#include <algorithm>
#include <limits>
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

        return _basicBlocks;
    }

} // namespace cial::inter
