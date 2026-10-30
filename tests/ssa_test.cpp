//
// Created by LiDong on 2026/10/29.
//
// SSA 构造测试：抽象 CFG 层（支配树/支配边界/phi 放置/重命名/异常边界）
// 与 Chunk 层（精确 def/use、pruned phi、def-use 链、拒绝语义、opcode 冒烟）。
#include <catch2/catch_all.hpp>

#include <functional>

#include "gen/ControlFlowGraph.hpp"
#include "gen/SSA.hpp"
#include "vm/Bytecode.hpp"
#include "vm/Chunk.hpp"

using namespace cial;
using namespace cial::inter;

namespace {
    SsaBlockFacts::Inst instFacts(const u32 pc, const Vec<u16> &defs, const Vec<u16> &uses = {}) {
        SsaBlockFacts::Inst inst;
        inst.pc = pc;
        inst.defs = defs;
        inst.uses = uses;
        return inst;
    }

    // 一致性不变量：每个有效 operandDef 必须指向"定义同一寄存器"的指令/phi
    void checkDefUseConsistency(const SsaForm &form) {
        for(size_t pc = 0; pc < form.insts().size(); ++pc) {
            const InstSsa &instSsa = form.instSsa(pc);
            for(const SsaRef &def : instSsa.operandDefs) {
                if(!def.valid())
                    continue;
                if(def.kind == SsaRef::Kind::Inst) {
                    INFO("pc=" << pc << " -> Inst:" << def.index << "#" << def.slot);
                    REQUIRE(def.index < form.insts().size());
                    REQUIRE(def.slot < form.instSsa(def.index).defs.size());
                } else if(def.kind == SsaRef::Kind::Phi) {
                    INFO("pc=" << pc << " -> Phi:" << def.index);
                    REQUIRE(def.index < form.phis().size());
                }
            }
        }
    }
} // namespace

TEST_CASE("ssa - linear chain has no phis and a single idom spine") {
    // B0 -> B1 -> B2，变量 0 在 B0 定义、B2 使用
    Vec<Vec<size_t>> successors = {{1}, {2}, {}};
    Vec<SsaBlockFacts> facts(3);
    facts[0].defined = {0};
    facts[0].insts.push_back(instFacts(0, {0}));
    facts[2].upwardUses = {0};
    facts[2].insts.push_back(instFacts(1, {}, {0}));

    const SsaForm form = SsaBuilder::build(successors, 0, facts, 1);

    REQUIRE(form.idomOf(0) == 0);
    REQUIRE(form.idomOf(1) == 0);
    REQUIRE(form.idomOf(2) == 1);
    REQUIRE(form.phis().empty());
    REQUIRE(form.dominanceFrontierOf(0).empty());
    REQUIRE(form.dominanceFrontierOf(1).empty());
    checkDefUseConsistency(form);
}

TEST_CASE("ssa - diamond join places phi with per-edge inputs") {
    // B0 -> B1/B2 -> B3；变量 1 在两臂各定义一次，B3 使用
    Vec<Vec<size_t>> successors = {{1, 2}, {3}, {3}, {}};
    Vec<SsaBlockFacts> facts(4);
    facts[1].defined = {1};
    facts[1].insts.push_back(instFacts(0, {1}));
    facts[2].defined = {1};
    facts[2].insts.push_back(instFacts(1, {1}));
    facts[3].upwardUses = {1};
    facts[3].insts.push_back(instFacts(2, {}, {1}));

    const SsaForm form = SsaBuilder::build(successors, 0, facts, 2);

    REQUIRE(form.idomOf(1) == 0);
    REQUIRE(form.idomOf(2) == 0);
    REQUIRE(form.idomOf(3) == 0);
    REQUIRE(form.dominanceFrontierOf(1) == Vec<size_t>{3});
    REQUIRE(form.dominanceFrontierOf(2) == Vec<size_t>{3});

    REQUIRE(form.phis().size() == 1);
    const SsaPhi &phi = form.phis()[0];
    REQUIRE(phi.block == 3);
    REQUIRE(phi.reg == Register{1});
    REQUIRE(phi.inputs == Vec<SsaRef>{
        SsaRef{SsaRef::Kind::Inst, 0}, SsaRef{SsaRef::Kind::Inst, 1}});

    // B3 的使用解析到 phi；两条臂的定义分别被 phi 输入引用
    REQUIRE(form.instSsa(2).operandDefs ==
        Vec<SsaRef>{SsaRef{SsaRef::Kind::Phi, 0}});
    const Vec<SsaUse> &phiUses = form.usesOfDef({SsaRef::Kind::Phi, 0});
    REQUIRE(phiUses == Vec<SsaUse>{{SsaUse::Kind::Operand, 2, 0}});
    REQUIRE(form.usesOfDef({SsaRef::Kind::Inst, 0}).size() == 1);
    REQUIRE(form.usesOfDef({SsaRef::Kind::Inst, 1}).size() == 1);
    checkDefUseConsistency(form);
}

