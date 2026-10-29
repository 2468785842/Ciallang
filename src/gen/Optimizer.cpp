//
// Created by LiDong on 2026/1/24.
//

#include "Optimizer.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace cial::inter {
    using namespace vm;

    bool OptimizerManager::profilingEnabled() noexcept {
        static const bool enabled = [] {
#ifdef _WIN32
            char *value = nullptr;
            size_t length = 0;
            if(_dupenv_s(&value, &length, "CIAL_OPTIMIZER_STATS") != 0)
                return false;
            const bool isEnabled = value != nullptr && length > 1;
            std::free(value);
            return isEnabled;
#else
            const char *value = std::getenv("CIAL_OPTIMIZER_STATS");
            return value != nullptr && *value != '\0';
#endif
        }();
        return enabled;
    }

    namespace {
        using RegisterSet = Vec<u64>;

        template <typename Callback>
        void forEachRegisterOf(const Instruction &instruction, Callback &&callback) {
            const auto addRegister = [&](const Operand &operand) {
                if(operand.type() == Operand::Type::Register)
                    callback(operand.value<Register>());
            };

            addRegister(instruction.getOp1());
            addRegister(instruction.getOp2());
            addRegister(instruction.getOp3());

            switch(instruction.opcode()) {
                case TacOpCode::GMethodC:
                    callback(static_cast<const GMethodC &>(instruction).receiver);
                    break;
                case TacOpCode::CallMethodV:
                    callback(static_cast<const CallMethodV &>(instruction).receiver);
                    [[fallthrough]];
                case TacOpCode::CallV: {
                    const auto &call = static_cast<const CallV &>(instruction);
                    callback(call.func);
                    for(const auto reg : call.args)
                        callback(reg);
                    callback(call.dst);
                    break;
                }
                case TacOpCode::CallOmit: {
                    const auto &call = static_cast<const CallOmit &>(instruction);
                    callback(call.func);
                    callback(call.dst);
                    break;
                }
                case TacOpCode::CallExpand: {
                    const auto &call = static_cast<const CallExpand &>(instruction);
                    callback(call.func);
                    callback(call.dst);
                    for(const auto &[_, reg] : call.entries)
                        callback(reg);
                    break;
                }
                case TacOpCode::CollapseArgs:
                    callback(static_cast<const CollapseArgs &>(instruction).dst);
                    break;
                default:
                    break;
            }
        }

        bool isSet(const RegisterSet &set, const u16 reg) {
            const size_t word = reg / 64;
            return word < set.size() && (set[word] & (u64{ 1 } << (reg % 64))) != 0;
        }

        void setRegister(RegisterSet &set, const u16 reg) {
            const size_t word = reg / 64;
            if(word < set.size())
                set[word] |= u64{ 1 } << (reg % 64);
        }

        void clearRegister(RegisterSet &set, const u16 reg) {
            const size_t word = reg / 64;
            if(word < set.size())
                set[word] &= ~(u64{ 1 } << (reg % 64));
        }

        bool mergeRegisters(RegisterSet &destination, const RegisterSet &source) {
            bool changed = false;
            for(size_t i = 0; i < destination.size(); ++i) {
                const u64 merged = destination[i] | source[i];
                changed |= merged != destination[i];
                destination[i] = merged;
            }
            return changed;
        }

        bool checkedAdd(const Integer lhs, const Integer rhs, Integer &result) {
            constexpr auto min = std::numeric_limits<Integer>::min();
            constexpr auto max = std::numeric_limits<Integer>::max();
            if((rhs > 0 && lhs > max - rhs) || (rhs < 0 && lhs < min - rhs))
                return false;
            result = lhs + rhs;
            return true;
        }

        bool checkedSub(const Integer lhs, const Integer rhs, Integer &result) {
            constexpr auto min = std::numeric_limits<Integer>::min();
            constexpr auto max = std::numeric_limits<Integer>::max();
            if((rhs > 0 && lhs < min + rhs) || (rhs < 0 && lhs > max + rhs))
                return false;
            result = lhs - rhs;
            return true;
        }

        bool checkedMul(const Integer lhs, const Integer rhs, Integer &result) {
            constexpr auto min = std::numeric_limits<Integer>::min();
            constexpr auto max = std::numeric_limits<Integer>::max();
            if(lhs == 0 || rhs == 0) {
                result = 0;
                return true;
            }
            if((lhs == -1 && rhs == min) || (rhs == -1 && lhs == min))
                return false;
            if(lhs > 0) {
                if((rhs > 0 && lhs > max / rhs) || (rhs < 0 && rhs < min / lhs))
                    return false;
            } else {
                if((rhs > 0 && lhs < min / rhs) || (rhs < 0 && lhs < max / rhs))
                    return false;
            }
            result = lhs * rhs;
            return true;
        }

        bool tryFoldIntegerBinary(const TacOpCode opcode, const Integer lhs, const Integer rhs, Integer &result) {
            switch(opcode) {
                case TacOpCode::Add:
                    return checkedAdd(lhs, rhs, result);
                case TacOpCode::Sub:
                    return checkedSub(lhs, rhs, result);
                case TacOpCode::Mul:
                    return checkedMul(lhs, rhs, result);
                default:
                    return false;
            }
        }

        bool tryGetImmediateLoadDestination(const Instruction &instruction, Register &destination) {
            switch(instruction.opcode()) {
                case TacOpCode::LoadImm:
                    destination = instruction.getOp1().value<Register>();
                    return true;
                default:
                    return false;
            }
        }

        bool protectedByTry(const Chunk &chunk, const BasicBlock &block) {
            return std::ranges::any_of(chunk.getThrowHandlers(), [&](const ThrowHandler &handler) {
                return block.start < handler.tryEnd && handler.tryStart < block.end;
            });
        }
    } // namespace

    size_t Optimizer::removeNopInst(Chunk &chunk) {
        auto &instructions = chunk.getInstVec();
        const size_t oldSize = instructions.size();
        if(oldSize == 0 || std::ranges::none_of(instructions, [](const auto &instruction) {
               return instruction->opcode() == TacOpCode::NOP;
           }))
            return 0;

        Vec<size_t> oldBoundaryToNew(oldSize + 1, 0);
        for(size_t i = 0; i < oldSize; ++i) {
            oldBoundaryToNew[i + 1] = oldBoundaryToNew[i] + (instructions[i]->opcode() == TacOpCode::NOP ? 0 : 1);
        }

        Vec<Box<Instruction>> compacted;
        compacted.reserve(oldBoundaryToNew.back());
        for(auto &instruction : instructions) {
            if(instruction->opcode() != TacOpCode::NOP)
                compacted.push_back(std::move(instruction));
        }
        instructions = std::move(compacted);

        for(auto &instruction : instructions) {
            switch(instruction->opcode()) {
                case TacOpCode::Jmp:
                case TacOpCode::JmpE:
                case TacOpCode::JmpNE: {
                    const size_t oldTarget = instruction->getOp1().value<Label>().address();
                    if(oldTarget > oldSize)
                        throw std::runtime_error("jump target exceeds TAC instruction count");
                    instruction->setOp1Val(Label{ oldBoundaryToNew[oldTarget] });
                    break;
                }
                default:
                    break;
            }
        }

        for(auto &handler : chunk.getThrowHandlers()) {
            if(handler.tryStart > oldSize || handler.tryEnd > oldSize)
                throw std::runtime_error("try handler range exceeds TAC instruction count");
            handler.tryStart = oldBoundaryToNew[handler.tryStart];
            handler.tryEnd = oldBoundaryToNew[handler.tryEnd];
        }

        auto &lineEntries = chunk.rlc().rawEntries();
        Vec<std::pair<size_t, Common::SourceLocation>> remappedLines;
        remappedLines.reserve(lineEntries.size());
        for(const auto &[oldIndex, location] : lineEntries) {
            if(oldIndex > oldSize)
                throw std::runtime_error("source line index exceeds TAC instruction count");
            const size_t newIndex = oldBoundaryToNew[oldIndex];
            if(!remappedLines.empty() && remappedLines.back().first == newIndex)
                remappedLines.back().second = location;
            else
                remappedLines.emplace_back(newIndex, location);
        }
        lineEntries = std::move(remappedLines);
        return oldSize - instructions.size();
    }

    size_t CompareTestFusionOptimizer::optimize(Chunk &chunk, const Vec<BasicBlock> &blocks) {
        size_t count = 0;
        for(const auto &block : blocks) {
            for(size_t pc = block.start; pc + 1 < block.end; ++pc) {
                const auto &cmp = *chunk.inst(pc);
                switch(cmp.opcode()) {
                    case TacOpCode::EQ:
                    case TacOpCode::NEQ:
                    case TacOpCode::AbsEQ:
                    case TacOpCode::AbsNEQ:
                    case TacOpCode::LT:
                    case TacOpCode::LE:
                    case TacOpCode::GT:
                    case TacOpCode::GE:
                        break;
                    default:
                        continue;
                }
                const auto &test = *chunk.inst(pc + 1);
                if(test.opcode() != TacOpCode::Test || test.getOp1() != cmp.getOp3().value<Register>())
                    continue;
                const auto op = cmp.opcode();
                const auto lhs = cmp.getOp1().value<Register>();
                const auto rhs = cmp.getOp2().value<Register>();
                const auto dst = cmp.getOp3().value<Register>();
                chunk.getInstVec()[pc] = std::make_unique<CompareTest>(op, lhs, rhs, dst);
                chunk.repl<NOP>(pc + 1);
                ++count;
            }
        }
        return count;
    }

    size_t IntegerConstantPropagationOptimizer::optimize(Chunk &chunk, const Vec<BasicBlock> &blocks) {
        bool hasImmediate = false;
        bool hasArithmetic = false;
        for(const auto &instruction : chunk.getInstVec()) {
            hasImmediate |= instruction->opcode() == TacOpCode::LoadImm;
            hasArithmetic |= instruction->opcode() == TacOpCode::Add || instruction->opcode() == TacOpCode::Sub ||
                instruction->opcode() == TacOpCode::Mul;
            if(hasImmediate && hasArithmetic)
                break;
        }
        if(!hasImmediate || !hasArithmetic)
            return 0;

        size_t foldedCount = 0;
        Vec<Integer> constants(chunk.getRegCount());
        Vec<u8> known(chunk.getRegCount(), 0);
        for(const auto &block : blocks) {
            std::fill(known.begin(), known.end(), 0);
            for(size_t pc = block.start; pc < block.end; ++pc) {
                const Instruction &instruction = *chunk.inst(pc);
                const TacOpCode opcode = instruction.opcode();
                if(opcode == TacOpCode::LoadImm) {
                    const Register destination = instruction.getOp1().value<Register>();
                    const Integer value = instruction.getOp2().value<Integer>();
                    if(destination.index() < known.size()) {
                        constants[destination.index()] = value;
                        known[destination.index()] = 1;
                    }
                    continue;
                }

                if(opcode == TacOpCode::Add || opcode == TacOpCode::Sub || opcode == TacOpCode::Mul) {
                    const Register lhsReg = instruction.getOp1().value<Register>();
                    const Register rhsReg = instruction.getOp2().value<Register>();
                    const Register destination = instruction.getOp3().value<Register>();
                    Integer result{};
                    const bool operandsKnown = lhsReg.index() < known.size() && rhsReg.index() < known.size() &&
                        known[lhsReg.index()] && known[rhsReg.index()];
                    if(operandsKnown &&
                       tryFoldIntegerBinary(opcode, constants[lhsReg.index()], constants[rhsReg.index()], result)) {
                        chunk.repl<LoadImm>(pc, destination, result);
                        ++foldedCount;
                        if(destination.index() < known.size()) {
                            constants[destination.index()] = result;
                            known[destination.index()] = 1;
                        }
                        continue;
                    }
                }

                // Any non-literal instruction may mutate or reinterpret its register
                // operands. Forget those facts; no value is propagated across it.
                forEachRegisterOf(instruction, [&](const Register reg) {
                    if(reg.index() < known.size())
                        known[reg.index()] = 0;
                });
            }
        }
        return foldedCount;
    }

    size_t DeadLoadEliminationOptimizer::optimize(Chunk &chunk, const Vec<BasicBlock> &blocks) {
        const bool hasImmediate = std::ranges::any_of(
            chunk.getInstVec(), [](const auto &instruction) { return instruction->opcode() == TacOpCode::LoadImm; });
        if(!hasImmediate)
            return 0;

        const size_t blockCount = blocks.size();
        const size_t registerCount = chunk.getRegCount();
        const size_t wordCount = (registerCount + 63) / 64;
        Vec<RegisterSet> blockUse(blockCount);
        Vec<RegisterSet> blockDefs(blockCount);
        Vec<RegisterSet> liveIn(blockCount);
        Vec<RegisterSet> liveOut(blockCount);
        Vec<RegisterSet> nextIn(blockCount);
        Vec<RegisterSet> nextOut(blockCount);
        Vec<bool> preserveAll(blockCount, false);

        size_t removedCount = 0;
        for(size_t i = 0; i < blockCount; ++i) {
            const auto &block = blocks[i];
            blockUse[i].resize(wordCount);
            blockDefs[i].resize(wordCount);
            liveIn[i].resize(wordCount);
            liveOut[i].resize(wordCount);
            nextIn[i].resize(wordCount);
            nextOut[i].resize(wordCount);
            preserveAll[i] = protectedByTry(chunk, block);
            for(size_t pc = block.start; pc < block.end; ++pc) {
                const Instruction &instruction = *chunk.inst(pc);
                Register destination;
                if(tryGetImmediateLoadDestination(instruction, destination)) {
                    setRegister(blockDefs[i], destination.index());
                    continue;
                }
                forEachRegisterOf(instruction, [&](const Register reg) {
                    if(!isSet(blockDefs[i], reg.index()))
                        setRegister(blockUse[i], reg.index());
                });
            }
        }

        bool anyChanged;
        do {
            anyChanged = false;
            for(size_t reverse = blockCount; reverse > 0; --reverse) {
                const size_t i = reverse - 1;
                auto &out = nextOut[i];
                std::fill(out.begin(), out.end(), 0);
                for(const size_t successor : blocks[i].successors)
                    mergeRegisters(out, liveIn[successor]);
                // parseAst may return a value in a register without emitting a Ret
                // (for example VM::evalExpr). That value is observable after any
                // normal exit from the chunk and must be a liveness root.
                if(externallyObserved && blocks[i].successors.empty())
                    setRegister(out, externallyObserved->index());
                if(preserveAll[i]) {
                    std::fill(out.begin(), out.end(), ~u64{ 0 });
                    if(registerCount % 64 != 0 && !out.empty())
                        out.back() = (u64{ 1 } << (registerCount % 64)) - 1;
                }

                auto &in = nextIn[i];
                bool blockChanged = false;
                for(size_t word = 0; word < wordCount; ++word) {
                    in[word] = blockUse[i][word] | (out[word] & ~blockDefs[i][word]);
                    blockChanged |= in[word] != liveIn[i][word] || out[word] != liveOut[i][word];
                }
                if(blockChanged) {
                    liveOut[i] = out;
                    liveIn[i] = in;
                    anyChanged = true;
                }
            }
        } while(anyChanged);

        for(size_t i = 0; i < blockCount; ++i) {
            if(preserveAll[i])
                continue;
            RegisterSet live = liveOut[i];
            const auto &block = blocks[i];
            for(size_t reverse = block.end; reverse > block.start; --reverse) {
                const size_t pc = reverse - 1;
                const Instruction &instruction = *chunk.inst(pc);
                Register destination;
                if(tryGetImmediateLoadDestination(instruction, destination)) {
                    if(!isSet(live, destination.index())) {
                        chunk.repl<NOP>(pc);
                        ++removedCount;
                    } else {
                        clearRegister(live, destination.index());
                    }
                    continue;
                }
                forEachRegisterOf(instruction, [&](const Register reg) { setRegister(live, reg.index()); });
            }
        }
        return removedCount;
    }

} // namespace cial::inter
