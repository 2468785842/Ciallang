//
// Created by LiDong on 2026/10/29.
//
// SSA 按需构造实现：
//   1. 精确 def/use 提取（95 条 TAC 全登记；未知 opcode 与隐式读帧实参的
//      CallOmit/CallExpand/CollapseArgs 抛错，不猜测）
//   2. 支配树（Cooper-Harvey-Kennedy 迭代求直接支配者）
//   3. 支配边界（Cooper 沿 idom 上行法；入口自环按形式定义补齐）
//   4. 块级活跃性 + pruned phi 放置（判据 liveIn：phi 位于块入口）
//   5. 支配树 DFS 重命名（Cytron），回填到达定义与 def-use 链；
//      多定义指令（GMethodC）以 slot 区分身份
// 异常边保守策略（与 try 语义对齐，见 README/AGENTS 坑位 6）：
//   try 保护块出口视为所有寄存器活跃；异常着陆块是 SSA 边界——到达定义
//   在此重置并引入每块独立的 Catch 未知值、块内不放 phi（块内中途抛错时
//   "块末定义"不保证成立）。
#include "SSA.hpp"

#include <algorithm>
#include <stdexcept>

#include <fmt/format.h>

namespace cial::inter {
    namespace {
        const char *opcodeName(const TacOpCode opcode) {
            switch(opcode) {
#define CASE_NAME(OP) \
    case TacOpCode::OP: return #OP;
                CIAL_TAC_OPCODE_ENUMS(CASE_NAME)
#undef CASE_NAME
            }
            return "Unknown";
        }

        Opt<Register> asRegister(const Operand &operand) {
            if(operand.type() != Operand::Type::Register)
                return {};
            return operand.value<Register>();
        }

        u64 refKey(const SsaRef &ref) {
            return (u64(ref.kind) << 62) | (u64(ref.index) << 16) | ref.slot;
        }

        std::string refText(const SsaRef &ref) {
            switch(ref.kind) {
                case SsaRef::Kind::None:
                    return "None";
                case SsaRef::Kind::Inst:
                    return ref.slot == 0 ? fmt::format("Inst:{}", ref.index)
                        : fmt::format("Inst:{}#{}", ref.index, ref.slot);
                case SsaRef::Kind::Phi:
                    return fmt::format("Phi:{}", ref.index);
                case SsaRef::Kind::Entry:
                    return fmt::format("Entry:%{}", ref.index);
                case SsaRef::Kind::Catch:
                    return fmt::format("Catch:B{}", ref.index);
            }
            return "?";
        }
    } // namespace