TEST_CASE("ssa - loop header phi carries the back-edge definition") {
    // B0 -> B1 -> B2 -> B1(回边)/B3；变量 1 在 B0 初始化、B2 递增、B1 使用
    Vec<Vec<size_t>> successors = {{1}, {2}, {1, 3}, {}};
    Vec<SsaBlockFacts> facts(4);
    facts[0].defined = {1};
    facts[0].insts.push_back(instFacts(0, {1}));
    facts[1].upwardUses = {1};
    facts[1].insts.push_back(instFacts(1, {}, {1}));
    facts[2].defined = {1};
    facts[2].insts.push_back(instFacts(2, {1}));
    facts[3].upwardUses = {1};
    facts[3].insts.push_back(instFacts(3, {}, {1}));

    const SsaForm form = SsaBuilder::build(successors, 0, facts, 2);

    REQUIRE(form.dominanceFrontierOf(2) == Vec<size_t>{1});
    REQUIRE(form.phis().size() == 1);
    const SsaPhi &phi = form.phis()[0];
    REQUIRE(phi.block == 1);
    REQUIRE(phi.reg == Register{1});
    // 前驱序 = [B0, B2(回边)]
    REQUIRE(phi.inputs == Vec<SsaRef>{
        SsaRef{SsaRef::Kind::Inst, 0}, SsaRef{SsaRef::Kind::Inst, 2}});
    // 循环携带：B1 内使用解析到 header phi，B3 使用解析到回边定义
    REQUIRE(form.instSsa(1).operandDefs == Vec<SsaRef>{SsaRef{SsaRef::Kind::Phi, 0}});
    REQUIRE(form.instSsa(3).operandDefs == Vec<SsaRef>{SsaRef{SsaRef::Kind::Inst, 2}});
    checkDefUseConsistency(form);
}

TEST_CASE("ssa - entry self-loop is in its own dominance frontier") {
    // 入口自环：DF(entry) 按形式定义包含自身
    Vec<Vec<size_t>> successors = {{0}};
    Vec<SsaBlockFacts> facts(1);
    facts[0].defined = {0};

    const SsaForm form = SsaBuilder::build(successors, 0, facts, 1);
    REQUIRE(form.dominanceFrontierOf(0) == Vec<size_t>{0});
}

TEST_CASE("ssa - pruned placement skips dead variables") {
    // 变量 5 只在 B1 定义、无任何使用 → 汇合块不放 phi
    Vec<Vec<size_t>> successors = {{1, 2}, {3}, {3}, {}};
    Vec<SsaBlockFacts> facts(4);
    facts[1].defined = {5};
    facts[1].insts.push_back(instFacts(0, {5}));

    const SsaForm form = SsaBuilder::build(successors, 0, facts, 6);
    REQUIRE(form.phis().empty());
}

TEST_CASE("ssa - unreachable blocks stay out of dominance and phis") {
    Vec<Vec<size_t>> successors = {{1}, {}, {}};
    Vec<SsaBlockFacts> facts(3);
    facts[0].defined = {0};
    facts[2].defined = {0}; // 不可达块的定义不参与
    facts[2].insts.push_back(instFacts(7, {0}));

    const SsaForm form = SsaBuilder::build(successors, 0, facts, 1);
    REQUIRE(form.idomOf(2) == SsaForm::InvalidBlock);
    REQUIRE(form.phis().empty());
    // 不可达指令保留静态 defs，但没有重命名结果
    REQUIRE(form.instSsa(7).defs == Vec<Register>{Register{0}});
    REQUIRE(form.instSsa(7).operandDefs.empty());
}

