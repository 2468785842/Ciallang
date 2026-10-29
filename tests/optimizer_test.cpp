#include <catch.hpp>

#include <algorithm>

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

TEST_CASE("optimizer - propagates constants into the unique-predecessor successor block") {
    vm::Chunk chunk;
    chunk.emit<LoadImm>(Register{ 1 }, 20);
    chunk.emit<LoadImm>(Register{ 2 }, 0);
    chunk.emit<JmpE>(Label{ 4 });
    chunk.emit<Add>(Register{ 1 }, Register{ 1 }, Register{ 3 });
    chunk.emit<inter::Ret>(Register{ 3 });
    chunk.setRegCount(4);

    OptimizerManager optimizer{ chunk };
    optimizer.addOptimizer(std::make_unique<IntegerConstantPropagationOptimizer>());
    optimizer.addOptimizer(std::make_unique<DeadLoadEliminationOptimizer>());
    (void)optimizer.applyOptimizations();

    // 块 [3,4) 的唯一前驱是入口块，继承 %1=20 后 Add 折叠为 40；
    // 折叠后 %1/%2 均死，随之删除。
    const auto &instructions = chunk.getInstVec();
    REQUIRE(instructions.size() == 3);
    REQUIRE(instructions[0]->opcode() == TacOpCode::JmpE);
    REQUIRE(instructions[1]->opcode() == TacOpCode::LoadImm);
    REQUIRE(instructions[1]->getOp1().value<Register>() == Register{ 3 });
    REQUIRE(instructions[1]->getOp2().value<Integer>() == 40);
    REQUIRE(instructions[2]->opcode() == TacOpCode::Ret);
}

TEST_CASE("optimizer - does not propagate into a block with multiple predecessors") {
    vm::Chunk chunk;
    chunk.emit<LoadImm>(Register{ 1 }, 20);
    chunk.emit<LoadImm>(Register{ 2 }, 1);
    chunk.emit<JmpE>(Label{ 4 });
    chunk.emit<Jmp>(Label{ 5 });
    chunk.emit<LoadImm>(Register{ 1 }, 7);
    chunk.emit<Add>(Register{ 1 }, Register{ 1 }, Register{ 3 });
    chunk.emit<inter::Ret>(Register{ 3 });
    chunk.setRegCount(4);

    OptimizerManager optimizer{ chunk };
    optimizer.addOptimizer(std::make_unique<IntegerConstantPropagationOptimizer>());
    optimizer.addOptimizer(std::make_unique<DeadLoadEliminationOptimizer>());
    (void)optimizer.applyOptimizations();

    // Add 所在块的前驱是条件分支两侧（Jmp 与 fallthrough），必须保持动态加法。
    const auto &instructions = chunk.getInstVec();
    const auto addPosition = std::ranges::find_if(instructions, [](const auto &instruction) {
        return instruction->opcode() == TacOpCode::Add;
    });
    REQUIRE(addPosition != instructions.end());
    REQUIRE((*addPosition)->getOp1().value<Register>() == Register{ 1 });
}

TEST_CASE("optimizer - does not propagate into a loop header across the back edge") {
    vm::Chunk chunk;
    chunk.emit<LoadImm>(Register{ 1 }, 5);
    chunk.emit<Add>(Register{ 1 }, Register{ 1 }, Register{ 3 });
    chunk.emit<JmpE>(Label{ 1 });
    chunk.emit<inter::Ret>(Register{ 3 });
    chunk.setRegCount(4);

    OptimizerManager optimizer{ chunk };
    optimizer.addOptimizer(std::make_unique<IntegerConstantPropagationOptimizer>());
    optimizer.addOptimizer(std::make_unique<DeadLoadEliminationOptimizer>());
    (void)optimizer.applyOptimizations();

    // 循环头块有回边前驱，入口处的 %1=5 不得跨越回边传播。
    REQUIRE(chunk.inst(1)->opcode() == TacOpCode::Add);
}

