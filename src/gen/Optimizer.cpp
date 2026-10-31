//
// Created by LiDong on 2026/1/24.
//

#include "Optimizer.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <stdexcept>

#include <fmt/format.h>

namespace cial::inter {
    using namespace vm;

    void optimizeDefaultChunk(vm::Chunk &chunk, const OptReg optReg) {
        OptimizerManager optimizerManager(chunk);
        optimizerManager.addOptimizer(std::make_unique<CopyPropagationOptimizer>());
        optimizerManager.addOptimizer(std::make_unique<IntegerConstantPropagationOptimizer>());
        optimizerManager.addOptimizer(std::make_unique<DeadLoadEliminationOptimizer>(optReg));
        optimizerManager.addOptimizer(std::make_unique<CompareTestFusionOptimizer>());
        const OptimizerStats optimizerStats = optimizerManager.applyOptimizations();
        if(OptimizerManager::profilingEnabled()) {
            fmt::print(stderr,
                       "[optimizer] TAC {} -> {}, removed={} by compaction, CFG {:.3f} us, compaction {:.3f} us",
                       optimizerStats.instructionsBefore, optimizerStats.instructionsAfter,
                       optimizerStats.instructionsRemovedByCompaction,
                       static_cast<double>(optimizerStats.cfgNanoseconds) / 1000.0,
                       static_cast<double>(optimizerStats.compactionNanoseconds) / 1000.0);
            for(const auto &pass : optimizerStats.passes)
                fmt::print(stderr, ", {} {:.3f} us (TAC {} -> {}, transforms={})", pass.name,
                           static_cast<double>(pass.nanoseconds) / 1000.0, pass.instructionsBefore,
                           pass.instructionsAfter, pass.transformations);
            fmt::print(stderr, "\n");
        }
    }

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

        // 纯拷贝指令判定：GLocal/DLocal 是 dst(op2) := src(op1) 的寄存器拷贝
        // （运行时 regs[r2]=regs[r1]，VM_CP 语义，无任何副作用）。
        bool pureCopyDestination(const Instruction &instruction, Register &destination, Register &source) {
            switch(instruction.opcode()) {
                case TacOpCode::GLocal:
                case TacOpCode::DLocal: {
                    if(instruction.getOp1().type() != Operand::Type::Register ||
                       instruction.getOp2().type() != Operand::Type::Register)
                        return false;
                    source = instruction.getOp1().value<Register>();
                    destination = instruction.getOp2().value<Register>();
                    return true;
                }
                default:
                    return false;
            }
        }


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