TEST_CASE("ssa - exception entry introduces a distinct catch identity") {
    // B0 -> B1(异常着陆) -> B2；变量 0 在 B0 定义、B1 使用
    Vec<Vec<size_t>> successors = {{1}, {2}, {}};
    Vec<SsaBlockFacts> facts(3);
    facts[0].defined = {0};
    facts[0].insts.push_back(instFacts(0, {0}));
    facts[1].upwardUses = {0};
    facts[1].insts.push_back(instFacts(1, {}, {0}));

    const Vec<bool> exceptionEntries = {false, true, false};
    const SsaForm form = SsaBuilder::build(successors, 0, facts, 1, {}, exceptionEntries);

    REQUIRE(form.isExceptionEntry(1));
    // 着陆块内使用重置为该块自己的 Catch 未知值，不引用块外定义，
    // 也不同于入口参数身份
    REQUIRE(form.instSsa(1).operandDefs ==
        Vec<SsaRef>{SsaRef{SsaRef::Kind::Catch, 1}});
    REQUIRE(form.usesOfDef({SsaRef::Kind::Catch, 1}).size() == 1);
    REQUIRE(form.usesOfDef({SsaRef::Kind::Entry, 0}).empty());
}

TEST_CASE("ssa - facts variables beyond variableCount are rejected") {
    Vec<Vec<size_t>> successors = {{}};
    Vec<SsaBlockFacts> facts(1);
    facts[0].defined = {5};

    REQUIRE_THROWS_AS(SsaBuilder::build(successors, 0, facts, 1), std::runtime_error);
}

TEST_CASE("ssa - straight-line chunk resolves uses to instruction defs") {
    vm::Chunk chunk;
    chunk.emit<LoadImm>(Register{1}, 10);
    chunk.emit<LoadImm>(Register{2}, 20);
    chunk.emit<Add>(Register{1}, Register{2}, Register{3});
    chunk.emit<inter::Ret>(Register{3});
    chunk.setRegCount(4);

    ControlFlowGraph cfg;
    const auto blocks = cfg.buildCFG(chunk);
    const SsaForm form = SsaBuilder::buildChunkSsa(chunk, blocks);

    REQUIRE(form.blockCount() == 1);
    REQUIRE(form.phis().empty());
    REQUIRE(form.instSsa(2).defs == Vec<Register>{Register{3}});
    REQUIRE(form.instSsa(2).operandDefs ==
        Vec<SsaRef>{SsaRef{SsaRef::Kind::Inst, 0}, SsaRef{SsaRef::Kind::Inst, 1}});
    REQUIRE(form.instSsa(3).operandDefs == Vec<SsaRef>{SsaRef{SsaRef::Kind::Inst, 2}});
    REQUIRE(form.usesOfDef({SsaRef::Kind::Inst, 2}).size() == 1);
    checkDefUseConsistency(form);
}

TEST_CASE("ssa - branch chunk places join phi for redefined register") {
    vm::Chunk chunk;
    chunk.emit<LoadImm>(Register{1}, 1);
    chunk.emit<LoadImm>(Register{2}, 0);
    chunk.emit<JmpE>(Label{4});
    chunk.emit<LoadImm>(Register{2}, 7); // then 臂重定义 %2
    chunk.emit<inter::Ret>(Register{2});

    ControlFlowGraph cfg;
    const auto blocks = cfg.buildCFG(chunk);
    REQUIRE(blocks.size() == 3);
    const SsaForm form = SsaBuilder::buildChunkSsa(chunk, blocks);

    REQUIRE(form.phis().size() == 1);
    const SsaPhi &phi = form.phis()[0];
    REQUIRE(phi.block == 2);
    REQUIRE(phi.reg == Register{2});
    // 前驱 = [B0(JmpE 目标), B1(fallthrough)]
    REQUIRE(phi.inputs == Vec<SsaRef>{
        SsaRef{SsaRef::Kind::Inst, 1}, SsaRef{SsaRef::Kind::Inst, 3}});
    REQUIRE(form.instSsa(4).operandDefs == Vec<SsaRef>{SsaRef{SsaRef::Kind::Phi, 0}});
    checkDefUseConsistency(form);
}

