#include <catch2/catch_all.hpp>

#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace cial;

TEST_CASE("tail recursion remains explicit and disabled by default", "[tail]") {
    Runtime rt;
    Context context{ rt };
    vm::VMState state{ context };
    VM vm{ &state };
    vm.eval("function sum(n, acc) { if(n == 0) return acc; return sum(n - 1, acc + n); } var total = sum(100, 0);"_str);
    REQUIRE(*vm.getGlobal<Integer>("total"_str) == 5050);
    REQUIRE(state.tailCallsEliminated() == 0);
}

TEST_CASE("tail recursion reuses numeric self-call frames beyond depth limit", "[tail]") {
    Runtime rt;
    Context context{ rt };
    vm::VMState state{ context };
    state.setTailRecursionEnabled(true);
    VM vm{ &state };
    vm.eval(R"(
        function sum(n, acc) { if(n <= 0) return acc; return sum(n - 1, acc + n); }
        var total = sum(20000, 0);
    )"_str);
    REQUIRE(*vm.getGlobal<Integer>("total"_str) == 200010000);
    REQUIRE(state.tailCallsEliminated() == 20000);
    REQUIRE(context.stackTop() == 0);
}

TEST_CASE("tail recursion snapshots swapped arguments before clearing locals", "[tail]") {
    Runtime rt;
    Context context{ rt };
    vm::VMState state{ context };
    state.setTailRecursionEnabled(true);
    VM vm{ &state };
    vm.eval(R"(
        function swap(n, a, b) {
            var initialized;
            if(n == 0) return a * 10 + b;
            return swap(n - 1, b, a);
        }
        var total = swap(10001, 2, 7);
    )"_str);
    REQUIRE(*vm.getGlobal<Integer>("total"_str) == 72);
    REQUIRE(state.tailCallsEliminated() == 10001);
}

TEST_CASE("tail recursion preserves exception handlers and other observable operations", "[tail]") {
    Runtime rt;
    Context context{ rt };
    vm::VMState state{ context };
    state.setTailRecursionEnabled(true);
    VM vm{ &state };
    vm.eval(R"(
        function guarded(n) {
            try { if(n == 0) throw 7; return guarded(n - 1); }
            catch(e) { return e + n; }
        }
        var answer = guarded(4);
        var visits = 0;
        function effects(n) { visits += 1; if(n == 0) return 12; return effects(n - 1); }
        var other = effects(8);
    )"_str);
    REQUIRE(*vm.getGlobal<Integer>("answer"_str) == 7);
    REQUIRE(*vm.getGlobal<Integer>("other"_str) == 12);
    REQUIRE(*vm.getGlobal<Integer>("visits"_str) == 9);
    REQUIRE(state.tailCallsEliminated() == 0);
}

TEST_CASE("tail recursion leaves non-tail calls objects and rebinding on normal path", "[tail]") {
    Runtime rt;
    Context context{ rt };
    vm::VMState state{ context };
    state.setTailRecursionEnabled(true);
    VM vm{ &state };
    vm.eval(R"(
        function fib(n) { if(n < 2) return n; return fib(n - 1) + fib(n - 2); }
        var result = fib(10);
        function original(n) { if(n == 0) return 3; return original(n - 1); }
        var saved = original;
        original = function(n) { return 99; };
        var rebound = saved(10);
        function objects(n, obj) { if(n == 0) return 7; return objects(n - 1, obj); }
        var objectResult = objects(8, []);
    )"_str);
    REQUIRE(*vm.getGlobal<Integer>("result"_str) == 55);
    REQUIRE(*vm.getGlobal<Integer>("rebound"_str) == 99);
    REQUIRE(*vm.getGlobal<Integer>("objectResult"_str) == 7);
    REQUIRE(state.tailCallsEliminated() == 0);
}