    SsaBuilder::Access SsaBuilder::extractAccess(const Instruction &inst) {
        Access access;
        const auto use1 = [&] {
            if(auto reg = asRegister(inst.getOp1()))
                access.uses.push_back(*reg);
        };
        const auto use2 = [&] {
            if(auto reg = asRegister(inst.getOp2()))
                access.uses.push_back(*reg);
        };
        const auto use3 = [&] {
            if(auto reg = asRegister(inst.getOp3()))
                access.uses.push_back(*reg);
        };

        switch(inst.opcode()) {
            // ── 无寄存器访问 ──
            case TacOpCode::NOP:
            case TacOpCode::Debugger:
            case TacOpCode::PopN:
            case TacOpCode::WithPop:
            case TacOpCode::Jmp:
            case TacOpCode::JmpE:
            case TacOpCode::JmpNE:
                break;

            // ── 单使用 ──
            case TacOpCode::Push:
            case TacOpCode::Test:
            case TacOpCode::Inv:
            case TacOpCode::Throw:
            case TacOpCode::Ret:
            case TacOpCode::WithPush:
                use1();
                break;

            // ── 定义 = op1 ──
            case TacOpCode::Global:
            case TacOpCode::Super:
            case TacOpCode::This:
            case TacOpCode::NewArray:
            case TacOpCode::NewDict:
            case TacOpCode::GetWith:
            case TacOpCode::Load:
            case TacOpCode::LoadImm:
            case TacOpCode::NewRegExp:
                if(auto reg = asRegister(inst.getOp1()))
                    access.defs.push_back(*reg);
                break;

            // ── 定义 = op2（src = op1）──
            case TacOpCode::ToInt:
            case TacOpCode::ToReal:
            case TacOpCode::ToString:
            case TacOpCode::TypeOf:
            case TacOpCode::Asc:
            case TacOpCode::Chr:
            case TacOpCode::LNot:
            case TacOpCode::ChgSign:
            case TacOpCode::ChkInv:
            case TacOpCode::EvalStr:
            case TacOpCode::BNot:
            case TacOpCode::ToNumber:
            case TacOpCode::ToOctet:
                use1();
                if(auto reg = asRegister(inst.getOp2()))
                    access.defs.push_back(*reg);
                break;

            // ── 定义 = op1（src = op2）──
            case TacOpCode::ChkIns:
                // VMState:dst(op1) 既是被测值又是 bool 输出槽——读-改-写
                use1();
                use2();
                if(auto reg = asRegister(inst.getOp1()))
                    access.defs.push_back(*reg);
                break;

            // ── 只读两个寄存器：改堆对象（fn->thisObj），不定义寄存器 ──
            case TacOpCode::ChgThis:
                use1();
                use2();
                break;

            // ── 守卫形态：dst = op2（src = op1）──
            case TacOpCode::TestVoid:
                use1();
                if(auto reg = asRegister(inst.getOp2()))
                    access.defs.push_back(*reg);
                break;

            // ── 全局/this 槽写：只读 src，不定义寄存器 ──
            case TacOpCode::DGlobal:
            case TacOpCode::DThis:
                use2();
                break;

            // ── 全局/this/裸词槽读：定义 = op2 ──
            case TacOpCode::GGlobal:
            case TacOpCode::GThis:
            case TacOpCode::GRefRaw:
                if(auto reg = asRegister(inst.getOp2()))
                    access.defs.push_back(*reg);
                break;

            // ── 纯寄存器拷贝 dst(op2) := src(op1)（VM_CP 语义，同 Optimizer）──
            case TacOpCode::GLocal:
            case TacOpCode::DLocal:
                use1();
                if(auto reg = asRegister(inst.getOp2()))
                    access.defs.push_back(*reg);
                break;

            // ── 三槽二元运算 dst = op3 ──
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
            case TacOpCode::LAnd:
            case TacOpCode::LOr:
                use1();
                use2();
                if(auto reg = asRegister(inst.getOp3()))
                    access.defs.push_back(*reg);
                break;

            // ── 调用：dst = op1，函数/成员 = op2 ──
            case TacOpCode::Call:
                if(auto reg = asRegister(inst.getOp1()))
                    access.defs.push_back(*reg);
                use2();
                break;

            // ── 属性族（寄存器成员名）：obj = op1，member = op2 ──
            case TacOpCode::GProp:
            case TacOpCode::DelProp:
            case TacOpCode::GPropRaw:
                use1();
                use2();
                if(auto reg = asRegister(inst.getOp3()))
                    access.defs.push_back(*reg);
                break;
            case TacOpCode::DProp:
            case TacOpCode::DPropRaw:
                use1();
                use2();
                use3();
                break;

            // ── 属性族（Atom 直引成员名）：obj = op1 ──
            case TacOpCode::GPropC:
            case TacOpCode::GPropRawC:
            case TacOpCode::DelPropC:
                use1();
                if(auto reg = asRegister(inst.getOp3()))
                    access.defs.push_back(*reg);
                break;
            case TacOpCode::DPropC:
            case TacOpCode::DPropRawC:
                use1();
                use3();
                break;

            // ── 指针解引用 ──
            case TacOpCode::DerefGet:
                use1();
                if(auto reg = asRegister(inst.getOp2()))
                    access.defs.push_back(*reg);
                break;
            case TacOpCode::DerefSet:
                use1();
                use2();
                break;

            // ── 容器下标 ──
            case TacOpCode::GetIndex:
                use1();
                use2();
                if(auto reg = asRegister(inst.getOp3()))
                    access.defs.push_back(*reg);
                break;
            case TacOpCode::SetIndex:
                use1();
                use2();
                use3();
                break;

            // ── 直写 _zf 的比较：只读，不定义寄存器 ──
            case TacOpCode::CmpEQ:
            case TacOpCode::CmpDEQ:
            case TacOpCode::CmpLT:
            case TacOpCode::CmpGT:
                use1();
                use2();
                break;

            // ── 读-改-写：src 既是使用又是定义 ──
            case TacOpCode::Inc:
            case TacOpCode::Dec:
                use1();
                if(auto reg = asRegister(inst.getOp1()))
                    access.defs.push_back(*reg);
                break;

            // ── 手写子类 ──
            case TacOpCode::CompareTest:
                use1();
                use2();
                if(auto reg = asRegister(inst.getOp3()))
                    access.defs.push_back(*reg);
                break;
            case TacOpCode::CallV: {
                const auto &call = static_cast<const CallV &>(inst);
                access.defs.push_back(call.dst);
                access.uses.push_back(call.func);
                for(const auto reg : call.args)
                    access.uses.push_back(reg);
                break;
            }
            case TacOpCode::CallMethodV: {
                const auto &call = static_cast<const CallMethodV &>(inst);
                access.defs.push_back(call.dst);
                access.uses.push_back(call.receiver);
                access.uses.push_back(call.func);
                for(const auto reg : call.args)
                    access.uses.push_back(reg);
                break;
            }
            case TacOpCode::GMethodC: {
                const auto &method = static_cast<const GMethodC &>(inst);
                use1(); // obj
                if(auto reg = asRegister(inst.getOp3()))
                    access.defs.push_back(*reg); // dst（方法值）
                access.defs.push_back(method.receiver); // receiver（原生方法绑定为接收者）
                break;
            }
            case TacOpCode::CallOmit:
            case TacOpCode::CallExpand:
            case TacOpCode::CollapseArgs:
                // 隐式读取帧实参（%0..%actualArgc-1 / unnamed 窗口），静态无法
                // 给出精确使用集——明确拒绝而非给出貌似精确的 def-use（#25）
                throw std::runtime_error(fmt::format(
                    "SSA: opcode {} 隐式读取帧实参，受限 SSA 拒绝构造",
                    opcodeName(inst.opcode())));
            default:
                throw std::runtime_error(
                    fmt::format("SSA: opcode {} 未登记 def/use，拒绝猜测", opcodeName(inst.opcode())));
        }
        if(!access.defs.empty())
            access.def = access.defs.front();
        return access;
    }