TEST_CASE("ssa - accumulating loop chunk gets a header phi with back-edge def") {
    vm::Chunk chunk;
    chunk.emit<LoadImm>(Register{1}, 0);
    chunk.emit<Add>(Register{1}, Register{2}, Register{1}); // 读-写 %1
    chunk.emit<JmpE>(Label{1});
    chunk.emit<inter::Ret>(Register{1});
    chunk.setRegCount(3);

    ControlFlowGraph cfg;
    const auto blocks = cfg.buildCFG(chunk);
    REQUIRE(blocks.size() == 3);
    const SsaForm form = SsaBuilder::buildChunkSsa(chunk, blocks);

    REQUIRE(form.phis().size() == 1);
    const SsaPhi &phi = form.phis()[0];
    REQUIRE(phi.block == 1);
    REQUIRE(phi.reg == Register{1});
    REQUIRE(phi.inputs == Vec<SsaRef>{
        SsaRef{SsaRef::Kind::Inst, 0}, SsaRef{SsaRef::Kind::Inst, 1}});
    // Add 的 %1 使用解析到 header phi；%2 无定义 → 入口参数值
    REQUIRE(form.instSsa(1).operandDefs ==
        Vec<SsaRef>{SsaRef{SsaRef::Kind::Phi, 0}, SsaRef{SsaRef::Kind::Entry, 2}});
    // Ret 解析到回边上的 Add 定义
    REQUIRE(form.instSsa(3).operandDefs == Vec<SsaRef>{SsaRef{SsaRef::Kind::Inst, 1}});
    checkDefUseConsistency(form);
}

TEST_CASE("ssa - inc is a read-modify-write of the same register") {
    vm::Chunk chunk;
    chunk.emit<LoadImm>(Register{1}, 5);
    chunk.emit<Inc>(Register{1});
    chunk.emit<inter::Ret>(Register{1});

    ControlFlowGraph cfg;
    const auto blocks = cfg.buildCFG(chunk);
    const SsaForm form = SsaBuilder::buildChunkSsa(chunk, blocks);

    const InstSsa &inc = form.instSsa(1);
    REQUIRE(inc.defs == Vec<Register>{Register{1}});
    REQUIRE(inc.operandDefs == Vec<SsaRef>{SsaRef{SsaRef::Kind::Inst, 0}});
    REQUIRE(form.instSsa(2).operandDefs == Vec<SsaRef>{SsaRef{SsaRef::Kind::Inst, 1}});
    REQUIRE(form.usesOfDef({SsaRef::Kind::Inst, 0}).size() == 1);
    checkDefUseConsistency(form);
}

TEST_CASE("ssa - chkins redefines its value operand per VM semantics") {
    // VMState: ChkIns 的 dst(op1) 既是被测值又是 bool 输出槽——读-改-写
    vm::Chunk chunk;
    chunk.emit<LoadImm>(Register{1}, 1);
    chunk.emit<LoadImm>(Register{2}, 2);
    chunk.emit<ChkIns>(Register{1}, Register{2});
    chunk.emit<inter::Ret>(Register{1});

    ControlFlowGraph cfg;
    const auto blocks = cfg.buildCFG(chunk);
    const SsaForm form = SsaBuilder::buildChunkSsa(chunk, blocks);

    const InstSsa &chk = form.instSsa(2);
    REQUIRE(chk.defs == Vec<Register>{Register{1}});
    REQUIRE(chk.operandDefs ==
        Vec<SsaRef>{SsaRef{SsaRef::Kind::Inst, 0}, SsaRef{SsaRef::Kind::Inst, 1}});
    REQUIRE(form.instSsa(3).operandDefs == Vec<SsaRef>{SsaRef{SsaRef::Kind::Inst, 2}});
    checkDefUseConsistency(form);
}

