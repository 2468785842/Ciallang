#include <catch.hpp>

#include "gen/ControlFlowGraph.hpp"
#include "gen/Optimizer.hpp"
#include "vm/Chunk.hpp"

using namespace cial;
using namespace cial::inter;

TEST_CASE("optimizer - CFG builds branch and fallthrough edges") {
    vm::Chunk chunk;
    chunk.emit<LoadImm>(Register{ 1 }, 1);
    chunk.emit<JmpE>(Label{ 3 });
    chunk.emit<LoadImm>(Register{ 2 }, 2);
    chunk.emit<inter::Ret>(Register{ 2 });

    ControlFlowGraph cfg;
    const auto blocks = cfg.buildCFG(chunk);

    REQUIRE(blocks.size() == 3);
    REQUIRE(blocks[0].start == 0);
    REQUIRE(blocks[0].end == 2);
    REQUIRE(blocks[0].successors.size() == 2);
    REQUIRE(blocks[0].successors[0] == 2);
    REQUIRE(blocks[0].successors[1] == 1);
    REQUIRE(blocks[1].successors == Vec<size_t>{ 2 });
    REQUIRE(blocks[2].successors.empty());
}

TEST_CASE("optimizer - folds safe integers and remaps jumps after dead load removal") {
    vm::Chunk chunk;
    chunk.emit<LoadImm>(Register{ 1 }, 20);
    chunk.emit<LoadImm>(Register{ 2 }, 22);
    chunk.emit<Add>(Register{ 1 }, Register{ 2 }, Register{ 3 });
    chunk.emit<LoadImm>(Register{ 4 }, 99); // dead
    chunk.emit<Jmp>(Label{ 6 });
    chunk.emit<LoadImm>(Register{ 5 }, 8); // dead on the fallthrough block
    chunk.emit<inter::Ret>(Register{ 3 });
    chunk.setRegCount(6);

    OptimizerManager optimizer{ chunk };
    optimizer.addOptimizer(std::make_unique<IntegerConstantPropagationOptimizer>());
    optimizer.addOptimizer(std::make_unique<DeadLoadEliminationOptimizer>());
    (void)optimizer.applyOptimizations();

    const auto &instructions = chunk.getInstVec();
    REQUIRE(instructions.size() == 3);
    REQUIRE(instructions[0]->opcode() == TacOpCode::LoadImm);
    REQUIRE(instructions[0]->getOp1().value<Register>() == Register{ 3 });
    REQUIRE(instructions[0]->getOp2().value<Integer>() == 42);
    REQUIRE(instructions[1]->opcode() == TacOpCode::Jmp);
    REQUIRE(instructions[1]->getOp1().value<Label>().address() == 2);
    REQUIRE(instructions[2]->opcode() == TacOpCode::Ret);
    REQUIRE(instructions[2]->getOp1().value<Register>() == Register{ 3 });
}

TEST_CASE("optimizer - remaps try ranges and source lines while compacting NOPs") {
    vm::Chunk chunk;
    chunk.emit<NOP>();
    chunk.emit<LoadImm>(Register{ 1 }, 7);
    chunk.emit<NOP>();
    chunk.emit<inter::Ret>(Register{ 1 });
    chunk.setRegCount(2);
    chunk.addThrowHandler(vm::ThrowHandler{ 0, 2, 1 });

    Common::SourceLocation line1;
    line1.start(1, 1);
    line1.end(1, 2);
    Common::SourceLocation line2;
    line2.start(2, 1);
    line2.end(2, 2);
    Common::SourceLocation line3;
    line3.start(3, 1);
    line3.end(3, 2);
    chunk.rlc().addBytecodeLine(0, line1);
    chunk.rlc().addBytecodeLine(1, line2);
    chunk.rlc().addBytecodeLine(2, line3);

    OptimizerManager optimizer{ chunk };
    optimizer.addOptimizer(std::make_unique<DeadLoadEliminationOptimizer>());
    (void)optimizer.applyOptimizations();

    REQUIRE(chunk.getInstVec().size() == 2);
    const auto &handler = chunk.getThrowHandlers().front();
    REQUIRE(handler.tryStart == 0);
    REQUIRE(handler.tryEnd == 1);
    const auto &lines = chunk.rlc().rawEntries();
    REQUIRE(lines.size() == 2);
    REQUIRE(lines[0].first == 0);
    REQUIRE(lines[0].second.start().line == 2);
    REQUIRE(lines[1].first == 1);
    REQUIRE(lines[1].second.start().line == 3);
}

TEST_CASE("optimizer - preserves constant-pool loads because materialization may allocate") {
    vm::Chunk chunk;
    const ConstIdx value = chunk.addConstant(Atom{ 1 });
    chunk.emit<Load>(Register{ 1 }, value);
    chunk.emit<inter::Ret>(Register{ 0 });
    chunk.setRegCount(2);

    OptimizerManager optimizer{ chunk };
    optimizer.addOptimizer(std::make_unique<DeadLoadEliminationOptimizer>());
    (void)optimizer.applyOptimizations();

    REQUIRE(chunk.getInstVec().size() == 2);
    REQUIRE(chunk.getInstVec()[0]->opcode() == TacOpCode::Load);
}

TEST_CASE("optimizer - compare-test fusion preserves values and control-flow entries", "[optimizer]") {
    vm::Chunk chunk;
    chunk.setRegCount(3);
    chunk.emit<LoadImm>(Register{ 0 }, 3);
    chunk.emit<LoadImm>(Register{ 1 }, 4);
    chunk.emit<LT>(Register{ 0 }, Register{ 1 }, Register{ 2 });
    chunk.emit<Test>(Register{ 2 });
    chunk.emit<JmpNE>(Label{ 6 });
    chunk.emit<inter::Ret>(Register{ 2 });
    chunk.emit<inter::Ret>(Register{ 0 });
    OptimizerManager manager{ chunk };
    manager.addOptimizer(std::make_unique<CompareTestFusionOptimizer>());
    const auto stats = manager.applyOptimizations();
    REQUIRE(stats.instructionsRemovedByCompaction == 1);
    REQUIRE(chunk.inst(2)->opcode() == TacOpCode::CompareTest);
    REQUIRE(chunk.inst(2)->getOp3().value<Register>() == Register{ 2 });
    REQUIRE(chunk.inst(3)->getOp1().value<Label>().address() == 5);

    vm::Chunk target;
    target.setRegCount(3);
    target.emit<LT>(Register{ 0 }, Register{ 1 }, Register{ 2 });
    target.emit<Test>(Register{ 2 });
    target.emit<Jmp>(Label{ 1 }); // Test 是独立入口，不得移除
    OptimizerManager other{ target };
    other.addOptimizer(std::make_unique<CompareTestFusionOptimizer>());
    (void)other.applyOptimizations();
    REQUIRE(target.inst(0)->opcode() == TacOpCode::LT);
    REQUIRE(target.inst(1)->opcode() == TacOpCode::Test);
}