    SsaForm SsaBuilder::build(Vec<Vec<size_t>> successors, const size_t entryBlock,
                              Vec<SsaBlockFacts> facts, const size_t variableCount,
                              const Vec<bool> &preserveAllLiveOut, const Vec<bool> &exceptionEntries) {
        const size_t blockCount = successors.size();
        if(blockCount == 0)
            throw std::runtime_error("SSA: empty control-flow graph");
        if(entryBlock >= blockCount)
            throw std::runtime_error("SSA: entry block out of range");
        if(facts.size() != blockCount)
            throw std::runtime_error("SSA: block facts size mismatch");

        SsaForm form;
        form._successors = std::move(successors);
        form._entry = entryBlock;
        form._variableCount = variableCount;

        for(auto &list : form._successors) {
            Vec<size_t> deduped;
            for(const size_t successor : list) {
                if(successor >= blockCount)
                    throw std::runtime_error("SSA: successor block out of range");
                if(std::ranges::find(deduped, successor) == deduped.end())
                    deduped.push_back(successor);
            }
            list = std::move(deduped);
        }

        form._predecessors.assign(blockCount, {});
        for(size_t i = 0; i < blockCount; ++i)
            for(const size_t successor : form._successors[i])
                form._predecessors[successor].push_back(i);

        // 事实越界校验：所有变量编号必须落在 variableCount 内
        const auto validateVar = [&](const u16 var) {
            if(var >= variableCount)
                throw std::runtime_error(
                    fmt::format("SSA: variable {} exceeds declared variableCount {}", var, variableCount));
        };
        for(const auto &blockFacts : facts) {
            for(const u16 var : blockFacts.defined)
                validateVar(var);
            for(const u16 var : blockFacts.upwardUses)
                validateVar(var);
            for(const auto &inst : blockFacts.insts) {
                for(const u16 var : inst.defs)
                    validateVar(var);
                for(const u16 var : inst.uses)
                    validateVar(var);
            }
        }

        // 入口可达性
        Vec<bool> reachable(blockCount, false);
        Vec<size_t> reachStack{entryBlock};
        reachable[entryBlock] = true;
        while(!reachStack.empty()) {
            const size_t current = reachStack.back();
            reachStack.pop_back();
            for(const size_t successor : form._successors[current]) {
                if(!reachable[successor]) {
                    reachable[successor] = true;
                    reachStack.push_back(successor);
                }
            }
        }

        // 后序 → 逆后序（支配算法的遍历基准）
        Vec<size_t> postorder;
        postorder.reserve(blockCount);
        Vec<std::pair<size_t, size_t>> dfs;
        Vec<u8> dfsState(blockCount, 0); // 0 未访问 1 栈内 2 完成
        dfs.push_back({entryBlock, 0});
        dfsState[entryBlock] = 1;
        while(!dfs.empty()) {
            const size_t block = dfs.back().first;
            if(dfs.back().second < form._successors[block].size()) {
                const size_t successor = form._successors[block][dfs.back().second++];
                if(dfsState[successor] == 0) {
                    dfsState[successor] = 1;
                    dfs.push_back({successor, 0});
                }
            } else {
                dfsState[block] = 2;
                postorder.push_back(block);
                dfs.pop_back();
            }
        }

        const Vec<size_t> rpo(postorder.rbegin(), postorder.rend());
        Vec<size_t> postIndex(blockCount, SsaForm::InvalidBlock);
        for(size_t i = 0; i < rpo.size(); ++i)
            postIndex[rpo[i]] = i;

        // 直接支配者：Cooper-Harvey-Kennedy 迭代法（入口 idom = 自身）
        auto intersect = [&](size_t a, size_t b) {
            while(a != b) {
                while(postIndex[a] > postIndex[b])
                    a = form._idom[a];
                while(postIndex[b] > postIndex[a])
                    b = form._idom[b];
            }
            return a;
        };
        form._idom.assign(blockCount, SsaForm::InvalidBlock);
        form._idom[entryBlock] = entryBlock;
        bool idomChanged = true;
        while(idomChanged) {
            idomChanged = false;
            for(size_t i = 1; i < rpo.size(); ++i) {
                const size_t block = rpo[i];
                size_t newIdom = SsaForm::InvalidBlock;
                for(const size_t predecessor : form._predecessors[block]) {
                    if(!reachable[predecessor] || form._idom[predecessor] == SsaForm::InvalidBlock)
                        continue;
                    newIdom = newIdom == SsaForm::InvalidBlock ? predecessor
                        : intersect(predecessor, newIdom);
                }
                if(newIdom != SsaForm::InvalidBlock && form._idom[block] != newIdom) {
                    form._idom[block] = newIdom;
                    idomChanged = true;
                }
            }
        }

        form._domTree.assign(blockCount, {});
        for(size_t i = 0; i < blockCount; ++i) {
            if(reachable[i] && i != entryBlock && form._idom[i] != SsaForm::InvalidBlock)
                form._domTree[form._idom[i]].push_back(i);
        }

        // 支配边界：对每个有前驱的块，沿前驱的 idom 链上行至直接支配者
        form._domFrontiers.assign(blockCount, {});
        for(size_t block = 0; block < blockCount; ++block) {
            if(!reachable[block] || form._predecessors[block].empty())
                continue;
            for(const size_t predecessor : form._predecessors[block]) {
                if(!reachable[predecessor])
                    continue;
                size_t runner = predecessor;
                while(runner != form._idom[block]) {
                    auto &frontier = form._domFrontiers[runner];
                    if(std::ranges::find(frontier, block) == frontier.end())
                        frontier.push_back(block);
                    if(runner == form._idom[runner])
                        break; // 入口块自支配，链到此为止
                    runner = form._idom[runner];
                }
            }
        }
        // 入口自环：形式定义下 entry ∈ DF(entry)（上行法因 entry 自支配而跳过）
        if(std::ranges::find(form._successors[entryBlock], entryBlock) != form._successors[entryBlock].end() &&
            std::ranges::find(form._domFrontiers[entryBlock], entryBlock) == form._domFrontiers[entryBlock].end())
            form._domFrontiers[entryBlock].push_back(entryBlock);
        for(auto &frontier : form._domFrontiers)
            std::ranges::sort(frontier);

        // 指令级静态信息（defs 与所属块；使用点由重命名回填）
        size_t maxPc = 0;
        for(const auto &blockFacts : facts)
            for(const auto &inst : blockFacts.insts)
                maxPc = std::ranges::max(maxPc, static_cast<size_t>(inst.pc));
        form._insts.assign(maxPc + 1, InstSsa{});
        for(size_t block = 0; block < blockCount; ++block) {
            for(const auto &inst : facts[block].insts) {
                form._insts[inst.pc].block = block;
                for(const u16 var : inst.defs)
                    form._insts[inst.pc].defs.push_back(Register{var});
            }
        }

        // 块级活跃性：liveOut = ∪ liveIn[succ]；try 保守块出口全活跃
        const auto flagOf = [&](const size_t block, const Vec<bool> &flags) {
            return block < flags.size() ? flags[block] : false;
        };
        const size_t wordCount = (variableCount + 63) / 64;
        using RegisterSet = Vec<u64>;
        const auto setBit = [](RegisterSet &set, const u16 var) { set[var / 64] |= u64{1} << (var % 64); };
        const auto testBit = [](const RegisterSet &set, const u16 var) {
            return (set[var / 64] & (u64{1} << (var % 64))) != 0;
        };
        Vec<RegisterSet> useBits(blockCount, RegisterSet(wordCount, 0));
        Vec<RegisterSet> defBits(blockCount, RegisterSet(wordCount, 0));
        for(size_t block = 0; block < blockCount; ++block) {
            for(const u16 var : facts[block].defined)
                setBit(defBits[block], var);
            for(const u16 var : facts[block].upwardUses)
                setBit(useBits[block], var);
        }
        Vec<RegisterSet> liveIn(blockCount, RegisterSet(wordCount, 0));
        Vec<RegisterSet> liveOut(blockCount, RegisterSet(wordCount, 0));
        bool liveChanged = true;
        while(liveChanged) {
            liveChanged = false;
            for(size_t reverse = blockCount; reverse > 0; --reverse) {
                const size_t block = reverse - 1;
                if(!reachable[block])
                    continue;
                RegisterSet out(wordCount, 0);
                for(const size_t successor : form._successors[block])
                    for(size_t word = 0; word < wordCount; ++word)
                        out[word] |= liveIn[successor][word];
                if(flagOf(block, preserveAllLiveOut)) {
                    for(size_t word = 0; word < wordCount; ++word)
                        out[word] = ~u64{0};
                    if(variableCount % 64 != 0 && wordCount > 0)
                        out.back() = (u64{1} << (variableCount % 64)) - 1;
                }
                RegisterSet in(wordCount, 0);
                for(size_t word = 0; word < wordCount; ++word)
                    in[word] = useBits[block][word] | (out[word] & ~defBits[block][word]);
                if(in != liveIn[block] || out != liveOut[block]) {
                    liveIn[block] = std::move(in);
                    liveOut[block] = std::move(out);
                    liveChanged = true;
                }
            }
        }

        // pruned phi 放置：定义块的迭代支配边界，且变量在汇合块入口活跃。
        // 判据是 liveIn 而非 liveOut：phi 位于块入口，汇合块内 def 之前的
        // 使用读的就是这个 phi（此时变量未必 live-out）
        form._phiIndices.assign(blockCount, {});
        Vec<size_t> defBlocksWorklist;
        Vec<u8> inWorklist(blockCount, 0);
        for(size_t var = 0; var < variableCount; ++var) {
            Vec<size_t> defBlocks;
            for(size_t block = 0; block < blockCount; ++block) {
                if(reachable[block] && testBit(defBits[block], var))
                    defBlocks.push_back(block);
            }
            if(defBlocks.empty())
                continue;

            Vec<u8> phiPlaced(blockCount, 0);
            for(const size_t block : defBlocks) {
                inWorklist[block] = 1;
                defBlocksWorklist.push_back(block);
            }
            while(!defBlocksWorklist.empty()) {
                const size_t block = defBlocksWorklist.back();
                defBlocksWorklist.pop_back();
                inWorklist[block] = 0;
                for(const size_t join : form._domFrontiers[block]) {
                    if(phiPlaced[join] || flagOf(join, exceptionEntries))
                        continue;
                    if(!testBit(liveIn[join], var))
                        continue;
                    SsaPhi phi;
                    phi.block = join;
                    phi.reg = Register{static_cast<u16>(var)};
                    phi.inputs.assign(form._predecessors[join].size(), SsaRef{});
                    form._phiIndices[join].push_back(static_cast<u32>(form._phis.size()));
                    form._phis.push_back(std::move(phi));
                    phiPlaced[join] = 1;
                    // phi 是新定义，继续沿支配边界扩散
                    if(!inWorklist[join]) {
                        inWorklist[join] = 1;
                        defBlocksWorklist.push_back(join);
                    }
                }
            }
        }

        // 重命名：支配树 DFS（迭代），使用点解析到栈顶定义
        form._exceptionEntry.assign(blockCount, false);
        for(size_t block = 0; block < blockCount; ++block)
            form._exceptionEntry[block] = flagOf(block, exceptionEntries);

        Vec<Vec<SsaRef>> stacks(variableCount);
        struct RenameFrame {
            size_t block{};
            size_t childCursor{};
            size_t catchBlock{SsaForm::InvalidBlock}; // 最近异常边界祖先
            Vec<std::pair<u16, size_t>> savedTops;    // (变量, 进入时栈深)
            bool hasSnapshot{};                       // 异常边界帧：整栈快照
            Vec<Vec<SsaRef>> snapshot;
        };
        Vec<RenameFrame> frames;

        const auto recordUse = [&](const SsaRef &def, const SsaUse &use) {
            form._usesOfDef[refKey(def)].push_back(use);
        };

        // 未解析使用的身份：catch 语境为该着陆块的 Catch 未知值，否则入口参数
        const auto unresolvedRef = [&](const size_t catchBlock, const u16 var) {
            return catchBlock == SsaForm::InvalidBlock
                ? SsaRef{SsaRef::Kind::Entry, var}
                : SsaRef{SsaRef::Kind::Catch, static_cast<u32>(catchBlock)};
        };

        // 进入块：phi 定义、使用点解析、后继 phi 输入回填
        const auto enterBlock = [&](const size_t block, const size_t parentCatchBlock) {
            RenameFrame frame{block, 0, parentCatchBlock, {}, false, {}};
            if(form._exceptionEntry[block]) {
                // 异常着陆块是 SSA 边界：整栈快照后清空（可发生在块内任意点，
                // 按长度恢复会丢失原到达定义——必须保存完整内容）
                frame.hasSnapshot = true;
                frame.snapshot = stacks;
                frame.catchBlock = block;
                for(auto &stack : stacks)
                    stack.clear();
            }
            // 块入口 phi 定义
            for(const u32 phiIndex : form._phiIndices[block]) {
                const u16 var = form._phis[phiIndex].reg.index();
                frame.savedTops.push_back({var, stacks[var].size()});
                stacks[var].push_back(SsaRef{SsaRef::Kind::Phi, phiIndex});
            }
            // 块内指令：先解析使用，再按槽位压定义
            for(const auto &inst : facts[block].insts) {
                auto &instSsa = form._insts[inst.pc];
                instSsa.operandDefs.reserve(inst.uses.size());
                for(size_t j = 0; j < inst.uses.size(); ++j) {
                    const u16 var = inst.uses[j];
                    const SsaRef def = stacks[var].empty()
                        ? unresolvedRef(frame.catchBlock, var)
                        : stacks[var].back();
                    instSsa.operandDefs.push_back(def);
                    recordUse(def, SsaUse{SsaUse::Kind::Operand, static_cast<u32>(inst.pc),
                                          static_cast<u32>(j)});
                }
                for(size_t slot = 0; slot < inst.defs.size(); ++slot) {
                    const u16 var = inst.defs[slot];
                    frame.savedTops.push_back({var, stacks[var].size()});
                    stacks[var].push_back(SsaRef{SsaRef::Kind::Inst, inst.pc, static_cast<u16>(slot)});
                }
            }
            // 后继块入口 phi 的本边输入
            for(const size_t successor : form._successors[block]) {
                if(form._phiIndices[successor].empty())
                    continue;
                const auto &preds = form._predecessors[successor];
                const size_t edgeIndex =
                    static_cast<size_t>(std::ranges::find(preds, block) - preds.begin());
                for(const u32 phiIndex : form._phiIndices[successor]) {
                    SsaPhi &phi = form._phis[phiIndex];
                    const u16 var = phi.reg.index();
                    if(stacks[var].empty()) {
                        // catch 语境下路径值是着陆块引入的未知值，可参与下游合流
                        phi.inputs[edgeIndex] =
                            frame.catchBlock == SsaForm::InvalidBlock
                                ? SsaRef{}
                                : SsaRef{SsaRef::Kind::Catch, static_cast<u32>(frame.catchBlock)};
                    } else {
                        phi.inputs[edgeIndex] = stacks[var].back();
                    }
                    if(phi.inputs[edgeIndex].valid())
                        recordUse(phi.inputs[edgeIndex],
                                  SsaUse{SsaUse::Kind::PhiInput, phiIndex,
                                         static_cast<u32>(edgeIndex)});
                }
            }
            return frame;
        };

        frames.push_back(enterBlock(entryBlock, SsaForm::InvalidBlock));

        while(!frames.empty()) {
            auto &frame = frames.back();
            if(frame.childCursor < form._domTree[frame.block].size()) {
                const size_t child = form._domTree[frame.block][frame.childCursor++];
                frames.push_back(enterBlock(child, frame.catchBlock));
            } else {
                // 离开子树：异常边界帧整栈还原，普通帧恢复进入时栈深
                if(frame.hasSnapshot)
                    stacks = std::move(frame.snapshot);
                else
                    for(auto it = frame.savedTops.rbegin(); it != frame.savedTops.rend(); ++it)
                        stacks[it->first].resize(it->second);
                frames.pop_back();
            }
        }

        return form;
    }