TEST_CASE("ssa - chgthis reads both registers and defines none") {
    // VMState: ChgThis 只改函数对象的 thisObj，不写任何寄存器
    vm::Chunk chunk;
    chunk.emit<LoadImm>(Register{1}, 1);
    chunk.emit<LoadImm>(Register{2}, 2);
    chunk.emit<ChgThis>(Register{1}, Register{2});
    chunk.emit<inter::Ret>(Register{1});

    ControlFlowGraph cfg;
    const auto blocks = cfg.buildCFG(chunk);
    const SsaForm form = SsaBuilder::buildChunkSsa(chunk, blocks);

    REQUIRE(form.instSsa(2).defs.empty());
    // ChgThis 之后 %1 仍是 LoadImm 的定义
    REQUIRE(form.instSsa(3).operandDefs == Vec<SsaRef>{SsaRef{SsaRef::Kind::Inst, 0}});
    checkDefUseConsistency(form);
}

TEST_CASE("ssa - gmethodc defines dst and receiver as distinct identities") {
    // VMState: GMethodC 同时写 dst（方法值）与 receiver（绑定接收者）
    vm::Chunk chunk;
    chunk.emit<Global>(Register{1});
    chunk.emitRaw<GMethodC>(Register{1}, Atom{9}, Register{2}, Register{3});
    chunk.emitRaw<CallMethodV>(Register{4}, Register{2}, 0, Vec<Register>{}, Register{3});
    chunk.emit<inter::Ret>(Register{4});
    chunk.setRegCount(5);

    ControlFlowGraph cfg;
    const auto blocks = cfg.buildCFG(chunk);
    const SsaForm form = SsaBuilder::buildChunkSsa(chunk, blocks);

    REQUIRE(form.instSsa(1).defs == Vec<Register>{Register{2}, Register{3}});
    // CallMethodV 使用序 = [receiver, func]：两个身份同 pc 不同 slot
    REQUIRE(form.instSsa(2).operandDefs == Vec<SsaRef>{
        SsaRef{SsaRef::Kind::Inst, 1, 1}, SsaRef{SsaRef::Kind::Inst, 1, 0}});
    checkDefUseConsistency(form);
}

TEST_CASE("ssa - try handler is an SSA boundary and preserves liveness") {
    vm::Chunk chunk;
    chunk.emit<LoadImm>(Register{1}, 1); // B0
    chunk.emit<Call>(Register{0}, Register{2}, 1); // B1，try 保护
    chunk.emit<LoadImm>(Register{3}, 2); // B2 = handler（tryEnd=2）
    chunk.emit<inter::Ret>(Register{1});
    chunk.addThrowHandler(vm::ThrowHandler{1, 2, 4});

    ControlFlowGraph cfg;
    const auto blocks = cfg.buildCFG(chunk);
    REQUIRE(blocks.size() == 3);
    const SsaForm form = SsaBuilder::buildChunkSsa(chunk, blocks);

    REQUIRE(form.isExceptionEntry(2));
    // handler 内的使用保守重置为该着陆块的 Catch 未知值
    REQUIRE(form.instSsa(3).operandDefs == Vec<SsaRef>{SsaRef{SsaRef::Kind::Catch, 2}});
    checkDefUseConsistency(form);
}

TEST_CASE("ssa - callv records func and arg uses precisely") {
    vm::Chunk chunk;
    chunk.emit<LoadImm>(Register{1}, 1);
    chunk.emit<LoadImm>(Register{2}, 2);
    chunk.emitRaw<CallV>(Register{3}, Register{1}, 2,
        Vec<Register>{Register{1}, Register{2}});
    chunk.emit<inter::Ret>(Register{3});

    ControlFlowGraph cfg;
    const auto blocks = cfg.buildCFG(chunk);
    const SsaForm form = SsaBuilder::buildChunkSsa(chunk, blocks);

    const InstSsa &call = form.instSsa(2);
    REQUIRE(call.defs == Vec<Register>{Register{3}});
    REQUIRE(call.operandDefs ==
        Vec<SsaRef>{SsaRef{SsaRef::Kind::Inst, 0}, SsaRef{SsaRef::Kind::Inst, 0},
            SsaRef{SsaRef::Kind::Inst, 1}});
    const Vec<SsaUse> &funcUses = form.usesOfDef({SsaRef::Kind::Inst, 0});
    REQUIRE(funcUses.size() == 2); // func + arg[%1]
    checkDefUseConsistency(form);
}

