#include <catch2/catch_all.hpp>

#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace cial;
using cial::inter::Label;
using cial::inter::Register;

TEST_CASE("execution trace counts actual loop edges", "[trace]") {
    Runtime rt;
    Context context{ rt };
    vm::VMState state{ context };
    vm::Chunk chunk;
    chunk.setRegCount(4);
    chunk.emit<inter::LoadImm>(Register{ 0 }, 0);
    chunk.emit<inter::LoadImm>(Register{ 1 }, 3);
    chunk.emit<inter::LoadImm>(Register{ 2 }, 1);
    chunk.emit<inter::LT>(Register{ 0 }, Register{ 1 }, Register{ 3 });
    chunk.emit<inter::Test>(Register{ 3 });
    chunk.emit<inter::JmpNE>(Label{ 8 });
    chunk.emit<inter::Add>(Register{ 0 }, Register{ 2 }, Register{ 0 });
    chunk.emit<inter::Jmp>(Label{ 3 });
    chunk.emit<inter::Ret>(Register{ 0 });
    vm::ExecutionTrace trace;
    state.setExecutionTrace(&trace);
    state.allocCallFrame(&chunk);
    state.run();
    REQUIRE(state.reg(0).asIntegerUnsafe() == 3);
    state.freeCallFrame();
    REQUIRE(trace.instructions() == 22);
    REQUIRE(trace.nodes().size() == 9);
    u64 edges = 0;
    size_t backEdges = 0;
    for(const auto &[edge, count] : trace.edges()) {
        edges += count;
        if(edge.second.second < edge.first.second) {
            REQUIRE(count == 3);
            ++backEdges;
        }
    }
    REQUIRE(edges == 21);
    REQUIRE(backEdges == 1);
    REQUIRE(trace.dumpDot().find("color=red") != std::string::npos);
}

TEST_CASE("execution trace separates eval runs and can be disabled", "[trace]") {
    Runtime rt;
    Context context{ rt };
    vm::VMState state{ context };
    VM vm{ &state };
    vm::ExecutionTrace trace;
    state.setExecutionTrace(&trace);
    vm.eval("var a = 7;"_str);
    vm.eval("a = a + 5;"_str);
    u64 edges = 0;
    for(const auto &[edge, count] : trace.edges()) {
        (void)edge;
        edges += count;
    }
    REQUIRE(edges + 2 == trace.instructions());
    const auto before = trace.instructions();
    state.setExecutionTrace(nullptr);
    REQUIRE(*vm.evalExpr<Integer>("a"_str) == 12);
    REQUIRE(trace.instructions() == before);
}

TEST_CASE("execution trace snapshots survive recursion exceptions and GC", "[trace]") {
    Runtime rt;
    Context context{ rt };
    vm::VMState state{ context };
    VM vm{ &state };
    vm::ExecutionTrace trace;
    state.setExecutionTrace(&trace);
    vm.eval(R"(
        function traced(n) { if(n == 0) throw "stop"; return traced(n - 1); }
        var caught = 0;
        try { traced(4); } catch(e) { caught = 17; }
    )"_str);
    state.setExecutionTrace(nullptr);
    REQUIRE(*vm.getGlobal<Integer>("caught"_str) == 17);
    bool sawThrow = false;
    bool sawFunction = false;
    for(const auto &[location, node] : trace.nodes()) {
        (void)location;
        sawThrow |= node.opcode == "Throw";
        sawFunction |= node.function == "traced";
        if(node.line)
            REQUIRE(*node.line <= 6);
    }
    REQUIRE(sawThrow);
    REQUIRE(sawFunction);
    const auto before = trace.dumpDot();
    rt.markSweep.collect([&] { rt.collectAllMark(); });
    REQUIRE(trace.dumpDot() == before);
}

TEST_CASE("execution trace source lines use predecessor and consecutive compression", "[trace]") {
    vm::Rlc lines;
    Common::SourceLocation first;
    first.start(2, 0);
    Common::SourceLocation second;
    second.start(5, 0);
    lines.addBytecodeLine(3, first);
    lines.addBytecodeLine(5, first); // 同行保留最早偏移
    lines.addBytecodeLine(9, second);
    lines.addBytecodeLine(12, first); // 回到旧行，不能删除 second
    REQUIRE(lines.rawEntries().size() == 3);
    REQUIRE(lines.rawEntries()[0].first == 3);
    REQUIRE(lines.find(8).start().line == 2);
    REQUIRE(lines.find(10).start().line == 5);
    REQUIRE(lines.find(50).start().line == 2);
    lines.addBytecodeLine(12, second); // 相同偏移以新位置为准
    REQUIRE(lines.rawEntries().size() == 3);
    REQUIRE(lines.find(12).start().line == 5);

    vm::Chunk chunk;
    chunk.rlc().addBytecodeLine(3, first);
    REQUIRE_FALSE(chunk.lineAtBc(0));
}

TEST_CASE("execution trace supports nested run from native callbacks", "[trace]") {
    Runtime rt;
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState state{ context };
    VM vm{ &state };
    vm::ExecutionTrace trace;
    state.setExecutionTrace(&trace);
    vm.eval(R"(
        var values = [3, 1, 2];
        function compare(a, b) { return a < b; }
        values.sort(compare);
    )"_str);
    state.setExecutionTrace(nullptr);
    REQUIRE(*vm.evalExpr<Integer>("values[0]"_str) == 1);
    bool callback = false;
    u64 edges = 0;
    for(const auto &[location, node] : trace.nodes()) {
        (void)location;
        callback |= node.function == "compare";
    }
    for(const auto &[edge, count] : trace.edges()) {
        (void)edge;
        edges += count;
    }
    REQUIRE(callback);
    REQUIRE(edges + 1 == trace.instructions());
}