    const Vec<SsaUse> &SsaForm::usesOfDef(const SsaRef &def) const {
        static const Vec<SsaUse> empty;
        const auto it = _usesOfDef.find(refKey(def));
        return it == _usesOfDef.end() ? empty : it->second;
    }

    SsaForm SsaBuilder::buildChunkSsa(vm::Chunk &chunk, const Vec<BasicBlock> &blocks) {
        const auto &code = chunk.getInstVec();
        const size_t codeSize = code.size();
        if(codeSize == 0 || blocks.empty())
            return SsaForm{};

        // 块必须连续覆盖全部 TAC（buildCFG 的输出形态）
        size_t expected = 0;
        for(const auto &block : blocks) {
            if(block.start != expected)
                throw std::runtime_error("SSA: basic blocks are not contiguous");
            expected = block.end;
        }
        if(expected != codeSize)
            throw std::runtime_error("SSA: basic blocks do not cover all instructions");

        Vec<u32> blockAtPc(codeSize);
        for(size_t i = 0; i < blocks.size(); ++i)
            for(size_t pc = blocks[i].start; pc < blocks[i].end; ++pc)
                blockAtPc[pc] = static_cast<u32>(i);

        // try 区间：保护块出口全活跃；着陆块作为 SSA 边界
        Vec<bool> preserveAll(blocks.size(), false);
        Vec<bool> exceptionEntry(blocks.size(), false);
        for(const auto &handler : chunk.getThrowHandlers()) {
            if(handler.tryStart > handler.tryEnd)
                throw std::runtime_error("SSA: try handler range is inverted");
            for(size_t i = 0; i < blocks.size(); ++i) {
                if(blocks[i].start < handler.tryEnd && handler.tryStart < blocks[i].end)
                    preserveAll[i] = true;
            }
            if(handler.tryEnd < codeSize)
                exceptionEntry[blockAtPc[handler.tryEnd]] = true;
        }

        // 先全量提取访问，确定变量全集
        Vec<Access> accesses(codeSize);
        u16 maxReg = 0;
        bool anyRegister = false;
        for(size_t pc = 0; pc < codeSize; ++pc) {
            accesses[pc] = extractAccess(*code[pc]);
            const auto track = [&](const Register reg) {
                anyRegister = true;
                maxReg = std::ranges::max(maxReg, reg.index());
            };
            for(const Register reg : accesses[pc].uses)
                track(reg);
            for(const Register reg : accesses[pc].defs)
                track(reg);
        }
        size_t variableCount = chunk.getRegCount();
        if(anyRegister)
            variableCount = std::ranges::max(variableCount, static_cast<size_t>(maxReg) + 1);

        Vec<SsaBlockFacts> facts(blocks.size());
        for(size_t i = 0; i < blocks.size(); ++i) {
            Vec<u8> defSeen(variableCount, 0);
            Vec<u8> upSeen(variableCount, 0);
            Vec<u8> localDef(variableCount, 0);
            for(size_t pc = blocks[i].start; pc < blocks[i].end; ++pc) {
                const Access &access = accesses[pc];
                SsaBlockFacts::Inst inst;
                inst.pc = static_cast<u32>(pc);
                for(const Register reg : access.uses) {
                    inst.uses.push_back(reg.index());
                    if(!localDef[reg.index()] && !upSeen[reg.index()]) {
                        upSeen[reg.index()] = 1;
                        facts[i].upwardUses.push_back(reg.index());
                    }
                }
                for(const Register reg : access.defs) {
                    inst.defs.push_back(reg.index());
                    if(!defSeen[reg.index()]) {
                        defSeen[reg.index()] = 1;
                        facts[i].defined.push_back(reg.index());
                    }
                    localDef[reg.index()] = 1;
                }
                facts[i].insts.push_back(std::move(inst));
            }
        }

        Vec<Vec<size_t>> successors(blocks.size());
        for(size_t i = 0; i < blocks.size(); ++i)
            successors[i] = blocks[i].successors;

        return build(std::move(successors), 0, std::move(facts), variableCount, preserveAll,
                     exceptionEntry);
    }