TEST_CASE("ssa - implicit frame-arg call opcodes are rejected explicitly") {
    const auto buildWith = [](const std::function<void(vm::Chunk &)> &emit) {
        vm::Chunk chunk;
        chunk.emit<LoadImm>(Register{1}, 1);
        chunk.emit<LoadImm>(Register{2}, 2);
        emit(chunk);
        chunk.setRegCount(3);
        ControlFlowGraph cfg;
        const auto blocks = cfg.buildCFG(chunk);
        return SsaBuilder::buildChunkSsa(chunk, blocks);
    };

    // CallOmit/CallExpand/CollapseArgs 隐式读取帧实参，必须拒绝而非猜测
    REQUIRE_THROWS_AS(buildWith([](vm::Chunk &chunk) {
        chunk.emitRaw<CallOmit>(Register{3}, Register{1});
    }), std::runtime_error);
    REQUIRE_THROWS_AS(buildWith([](vm::Chunk &chunk) {
        chunk.emitRaw<CallExpand>(Register{3}, Register{1}, 0,
            Vec<std::pair<u8, Register>>{{1, Register{2}}});
    }), std::runtime_error);
    REQUIRE_THROWS_AS(buildWith([](vm::Chunk &chunk) {
        chunk.emitRaw<CollapseArgs>(Register{3}, 0);
    }), std::runtime_error);

    // 错误信息点名 opcode
    try {
        vm::Chunk chunk;
        chunk.emit<LoadImm>(Register{1}, 1);
        chunk.emitRaw<CallOmit>(Register{2}, Register{1});
        ControlFlowGraph cfg;
        const auto blocks = cfg.buildCFG(chunk);
        (void)SsaBuilder::buildChunkSsa(chunk, blocks);
        FAIL("expected rejection");
    } catch(const std::runtime_error &error) {
        REQUIRE(std::string(error.what()).find("CallOmit") != std::string::npos);
    }
}