    size_t CopyPropagationOptimizer::optimize(Chunk &chunk, const Vec<BasicBlock> &blocks) {
        const bool hasCopyInstruction = std::ranges::any_of(chunk.getInstVec(), [](const auto &instruction) {
            const TacOpCode opcode = instruction->opcode();
            return opcode == TacOpCode::GLocal || opcode == TacOpCode::DLocal;
        });
        if(!hasCopyInstruction)
            return 0;

        const size_t registerCount = chunk.getRegCount();

        // 前驱统计：跨块仅沿唯一前驱前向边继承出口状态（同整数常量传播；
        // 回边前驱晚于自身出现自然跳过，多前驱/try handler 汇聚保守放弃）
        Vec<u32> predecessorCount(blocks.size(), 0);
        Vec<size_t> predecessorIndex(blocks.size(), 0);
        for(size_t i = 0; i < blocks.size(); ++i) {
            for(const size_t successor : blocks[i].successors) {
                if(predecessorCount[successor] == 0)
                    predecessorIndex[successor] = i;
                ++predecessorCount[successor];
            }
        }

        size_t replacedCount = 0;
        Vec<Vec<Register>> blockOutSource(blocks.size());
        Vec<Vec<u8>> blockOutHas(blocks.size());
        Vec<Register> copySource(registerCount);
        Vec<u8> hasCopy(registerCount, 0);

        // 链式解析：%dst := %src 且 %src 自身也是拷贝的 dst → 指向最终源。
        // 记录时源已完全解析，且 def 处理会清以 dst 为源的条目，循环必终止。
        auto resolve = [&](Register reg) {
            while(reg.index() < registerCount && hasCopy[reg.index()])
                reg = copySource[reg.index()];
            return reg;
        };
        auto processDef = [&](const Register def) {
            if(def.index() >= registerCount)
                return;
            hasCopy[def.index()] = 0;
            for(size_t r = 0; r < registerCount; ++r)
                if(hasCopy[r] && copySource[r].index() == def.index())
                    hasCopy[r] = 0;
        };
        auto replaceUse = [&](const Register use) {
            if(use.index() < registerCount && hasCopy[use.index()])
                return copySource[use.index()];
            return use;
        };
        auto replaceOperand = [&](auto operandSet, const Register current) {
            const Register replaced = replaceUse(current);
            if(replaced.index() != current.index()) {
                operandSet(replaced);
                ++replacedCount;
            }
        };

        for(size_t blockIndex = 0; blockIndex < blocks.size(); ++blockIndex) {
            const auto &block = blocks[blockIndex];
            std::fill(hasCopy.begin(), hasCopy.end(), 0);
            if(predecessorCount[blockIndex] == 1) {
                const size_t predecessor = predecessorIndex[blockIndex];
                if(predecessor < blockIndex && !blockOutHas[predecessor].empty()) {
                    hasCopy = blockOutHas[predecessor];
                    copySource = blockOutSource[predecessor];
                }
            }

            for(size_t pc = block.start; pc < block.end; ++pc) {
                Instruction &instruction = *chunk.inst(pc);
                const TacOpCode opcode = instruction.opcode();

                Register copyDst{}, copySrc{};
                const bool isCopy = pureCopyDestination(instruction, copyDst, copySrc);

                // 1) 使用点改写：仅限寄存器操作数均为纯读取的指令；
                //    拷贝指令只改写其 src（dst 是定义位，改写会污染写入目标）
                switch(opcode) {
                    case TacOpCode::GLocal:
                    case TacOpCode::DLocal:
                        if(copySrc.index() < registerCount)
                            replaceOperand([&](const Register v) { instruction.setOp1Val(v); }, copySrc);
                        break;
                    case TacOpCode::Test:
                    case TacOpCode::Ret:
                        if(instruction.getOp1().type() == Operand::Type::Register)
                            replaceOperand([&](const Register v) { instruction.setOp1Val(v); },
                                           instruction.getOp1().value<Register>());
                        break;
                    case TacOpCode::Add:
                    case TacOpCode::Sub:
                    case TacOpCode::Mul:
                    case TacOpCode::Div:
                    case TacOpCode::Idiv:
                    case TacOpCode::Mod:
                    case TacOpCode::BXor:
                    case TacOpCode::BOr:
                    case TacOpCode::BAnd:
                    case TacOpCode::BlShift:
                    case TacOpCode::BrShift:
                    case TacOpCode::BurShift:
                    case TacOpCode::EQ:
                    case TacOpCode::NEQ:
                    case TacOpCode::AbsEQ:
                    case TacOpCode::AbsNEQ:
                    case TacOpCode::LT:
                    case TacOpCode::LE:
                    case TacOpCode::GT:
                    case TacOpCode::GE:
                        if(instruction.getOp1().type() == Operand::Type::Register)
                            replaceOperand([&](const Register v) { instruction.setOp1Val(v); },
                                           instruction.getOp1().value<Register>());
                        if(instruction.getOp2().type() == Operand::Type::Register)
                            replaceOperand([&](const Register v) { instruction.setOp2Val(v); },
                                           instruction.getOp2().value<Register>());
                        break;
                    case TacOpCode::CompareTest:
                        if(instruction.getOp1().type() == Operand::Type::Register)
                            replaceOperand([&](const Register v) { instruction.setOp1Val(v); },
                                           instruction.getOp1().value<Register>());
                        if(instruction.getOp2().type() == Operand::Type::Register)
                            replaceOperand([&](const Register v) { instruction.setOp2Val(v); },
                                           instruction.getOp2().value<Register>());
                        break;
                    case TacOpCode::CallV: {
                        auto &call = static_cast<CallV &>(instruction);
                        const Register func = replaceUse(call.func);
                        if(func.index() != call.func.index()) {
                            call.func = func;
                            ++replacedCount;
                        }
                        for(auto &arg : call.args) {
                            const Register replaced = replaceUse(arg);
                            if(replaced.index() != arg.index()) {
                                arg = replaced;
                                ++replacedCount;
                            }
                        }
                        break;
                    }
                    case TacOpCode::CallMethodV: {
                        auto &call = static_cast<CallMethodV &>(instruction);
                        const Register func = replaceUse(call.func);
                        if(func.index() != call.func.index()) {
                            call.func = func;
                            ++replacedCount;
                        }
                        const Register receiver = replaceUse(call.receiver);
                        if(receiver.index() != call.receiver.index()) {
                            call.receiver = receiver;
                            ++replacedCount;
                        }
                        for(auto &arg : call.args) {
                            const Register replaced = replaceUse(arg);
                            if(replaced.index() != arg.index()) {
                                arg = replaced;
                                ++replacedCount;
                            }
                        }
                        break;
                    }
                    case TacOpCode::GMethodC: {
                        auto &method = static_cast<GMethodC &>(instruction);
                        method.receiver = replaceUse(method.receiver);
                        if(method.getOp1().type() == Operand::Type::Register)
                            replaceOperand([&](const Register v) { method.setOp1Val(v); },
                                           method.getOp1().value<Register>());
                        break;
                    }
                    default:
                        break;
                }

                // 2) 定义处理：拷贝指令 def 精确为 dst（src 是读取，不得失效链）；
                //    其余指令保守——提及即重定义
                if(isCopy) {
                    processDef(copyDst);
                    if(copyDst.index() < registerCount) {
                        const Register source = resolve(copySrc);
                        if(source.index() != copyDst.index()) {
                            copySource[copyDst.index()] = source;
                            hasCopy[copyDst.index()] = 1;
                        }
                    }
                } else {
                    forEachRegisterOf(instruction, [&](const Register reg) { processDef(reg); });
                }
            }
            blockOutHas[blockIndex] = hasCopy;
            blockOutSource[blockIndex] = copySource;
        }
        return replacedCount;
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

        const size_t registerCount = chunk.getRegCount();

        // 仅合并已遍历的全部前驱；值相同才保留。回边尚无出口状态，整体放弃，
        // 避免把入口常量误用于后续迭代。异常前驱的中间状态不能用正常出口代替。
        Vec<Vec<size_t>> predecessors(blocks.size());
        for(size_t i = 0; i < blocks.size(); ++i)
            for(const size_t successor : blocks[i].successors)
                predecessors[successor].push_back(i);

        size_t foldedCount = 0;
        Vec<Vec<Integer>> blockOutConstants(blocks.size());
        Vec<Vec<u8>> blockOutKnown(blocks.size());
        Vec<Integer> constants(registerCount);
        Vec<u8> known(registerCount, 0);
        for(size_t blockIndex = 0; blockIndex < blocks.size(); ++blockIndex) {
            const auto &block = blocks[blockIndex];
            std::fill(known.begin(), known.end(), 0);
            bool first = true;
            for(const size_t predecessor : predecessors[blockIndex]) {
                if(predecessor >= blockIndex || protectedByTry(chunk, blocks[predecessor])) {
                    std::fill(known.begin(), known.end(), 0);
                    break;
                }
                if(first) {
                    known = blockOutKnown[predecessor];
                    constants = blockOutConstants[predecessor];
                    first = false;
                } else {
                    for(size_t reg = 0; reg < registerCount; ++reg)
                        known[reg] = known[reg] && blockOutKnown[predecessor][reg] &&
                            constants[reg] == blockOutConstants[predecessor][reg];
                }
            }
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
                // 出口状态取"正常完成"路径：块内中途抛异常时控制转 handler，
                // 唯一前驱的继承语义不受影响。
                forEachRegisterOf(instruction, [&](const Register reg) {
                    if(reg.index() < known.size())
                        known[reg.index()] = 0;
                });
            }
            blockOutKnown[blockIndex] = known;
            blockOutConstants[blockIndex] = constants;
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
                Register destination{};
                if(tryGetImmediateLoadDestination(instruction, destination)) {
                    setRegister(blockDefs[i], destination.index());
                    continue;
                }
                Register copyDst{}, copySrc{};
                if(pureCopyDestination(instruction, copyDst, copySrc)) {
                    // 拷贝的 src 仍计入 use：活跃性不得欠记，否则跨块前驱的
                    // 定义会被误删（死拷贝自身的清除在反向遍历中处理）
                    setRegister(blockDefs[i], copyDst.index());
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
                Register destination{};
                if(tryGetImmediateLoadDestination(instruction, destination)) {
                    if(!isSet(live, destination.index())) {
                        chunk.repl<NOP>(pc);
                        ++removedCount;
                    } else {
                        clearRegister(live, destination.index());
                    }
                    continue;
                }
                Register copyDst{}, copySrc{};
                if(pureCopyDestination(instruction, copyDst, copySrc)) {
                    // 纯拷贝无副作用：dst 死即可删；dst 活则删除 dst 的活跃并
                    // 让 src（家园寄存器）接续活跃
                    if(!isSet(live, copyDst.index())) {
                        chunk.repl<NOP>(pc);
                        ++removedCount;
                    } else {
                        clearRegister(live, copyDst.index());
                        setRegister(live, copySrc.index());
                    }
                    continue;
                }
                forEachRegisterOf(instruction, [&](const Register reg) { setRegister(live, reg.index()); });
            }
        }
        return removedCount;
    }

} // namespace cial::inter
