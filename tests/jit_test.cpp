#include <catch.hpp>
#include <limits>

#include "vm/IntegerJit.hpp"
#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace cial;
using cial::inter::Register;

TEST_CASE("integer JIT executes native arithmetic and commits every output", "[jit]") {
    if(!vm::IntegerJit::available()) {
        REQUIRE_THROWS(vm::IntegerJit(vm::JitMode::Eager));
        return;
    }
    vm::Chunk chunk;
    chunk.setRegCount(5);
    chunk.emit<inter::LoadImm>(Register{ 2 }, 7);
    chunk.emit<inter::Add>(Register{ 0 }, Register{ 1 }, Register{ 3 });
    chunk.emit<inter::Mul>(Register{ 3 }, Register{ 2 }, Register{ 4 });
    chunk.emit<inter::Ret>(Register{ 4 });
    chunk.toBytecode();
    Value registers[5]{ Value{ 2 }, Value{ 3 } };
    vm::IntegerJit jit{ vm::JitMode::Eager };
    u64 pc = 0;
    bool zf = true;
    REQUIRE(jit.tryRun(chunk, registers, pc, zf));
    REQUIRE(registers[2].asIntegerUnsafe() == 7);
    REQUIRE(registers[3].asIntegerUnsafe() == 5);
    REQUIRE(registers[4].asIntegerUnsafe() == 35);
    REQUIRE(zf);
    REQUIRE(chunk.code()[pc] == static_cast<u8>(vm::VmOpCode::Ret));
    REQUIRE(jit.stats().executions == 1);
}

TEST_CASE("integer JIT guards types and overflow without partial state changes", "[jit]") {
    if(!vm::IntegerJit::available())
        return;
    vm::Chunk chunk;
    chunk.setRegCount(4);
    chunk.emit<inter::LoadImm>(Register{ 2 }, 7);
    chunk.emit<inter::Add>(Register{ 0 }, Register{ 1 }, Register{ 3 });
    chunk.emit<inter::Ret>(Register{ 3 });
    chunk.toBytecode();
    vm::IntegerJit jit{ vm::JitMode::Eager };
    Value registers[4]{ Value{ "text"_str }, Value{ 1 }, Value{ 99 }, Value{ 123 } };
    u64 pc = 0;
    bool zf = true;
    REQUIRE_FALSE(jit.tryRun(chunk, registers, pc, zf));
    REQUIRE(pc == 0);
    REQUIRE(registers[2].asIntegerUnsafe() == 99);
    REQUIRE(jit.stats().typeExits == 1);
    registers[0] = Value{ std::numeric_limits<i64>::max() };
    REQUIRE_FALSE(jit.tryRun(chunk, registers, pc, zf));
    REQUIRE(pc == 0);
    REQUIRE(zf);
    REQUIRE(registers[2].asIntegerUnsafe() == 99);
    REQUIRE(registers[3].asIntegerUnsafe() == 123);
    REQUIRE(jit.stats().sideExits == 1);
    REQUIRE(jit.stats().executions == 0);
}

TEST_CASE("trace JIT records loop paths and exits safely when the branch changes", "[jit]") {
    if(!vm::IntegerJit::available())
        return;
    Runtime rt;
    Context context{ rt };
    vm::VMState state{ context };
    VM vm{ &state };
    state.setJitMode(vm::JitMode::Trace, 2);
    vm.eval(R"(
        function spin(n) {
            var i = 0; var total = 0;
            while(i < n) { total = total + i; i = i + 1; }
            return total;
        }
        var answer = spin(300);
    )"_str);
    REQUIRE(*vm.getGlobal<Integer>("answer"_str) == 44850);
    REQUIRE(state.jitStats().compiled > 0);
    REQUIRE(state.jitStats().executions > 0);
    REQUIRE(state.jitStats().recordedTraces > 0);
    REQUIRE(state.jitStats().sideExits > 0);
}

