//
// Created by LiDong on 2026/1/24.
//

#include "Optimizer.hpp"

#include <ranges>
#include <stack>

namespace cial::inter {
    using namespace vm;

    void Optimizer::removeNopInst(Vec<Box<Instruction>> &instVec) {
        if(instVec.empty())
            return;

        const size_t oldLen = instVec.size();
        Vec<i64> oldToNew(oldLen, 0);

        int write = 0; // if write != i this op is nop
        for(size_t i = 0; i < oldLen; ++i) {
            oldToNew[i] = write; // 记录：旧read -> 新write
            if(instVec[i]->opcode() != TacOpCode::NOP) {
                if(write != i) {
                    instVec[write] = std::move(instVec[i]);
                }
                ++write;
            }
        }

        instVec.erase(instVec.begin() + write, instVec.end());

        // 更新跳转
        for(auto &inst : instVec) {
            const TacOpCode op = inst->opcode();
            if(op == TacOpCode::Jmp || op == TacOpCode::JmpE || op == TacOpCode::JmpNE) {
                auto oldTarget = inst->ops()[0].value<Label>();
                const i64 oldAddr = oldTarget.address();

                assert(oldAddr < oldToNew.size());
                inst->ops()[0] = Operand{ Label{ oldToNew[oldAddr] } };
            }
        }
    }

    // 计算每个寄存器的生存周期 [first_seen, last_seen]
    Vec<Interval> RegisterAllocator::computeIntervals(TacChunk &chunk) {
        std::map<Register, Interval> intervalMap;

        for(int i = 0; i < static_cast<int>(chunk.getInstVec().size()); ++i) {
            auto &inst = chunk.getInstVec()[i];

            // 辅助函数：更新寄存器的出现范围
            auto updateRange = [&](const Register r) {
                if(r.index() < 0)
                    return; // 忽略非寄存器值
                if(!intervalMap.contains(r)) {
                    intervalMap[r] = { r, i, i, Register{ -1 } };
                } else {
                    intervalMap[r].end = i;
                }
            };

            for(auto &op : inst->ops()) {
                if(op.type() == Operand::Type::Register)
                    updateRange(op.value<Register>());
            }
        }

        std::vector<Interval> result;
        for(auto &val : intervalMap | std::views::values)
            result.push_back(val);
        std::sort(result.begin(), result.end());
        return result;
    }


    void RegisterAllocator::allocate(Vec<Interval> &intervals) {
        std::stack<u16> freePool;

        // 倒序初始化空闲池，这样 pop 时从小到大分配
        for(u16 i = 65534; i > 0; --i)
            freePool.push(i);

        freePool.push(0);

        // 正在占用的寄存器，按结束时间排序
        // auto compareEnd = [](Interval *a, Interval *b) { return a->end > b->end; };
        std::vector<Interval *> active;

        for(auto &interval : intervals) {
            // 过期检查：释放所有结束位置早于当前起始位置的寄存器
            auto it = active.begin();
            while(it != active.end()) {
                if((*it)->end < interval.start) {
                    auto reg = (*it)->pReg;
                    if(reg.index() >= std::numeric_limits<u16>::max()) {
                        throw std::runtime_error("RegisterAllocator: register index out of range");
                    }
                    freePool.push(reg.index());
                    it = active.erase(it);
                } else {
                    ++it;
                }
            }

            // 分配物理寄存器
            if(!freePool.empty()) {
                interval.pReg = Register{ freePool.top() };
                freePool.pop();
                active.push_back(&interval);
            } else {
                // 这种情况理论上在 65k 寄存器下极难发生
                // 如果发生，则需要处理 Spill (溢出到内存/栈)
                throw std::runtime_error("RegisterAllocator: register index out of range");
            }
        }
    }

    void RegisterAllocator::rewriteInstructions(TacChunk &chunk, const std::vector<Interval> &intervals) {
        std::map<Register, Register> vToP;

        for(const auto &interval : intervals) {
            vToP[interval.vReg] = interval.pReg;
        }

        for(const auto &inst : chunk.getInstVec()) {
            for(auto &op : inst->ops()) {
                if(op.type() != Operand::Type::Register)
                    continue;
                if(auto reg = op.value<Register>(); vToP.contains(reg))
                    op = Operand{ Register{ vToP[reg] } };
            }
        }

        for(auto &localVar : chunk.getLocalVars()) {
            localVar.reg = vToP[localVar.reg];
        }

        for(auto &th : chunk.getThrowHandlers()) {
            th.exValueReg = vToP[th.exValueReg];
        }
    }

} // namespace cial::inter