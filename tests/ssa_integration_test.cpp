#include <catch.hpp>

#include "gen/SSA.hpp"

using namespace cial;
using namespace cial::inter;

TEST_CASE("SSA integration preserves a value used only inside the return join", "[ssa]") {
    vm::Chunk chunk;
    chunk.setRegCount(2);
    chunk.emit<Test>(Register{ 0 });
    chunk.emit<JmpNE>(Label{ 4 });
    chunk.emit<LoadImm>(Register{ 1 }, 7);
    chunk.emit<Jmp>(Label{ 6 });
    chunk.emit<LoadImm>(Register{ 1 }, 9);
    chunk.emit<NOP>();
    chunk.emit<inter::Ret>(Register{ 1 });
    const auto before = chunk.dumpInstructions();
    ControlFlowGraph cfg;
    cfg.buildCFG(chunk);
    const auto form = SsaBuilder::buildChunkSsa(chunk, cfg.getBasicBlocks());
    REQUIRE(chunk.dumpInstructions() == before);
    REQUIRE(form.phis().size() == 1);
    REQUIRE(form.phis()[0].reg == Register{ 1 });
    REQUIRE(form.phis()[0].inputs.size() == 2);
    for(const auto input : form.phis()[0].inputs) {
        REQUIRE(input.kind == SsaRef::Kind::Inst);
        REQUIRE((input.index == 2 || input.index == 4));
    }
    REQUIRE(form.instSsa(0).operandDefs.size() == 1);
    REQUIRE(form.instSsa(0).operandDefs[0].kind == SsaRef::Kind::Entry);
    REQUIRE(form.instSsa(6).operandDefs.size() == 1);
    REQUIRE(form.instSsa(6).operandDefs[0].kind == SsaRef::Kind::Phi);
}

TEST_CASE("SSA integration keeps both initial and carried loop definitions", "[ssa]") {
    vm::Chunk chunk;
    chunk.setRegCount(3);
    chunk.emit<LoadImm>(Register{ 1 }, 0);
    chunk.emit<LoadImm>(Register{ 2 }, 1);
    chunk.emit<Add>(Register{ 1 }, Register{ 2 }, Register{ 1 });
    chunk.emit<Test>(Register{ 0 });
    chunk.emit<JmpE>(Label{ 2 });
    chunk.emit<inter::Ret>(Register{ 1 });
    ControlFlowGraph cfg;
    cfg.buildCFG(chunk);
    const auto form = SsaBuilder::buildChunkSsa(chunk, cfg.getBasicBlocks());
    REQUIRE(form.phis().size() == 1);
    REQUIRE(form.phis()[0].reg == Register{ 1 });
    REQUIRE(form.phis()[0].inputs.size() == 2);
    for(const auto input : form.phis()[0].inputs) {
        REQUIRE(input.kind == SsaRef::Kind::Inst);
        REQUIRE((input.index == 0 || input.index == 2));
    }
    REQUIRE(form.instSsa(2).operandDefs[0].kind == SsaRef::Kind::Phi);
    REQUIRE(form.instSsa(2).operandDefs[1].kind == SsaRef::Kind::Inst);
    REQUIRE(form.instSsa(2).operandDefs[1].index == 1);
}

TEST_CASE("SSA integration models read-modify-write and heap-only mutations", "[ssa]") {
    ChkIns instance{ Operand{ Register{ 0 } }, Operand{ Register{ 1 } } };
    const auto checked = SsaBuilder::extractAccess(instance);
    REQUIRE(checked.def == Register{ 0 });
    REQUIRE(checked.uses == Vec<Register>{ Register{ 0 }, Register{ 1 } });
    ChgThis binding{ Operand{ Register{ 0 } }, Operand{ Register{ 1 } } };
    const auto bound = SsaBuilder::extractAccess(binding);
    REQUIRE_FALSE(bound.def);
    REQUIRE(bound.uses == Vec<Register>{ Register{ 0 }, Register{ 1 } });
}

TEST_CASE("SSA integration distinguishes both method-read results or rejects the opcode", "[ssa]") {
    vm::Chunk chunk;
    chunk.setRegCount(4);
    chunk.emit<LoadImm>(Register{ 0 }, 10);
    chunk.getInstVec().push_back(std::make_unique<GMethodC>(Register{ 0 }, Atom{ 1 }, Register{ 1 }, Register{ 2 }));
    chunk.getInstVec().push_back(
        std::make_unique<CallMethodV>(Register{ 3 }, Register{ 1 }, 0, Vec<Register>{}, Register{ 2 }));
    chunk.emit<inter::Ret>(Register{ 3 });
    ControlFlowGraph cfg;
    cfg.buildCFG(chunk);
    SsaForm form;
    try {
        form = SsaBuilder::buildChunkSsa(chunk, cfg.getBasicBlocks());
    } catch(const std::runtime_error &error) {
        // A limited analyser may explicitly refuse multi-result operations.
        REQUIRE(std::string(error.what()).find("GMethodC") != std::string::npos);
        return;
    }
    const auto &uses = form.instSsa(2).operandDefs;
    REQUIRE(uses.size() == 2);
    REQUIRE(uses[0].kind == SsaRef::Kind::Inst);
    REQUIRE(uses[1].kind == SsaRef::Kind::Inst);
    REQUIRE_FALSE(uses[0] == uses[1]);
}
