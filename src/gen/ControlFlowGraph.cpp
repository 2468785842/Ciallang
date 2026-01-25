//
// Created by LiDong on 2026/1/24.
//

#include "ControlFlowGraph.hpp"

#include <unordered_set>

#include "vm/Chunk.hpp"

namespace cial::Inter {
    using namespace Bytecode;

    Vec<BasicBlock> ControlFlowGraph::buildCFG(Chunk &chunk) {
        const Vec<Instruction> &code = chunk.getInstVec();
        std::unordered_set<int> leaders;
        leaders.insert(0);

        for(int i = 0; i < code.size(); i++) {
            const auto &ins = code[i];
            if(ins.opcode() == OpCode::Jmp) {
                leaders.insert(static_cast<int>(Jmp::label(ins).address()));
                leaders.insert(i + 1); // fallthrough
            }
            if(ins.opcode() == OpCode::Ret) {
                leaders.insert(i + 1);
            }
        }

        Vec<int> leaderList{ leaders.begin(), leaders.end() };
        std::ranges::sort(leaderList);

        for(int i = 0; i < leaderList.size(); i++) {
            const int start = leaderList[i];
            const int end = i + 1 < leaderList.size() ? leaderList[i + 1] - 1 : code.size() - 1;

            BasicBlock bb{ &chunk, start, end };

            // 副作用扫描
            for(int pc = start; pc <= end; pc++) {
                if(code[pc].opcode() == OpCode::Call) {
                    bb.hasSideEffect = true;
                    break;
                }
            }

            _basicBlocks.push_back(bb);
        }

        return _basicBlocks;
    }

} // namespace cial::Inter