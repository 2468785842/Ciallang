//
// Created by LiDong on 2026/1/24.
//

#include "Optimizer.hpp"

namespace cial::Inter {
    using namespace Bytecode;

    void Optimizer::removeNopInst(Vec<Instruction> &instVec) {
        if(instVec.empty())
            return;

        const size_t oldLen = instVec.size();
        Vec<size_t> oldToNew(oldLen, 0);

        int write = 0; // if write != i this op is nop
        for(size_t i = 0; i < oldLen; ++i) {
            oldToNew[i] = write; // 记录：旧read -> 新write
            if(instVec[i].opcode() != OpCode::NOP) {
                if(write != i) {
                    instVec[write] = instVec[i];
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

                assert(oldAddr < oldToNew.size());

                Jmp::target(inst, Label{ oldToNew[oldAddr] });
            }
        }
    }
    bool Optimizer::isUsing(const Instruction &instruction, const Register &reg) {
        switch(instruction.opcode()) {
            case OpCode::NOP:
            case OpCode::PopN:
            case OpCode::Jmp:
            case OpCode::JmpE:
            case OpCode::JmpNE:
            case OpCode::Debugger:
                return false;

            // use op1
            case OpCode::Push:
            case OpCode::Inv:
            case OpCode::Test:
            case OpCode::LNot: // use op1 def op1 → 使用 dst 作为源
            case OpCode::ChgSign: // use op1 def op1
            case OpCode::Throw:
            case OpCode::Ret:
                return instruction.getOp1() && instruction.getOp1().value() == reg;

            // def op1
            case OpCode::Load:
            case OpCode::ILoad:
            case OpCode::Global:
            case OpCode::Super:
            case OpCode::This:
            case OpCode::ToInt:
            case OpCode::ToReal:
            case OpCode::ToString:
                return false;

            // use op1 op2 def op2
            case OpCode::Add:
            case OpCode::Sub:
            case OpCode::Mul:
            case OpCode::Div:
            case OpCode::Idiv:
            case OpCode::Mod:
            case OpCode::EQ:
            case OpCode::NEQ:
            case OpCode::LT:
            case OpCode::LE:
            case OpCode::GT:
            case OpCode::GE:
            case OpCode::AbsEQ:
            case OpCode::AbsNEQ:
            case OpCode::LAnd:
                return (instruction.getOp1() && instruction.getOp1().value() == reg) ||
                    (instruction.getOp2() && instruction.getOp2().value() == reg);

            // use op1 op2 def op2   (op1 是立即数)
            case OpCode::IAdd:
            case OpCode::ISub:
                return IAdd::dst(instruction) == reg;

            // use op1 def op2
            case OpCode::Mov:
            case OpCode::CP:
                return Mov::src(instruction) == reg;

            // use op2
            case OpCode::DGlobal:
            case OpCode::DThis:
                return DGlobal::src(instruction) == reg; // op2

            // def op2
            case OpCode::GGlobal:
            case OpCode::GThis:
            case OpCode::GUpval:
                return false;

            // use op1 op2
            case OpCode::ChgThis:
                return ChgThis::dst(instruction) == reg || ChgThis::src(instruction) == reg;

            // use op1 op2 def op2
            case OpCode::ChkInv:
                return ChkInv::src(instruction) == reg || ChkInv::dst(instruction) == reg;

            // use op1 op2 def op1
            case OpCode::ChkIns:
                return ChkIns::dst(instruction) == reg || ChkIns::src(instruction) == reg;

            // use op1 op2 def op2
            case OpCode::LOr:
            case OpCode::BXor:
            case OpCode::BOr:
            case OpCode::BAnd:
            case OpCode::BlShift:
            case OpCode::BrShift:
            case OpCode::BurShift:
                return LOr::src(instruction) == reg || LOr::dst(instruction) == reg;

            // use op2 def op1
            case OpCode::Call:
                return Call::memberReg(instruction) == reg;

            // use op1 op2 def op3
            case OpCode::GProp:
                return GProp::obj(instruction) == reg || GProp::memberReg(instruction) == reg;

            // use op1 op2 op3
            case OpCode::DProp:
                return DProp::obj(instruction) == reg || DProp::memberReg(instruction) == reg ||
                    DProp::src(instruction) == reg;

            default:
                return false;
        }
    }
    bool Optimizer::isDefining(const Instruction &instruction, const Register &reg) {
        switch(instruction.opcode()) {
            case OpCode::NOP:
            case OpCode::Push:
            case OpCode::PopN:
            case OpCode::Jmp:
            case OpCode::JmpE:
            case OpCode::JmpNE:
            case OpCode::Debugger:
            case OpCode::DGlobal:
            case OpCode::DThis:
            case OpCode::DProp:
            case OpCode::Throw:
                return false;

            // def op1
            case OpCode::Load:
            case OpCode::ILoad:
            case OpCode::Global:
            case OpCode::Super:
            case OpCode::This:
            case OpCode::ToInt:
            case OpCode::ToReal:
            case OpCode::ToString:
                return instruction.getOp1() && instruction.getOp1().value() == reg;

            // use op1 op2 def op2
            case OpCode::Add:
            case OpCode::Sub:
            case OpCode::Mul:
            case OpCode::Div:
            case OpCode::Idiv:
            case OpCode::Mod:
            case OpCode::EQ:
            case OpCode::NEQ:
            case OpCode::LT:
            case OpCode::LE:
            case OpCode::GT:
            case OpCode::GE:
            case OpCode::AbsEQ:
            case OpCode::AbsNEQ:
            case OpCode::LAnd:
            case OpCode::ChkInv:
                return Add::dst(instruction) == reg;

            // use op1 op2 def op2   (op1 是立即数)
            case OpCode::IAdd:
            case OpCode::ISub:
                return IAdd::dst(instruction) == reg;

            // use op1 def op2
            case OpCode::Mov:
            case OpCode::CP:
                return Mov::dst(instruction) == reg;

            // def op2
            case OpCode::GGlobal:
            case OpCode::GThis:
            case OpCode::GUpval:
                return GGlobal::dst(instruction) == reg;

            // use op1 op2
            case OpCode::ChgThis:
                return ChgThis::dst(instruction) == reg;

            // use op1 def op1
            case OpCode::Inv:
            case OpCode::LNot:
            case OpCode::ChgSign:
                return instruction.getOp1() && instruction.getOp1().value() == reg;

            // use op1 op2 def op1
            case OpCode::ChkIns:
                return ChkIns::dst(instruction) == reg;

            // use op1 op2 def op2
            case OpCode::LOr:
            case OpCode::BXor:
            case OpCode::BOr:
            case OpCode::BAnd:
            case OpCode::BlShift:
            case OpCode::BrShift:
            case OpCode::BurShift:
                return LOr::dst(instruction) == reg;

            // use op2 def op1
            case OpCode::Call:
                return Call::dst(instruction) == reg;

            // use op1 op2 def op3
            case OpCode::GProp:
                return GProp::dst(instruction) == reg;

            default:
                return false;
        }
    }

    void LoadSubOptimizer::optimize(Chunk &chunk, const Vec<BasicBlock> &blocks) {
        Vec<Instruction> &code = chunk.getInstVec();

        for(const auto &bb : blocks) {
            if(bb.hasSideEffect)
                continue;

            for(int i = bb.start; i <= bb.end; i++) {
                auto &load = code[i];
                if(load.opcode() != OpCode::ILoad)
                    continue;
                Register rC = ILoad::dst(load);
                Integer K = ILoad::value(load);
                if(i + 1 <= bb.end) {
                    auto &next = code[i + 1];
                    if(next.opcode() == OpCode::Sub && Sub::src(next) == rC) {
                        bool usedAgain = false;
                        for(int j = i + 2; j <= bb.end; j++) {
                            if(isUsing(code[j], rC)) {
                                usedAgain = true;
                                break;
                            }
                            if(isDefining(code[j], rC)) {
                                break;
                            }
                        }
                        if(!usedAgain) {
                            next = Instruction{ OpCode::ISub, Operand{ K }, Operand{ Sub::dst(next) } };
                            code[i] = Instruction{ OpCode::NOP }; // delete inst
                        }
                    }
                }
            }
        }
    }

} // namespace cial::Inter