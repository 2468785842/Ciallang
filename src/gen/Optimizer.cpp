//
// Created by LiDong on 2026/1/24.
//

#include "Optimizer.hpp"

namespace cial::inter {
    using namespace vm;

    void Optimizer::removeNopInst(Vec<Instruction> &instVec) {
        // if(instVec.empty())
        //     return;
        //
        // const size_t oldLen = instVec.size();
        // Vec<size_t> oldToNew(oldLen, 0);
        //
        // int write = 0; // if write != i this op is nop
        // for(size_t i = 0; i < oldLen; ++i) {
        //     oldToNew[i] = write; // 记录：旧read -> 新write
        //     if(instVec[i].opcode() != OpCode::NOP) {
        //         if(write != i) {
        //             instVec[write] = instVec[i];
        //         }
        //         ++write;
        //     }
        // }
        //
        // instVec.erase(instVec.begin() + write, instVec.end());
        //
        // // 更新跳转
        // for(auto &inst : instVec) {
        //     OpCode op = inst.opcode();
        //     if(op == OpCode::Jmp || op == OpCode::JmpE || op == OpCode::JmpNE) {
        //         Label oldTarget = Jmp::label(inst);
        //         const size_t oldAddr = oldTarget.address();
        //
        //         assert(oldAddr < oldToNew.size());
        //
        //         Jmp::target(inst, Label{ oldToNew[oldAddr] });
        //     }
        // }
    }
} // namespace cial::inter