TEST_CASE("optimizer - detects natural loop from a back edge") {
    vm::Chunk chunk;
    chunk.emit<LoadImm>(Register{ 1 }, 5); // 0 init
    chunk.emit<Jmp>(Label{ 3 }); // 1 → 循环头
    chunk.emit<Add>(Register{ 1 }, Register{ 1 }, Register{ 1 }); // 2 latch
    chunk.emit<LoadImm>(Register{ 2 }, 7); // 3 循环头（条件）
    chunk.emit<JmpE>(Label{ 2 }); // 4 → body；fallthrough 出循环
    chunk.emit<inter::Ret>(Register{ 1 }); // 5
    chunk.setRegCount(3);

    ControlFlowGraph cfg;
    (void)cfg.buildCFG(chunk);
    const auto &loops = cfg.getLoops();

    // 块划分：[0,2)=init+Jmp、[2,3)=latch、[3,5)=header 条件、[5,6)=出口。
    // 回边 latch→header（header 支配 latch 的唯一入路）
    REQUIRE(loops.size() == 1);
    REQUIRE(loops[0].header == 2);
    REQUIRE(loops[0].body == Vec<size_t>({ 1, 2 }));
}

TEST_CASE("optimizer - detects nested loops with distinct headers") {
    vm::Chunk chunk;
    chunk.emit<LoadImm>(Register{ 1 }, 0); // 0 外层 init
    chunk.emit<Jmp>(Label{ 5 }); // 1 → 外层头
    chunk.emit<LoadImm>(Register{ 2 }, 0); // 2 内层 init（外层 body 起点）
    chunk.emit<Jmp>(Label{ 8 }); // 3 → 内层头
    chunk.emit<Add>(Register{ 1 }, Register{ 1 }, Register{ 1 }); // 4 外层 latch
    chunk.emit<LoadImm>(Register{ 3 }, 7); // 5 外层头（条件）
    chunk.emit<JmpE>(Label{ 2 }); // 6 → 外层 body；fallthrough 退出
    chunk.emit<inter::Ret>(Register{ 1 }); // 7
    chunk.emit<LoadImm>(Register{ 4 }, 1); // 8 内层头（条件）
    chunk.emit<JmpNE>(Label{ 11 }); // 9 → 内层 body；fallthrough 内层退出
    chunk.emit<Jmp>(Label{ 4 }); // 10 内层退出 → 外层 latch
    chunk.emit<Add>(Register{ 2 }, Register{ 2 }, Register{ 2 }); // 11 内层 latch
    chunk.emit<Jmp>(Label{ 8 }); // 12 → 内层头（回边）
    chunk.setRegCount(5);

    ControlFlowGraph cfg;
    (void)cfg.buildCFG(chunk);
    const auto &loops = cfg.getLoops();

    REQUIRE(loops.size() == 2);
    // 块划分：[0,2)/[2,4)/[4,5)/[5,7)/[7,8)/[8,10)/[10,11)/[11,13)。
    // 外层 {外层条件, 内层 init, 外层 latch, 内层头, 内层出口, 内层体}，
    // 内层 {内层头, 内层体}，内层 body 是外层 body 的子集。
    REQUIRE(loops[0].header == 3);
    REQUIRE(loops[0].body == Vec<size_t>({ 1, 2, 3, 5, 6, 7 }));
    REQUIRE(loops[1].header == 5);
    REQUIRE(loops[1].body == Vec<size_t>({ 5, 7 }));
}

TEST_CASE("optimizer - ignores loops inside unreachable blocks") {
    vm::Chunk chunk;
    chunk.emit<inter::Ret>(Register{ 0 });
    chunk.emit<Jmp>(Label{ 1 }); // 不可达代码里的自环
    chunk.setRegCount(1);

    ControlFlowGraph cfg;
    (void)cfg.buildCFG(chunk);
    REQUIRE(cfg.getLoops().empty());
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