TEST_CASE("ssa - every supported tac opcode has a registered def/use entry") {
    // 支持集全量冒烟：未来新增 opcode 若未登记 def/use，此处直接抛错
    vm::Chunk chunk;
    const Register r1{ 1 }, r2{ 2 }, r3{ 3 }, r4{ 4 }, r5{ 5 };
    chunk.emit<NOP>();
    chunk.emit<Debugger>();
    chunk.emit<Push>(r1);
    chunk.emit<PopN>(2);
    chunk.emit<Global>(r1);
    chunk.emit<Super>(r1);
    chunk.emit<This>(r1);
    chunk.emit<Test>(r1);
    chunk.emit<Inv>(r1);
    chunk.emit<Throw>(r1);
    chunk.emit<inter::Ret>(r1);
    chunk.emit<Jmp>(Label{0});
    chunk.emit<JmpE>(Label{0});
    chunk.emit<JmpNE>(Label{0});
    chunk.emit<ToInt>(r1, r2);
    chunk.emit<ToReal>(r1, r2);
    chunk.emit<Asc>(r1, r2);
    chunk.emit<Chr>(r1, r2);
    chunk.emit<ToString>(r1, r2);
    chunk.emit<TypeOf>(r1, r2);
    chunk.emit<LNot>(r1, r2);
    chunk.emit<ChgSign>(r1, r2);
    chunk.emit<ChgThis>(r1, r2);
    chunk.emit<ChkInv>(r1, r2);
    chunk.emit<EvalStr>(r1, r2);
    chunk.emit<ChkIns>(r1, r2);
    chunk.emit<TestVoid>(r1, r2);
    chunk.emit<Load>(r1, chunk.addConstant(Atom{7}));
    chunk.emit<LoadImm>(r1, 1);
    chunk.emit<DGlobal>(Atom{1}, r1);
    chunk.emit<GGlobal>(Atom{1}, r2);
    chunk.emit<GThis>(Atom{1}, r2);
    chunk.emit<DThis>(Atom{1}, r1);
    chunk.emit<GLocal>(r1, r2);
    chunk.emit<DLocal>(r1, r2);
    chunk.emit<Add>(r1, r2, r3);
    chunk.emit<Sub>(r1, r2, r3);
    chunk.emit<Mul>(r1, r2, r3);
    chunk.emit<Div>(r1, r2, r3);
    chunk.emit<Idiv>(r1, r2, r3);
    chunk.emit<Mod>(r1, r2, r3);
    chunk.emit<BXor>(r1, r2, r3);
    chunk.emit<BOr>(r1, r2, r3);
    chunk.emit<BAnd>(r1, r2, r3);
    chunk.emit<BlShift>(r1, r2, r3);
    chunk.emit<BrShift>(r1, r2, r3);
    chunk.emit<BurShift>(r1, r2, r3);
    chunk.emit<EQ>(r1, r2, r3);
    chunk.emit<NEQ>(r1, r2, r3);
    chunk.emit<AbsEQ>(r1, r2, r3);
    chunk.emit<AbsNEQ>(r1, r2, r3);
    chunk.emit<LT>(r1, r2, r3);
    chunk.emit<LE>(r1, r2, r3);
    chunk.emit<GT>(r1, r2, r3);
    chunk.emit<GE>(r1, r2, r3);
    chunk.emit<LAnd>(r1, r2, r3);
    chunk.emit<LOr>(r1, r2, r3);
    chunk.emit<Call>(r1, r2, 1);
    chunk.emit<GProp>(r1, r2, r3);
    chunk.emit<DProp>(r1, r2, r3);
    chunk.emit<DelProp>(r1, r2, r3);
    chunk.emit<GPropRaw>(r1, r2, r3);
    chunk.emit<GRefRaw>(Atom{1}, r3);
    chunk.emit<DPropRaw>(r1, r2, r3);
    chunk.emit<DerefGet>(r3, r4);
    chunk.emit<DerefSet>(r3, r4);
    chunk.emit<NewArray>(r4, 2);
    chunk.emit<NewDict>(r4);
    chunk.emit<GetIndex>(r1, r2, r4);
    chunk.emit<SetIndex>(r1, r2, r4);
    chunk.emit<WithPush>(r4);
    chunk.emit<WithPop>();
    chunk.emit<GetWith>(r4);
    chunk.emit<NewRegExp>(r4, chunk.addConstant(Atom{7}));
    chunk.emitRaw<CallV>(r4, r1, 2, Vec<Register>{r1, r2});
    chunk.emit<CmpEQ>(r1, r2);
    chunk.emit<CmpDEQ>(r1, r2);
    chunk.emit<CmpLT>(r1, r2);
    chunk.emit<CmpGT>(r1, r2);
    chunk.emit<Inc>(r4);
    chunk.emit<Dec>(r4);
    chunk.emit<BNot>(r1, r4);
    chunk.emit<ToNumber>(r1, r4);
    chunk.emit<ToOctet>(r1, r4);
    chunk.emit<GPropC>(r1, Atom{1}, r4);
    chunk.emit<DPropC>(r1, Atom{1}, r4);
    chunk.emit<GPropRawC>(r1, Atom{1}, r4);
    chunk.emit<DPropRawC>(r1, Atom{1}, r4);
    chunk.emit<DelPropC>(r1, Atom{1}, r4);
    chunk.emitRaw<CompareTest>(TacOpCode::LT, r1, r2, r4);
    chunk.emitRaw<GMethodC>(r1, Atom{1}, r4, r5);
    chunk.emitRaw<CallMethodV>(r4, r1, 1, Vec<Register>{r2}, r5);

    chunk.setRegCount(6);

    ControlFlowGraph cfg;
    const auto blocks = cfg.buildCFG(chunk);
    SsaForm form;
    REQUIRE_NOTHROW(form = SsaBuilder::buildChunkSsa(chunk, blocks));
    REQUIRE(form.insts().size() == chunk.getInstVec().size());
    checkDefUseConsistency(form);
}

TEST_CASE("ssa - dump text mentions blocks and phis") {
    vm::Chunk chunk;
    chunk.emit<LoadImm>(Register{1}, 1);
    chunk.emit<JmpE>(Label{3});
    chunk.emit<LoadImm>(Register{1}, 2);
    chunk.emit<inter::Ret>(Register{1});

    ControlFlowGraph cfg;
    const auto blocks = cfg.buildCFG(chunk);
    const SsaForm form = SsaBuilder::buildChunkSsa(chunk, blocks);
    const std::string text = SsaBuilder::dumpSsaText(chunk, form);
    REQUIRE(text.find("== SSA") == 0);
    REQUIRE(text.find("B2") != std::string::npos);
}