    std::string SsaBuilder::dumpSsaText(vm::Chunk &chunk, const SsaForm &form) {
        if(form.blockCount() == 0)
            return "== SSA (empty)\n";

        auto indexList = [](const Vec<size_t> &list) {
            std::string text = "[";
            for(size_t i = 0; i < list.size(); ++i) {
                if(i > 0)
                    text += ",";
                text += fmt::format("B{}", list[i]);
            }
            return text + "]";
        };

        std::string out = fmt::format("== SSA blocks={} entry=B{} vars={}\n", form.blockCount(),
                                      form.entryBlock(), form.variableCount());
        const auto &code = chunk.getInstVec();
        for(size_t block = 0; block < form.blockCount(); ++block) {
            const size_t idom = form.idomOf(block);
            out += fmt::format("B{} idom={} preds={} succ={}{}\n", block,
                               idom == SsaForm::InvalidBlock ? std::string("-")
                                                             : fmt::format("B{}", idom),
                               indexList(form.predecessorsOf(block)),
                               indexList(form.successorsOf(block)),
                               form.isExceptionEntry(block) ? " exc-entry" : "");
            for(const u32 phiIndex : form.phiIndicesAt(block)) {
                const SsaPhi &phi = form.phis()[phiIndex];
                out += fmt::format("  phi %{} <- [", phi.reg.index());
                for(size_t i = 0; i < phi.inputs.size(); ++i) {
                    if(i > 0)
                        out += ", ";
                    out += fmt::format("B{}={}", form.predecessorsOf(block)[i],
                                       refText(phi.inputs[i]));
                }
                out += "]\n";
            }
            for(size_t pc = 0; pc < code.size() && pc < form.insts().size(); ++pc) {
                const InstSsa &instSsa = form.instSsa(pc);
                if(instSsa.block != block)
                    continue;
                out += fmt::format("  pc{} {}", pc, opcodeName(code[pc]->opcode()));
                for(size_t slot = 0; slot < instSsa.defs.size(); ++slot)
                    out += fmt::format(" def%{}<-{}", instSsa.defs[slot].index(),
                                       refText(SsaRef{SsaRef::Kind::Inst,
                                                      static_cast<u32>(pc), static_cast<u16>(slot)}));
                const Access access = extractAccess(*code[pc]);
                for(size_t j = 0; j < access.uses.size(); ++j) {
                    const SsaRef &def =
                        j < instSsa.operandDefs.size() ? instSsa.operandDefs[j] : SsaRef{};
                    out += fmt::format(" use%{}<-{}", access.uses[j].index(), refText(def));
                }
                out += "\n";
            }
        }
        return out;
    }

} // namespace cial::inter