TEST_CASE("integer JIT preserves dynamic types across repeated function calls", "[jit]") {
    if(!vm::IntegerJit::available())
        return;
    Runtime rt;
    Context context{ rt };
    vm::VMState state{ context };
    VM vm{ &state };
    state.setJitMode(vm::JitMode::Eager);
    vm.eval(R"(
        function plusTwo(x) { return x + 2; }
        var number = plusTwo(7);
        var text = plusTwo("a");
    )"_str);
    REQUIRE(*vm.getGlobal<Integer>("number"_str) == 9);
    auto text = vm.getGlobal<Value>("text"_str);
    REQUIRE(text->asString().unwrap()->toStdStr() == "a2");
    REQUIRE(state.jitStats().executions > 0);
    REQUIRE(state.jitStats().typeExits > 0);
}

TEST_CASE("integer JIT preserves signed comparisons flags and aliased destinations", "[jit]") {
    if(!vm::IntegerJit::available())
        return;
    const inter::TacOpCode comparisons[] = { inter::TacOpCode::EQ,     inter::TacOpCode::NEQ, inter::TacOpCode::AbsEQ,
                                             inter::TacOpCode::AbsNEQ, inter::TacOpCode::LT,  inter::TacOpCode::LE,
                                             inter::TacOpCode::GT,     inter::TacOpCode::GE };
    for(const auto [lhs, rhs] : { std::pair<i64, i64>{ -5, 3 }, { 3, 3 } }) {
        const bool expected[] = { lhs == rhs, lhs != rhs, lhs == rhs, lhs != rhs, lhs<rhs, lhs <= rhs, lhs> rhs,
                                  lhs >= rhs };
        for(size_t kind = 0; kind < 8; ++kind) {
            vm::Chunk chunk;
            chunk.setRegCount(3);
            chunk.getInstVec().push_back(std::make_unique<inter::CompareTest>(
                comparisons[kind], Register{ 0 }, Register{ 1 }, Register{ 0 }));
            chunk.emit<inter::GLocal>(Register{ 0 }, Register{ 2 });
            chunk.emit<inter::Ret>(Register{ 2 });
            chunk.toBytecode();
            Value registers[3]{ Value{ lhs }, Value{ rhs }, Value{ "old payload"_str } };
            vm::IntegerJit jit{ vm::JitMode::Eager };
            u64 pc = 0;
            bool zf = !expected[kind];
            REQUIRE(jit.tryRun(chunk, registers, pc, zf));
            REQUIRE(registers[0].asIntegerUnsafe() == expected[kind]);
            REQUIRE(registers[2].asIntegerUnsafe() == expected[kind]);
            REQUIRE(zf == expected[kind]);
        }
    }
}

TEST_CASE("integer JIT leaves protected code and profiling on interpreter path", "[jit]") {
    if(!vm::IntegerJit::available())
        return;
    vm::Chunk chunk;
    chunk.setRegCount(2);
    chunk.emit<inter::LoadImm>(Register{ 0 }, 4);
    chunk.emit<inter::LoadImm>(Register{ 1 }, 5);
    chunk.emit<inter::Ret>(Register{ 1 });
    chunk.addThrowHandler({ 0, 2, 1 });
    chunk.toBytecode();
    vm::IntegerJit jit{ vm::JitMode::Eager };
    Value registers[2];
    u64 pc = 0;
    bool zf = false;
    REQUIRE_FALSE(jit.tryRun(chunk, registers, pc, zf));
    REQUIRE(jit.stats().compiled == 0);
    Runtime rt;
    Context context{ rt };
    vm::VMState state{ context };
    VM vm{ &state };
    vm::ExecutionTrace trace;
    state.setJitMode(vm::JitMode::Eager);
    state.setExecutionTrace(&trace);
    vm.eval("var a = 1; var b = 2; var result = a + b;"_str);
    REQUIRE(trace.instructions() > 0);
    REQUIRE(state.jitStats().executions == 0);
}
