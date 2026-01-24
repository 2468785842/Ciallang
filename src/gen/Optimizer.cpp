//
// Created by LiDong on 2026/1/24.
//

#include "Optimizer.hpp"

namespace cial::Inter {
    using namespace Bytecode;

    void Optimizer::removeNopInst(Vec<Instruction> &instVec) {
        if(instVec.empty())
            return;

        const size_t oldLen = instVec.size(); // 记录旧长度
        Vec<size_t> oldToNew(oldLen, -1); // +1防越界，默认无效

        size_t write = 0; // if write != i this op is nop
        for(size_t i = 0; i < oldLen; ++i) {
            oldToNew[i] = write; // 记录：旧read -> 新write
            if(instVec[i].opcode() != OpCode::NOP) {
                if(write != i) {
                    instVec[write] = std::move(instVec[i]);
                }
                ++write;
            }
        }

        instVec.erase(instVec.begin() + write, instVec.end());

        // 更新跳转
        for(auto &inst : instVec) {
            OpCode op = inst.opcode();
            if(op == OpCode::Jmp || op == OpCode::JmpE || op == OpCode::JmpNE) {
                Label oldTarget = Jmp::label(inst);
                const size_t oldAddr = oldTarget.address();

                if(oldAddr >= oldToNew.size() || oldToNew[oldAddr] == -1) {
                    // 防御：目标被删或越界。根据需求处理，比如抛异常、设无效label或忽略
                    // 示例：Jmp::target(inst, Label{0}); // 或自定义错误处理
                    // throw OptimizerError("jump target out of range");
                    assert(false);
                }

                Jmp::target(inst, Label{ oldToNew[oldAddr] });
            }
        }
    }

    bool Optimizer::usingRegister(Instruction &inst) { return true; }

    void LoadSubOptimizer::optimize(Chunk &chunk, const Vec<BasicBlock> &blocks) {
        const Vec<Instruction> &code = chunk.getInstVec();

        for(const auto &bb : blocks) {
            if(bb.hasSideEffect)
                continue;

            for(int i = bb.start; i <= bb.end; i++) {
                auto &load = code[i];
                if(load.opcode() != OpCode::Load)
                    continue;
                Register rC = Load::reg(load);
                ConstIdx K = Load::value(load);
                if(i + 1 <= bb.end) {
                    // auto &next = code[i + 1];
                    // if(next.opcode() == OpCode::Sub && Sub::src(next) == rC) {
                    //     bool usedAgain = false;
                    //     for(int j = i + 2; j <= bb.end; j++) {
                    //         if(code[j].src1 == rC || code[j].src2 == rC) {
                    //             usedAgain = true;
                    //             break;
                    //         }
                    //         if(code[j].dst == rC) {
                    //             break;
                    //         }
                    //     }
                    //
                    //     if(!usedAgain) {
                    //         next.op = Bytecode::Instruction::Op::SubImm;
                    //         next.imm = K;
                    //         next.src2 = -1;
                    //         code.erase(code.begin() + i); // 删除 load
                    //     }
                    // }
                }
            }
        }
    }

} // namespace cial::Inter