#include <catch2/catch_test_macros.hpp>
#include "Binding.hpp"
#include "types/Class.hpp"

using namespace cial;
namespace {
    struct Counter {
        static inline int destroyed{};
        int number;
        Value held;
        [[maybe_unused]] char large[1024]{}; // 验证 payload 不受 512B GC 节点限制
        explicit Counter(int n) : number(n) {}
        ~Counter() { ++destroyed; }
        [[nodiscard]] int value() const { return number; }
        void set(int n) { number = n; }
        int add(int n) { return number += n; }
        [[nodiscard]] std::string echo(const std::string &s) const { return std::to_string(number) + ":" + s; }
        [[nodiscard]] Value getHeld() const { return held; }
        void keep(Value v) { held = std::move(v); }
    };
    int twice(int n) noexcept { return n * 2; }
} // namespace

TEST_CASE("Declarative native bindings construct ordinary C++ classes", "[native][binding]") {
    Runtime rt;
    Context ctx{ rt };
    vm::VMState state{ ctx };
    VM vm{ &state };
    binding::Module root{ ctx };
    auto graphics = root.module("Graphics"_str);
    graphics.class_<Counter>("Counter"_str)
        .constructor<int>()
        .method("add"_str, &Counter::add)
        .method("echo"_str, &Counter::echo)
        .method("keep"_str, &Counter::keep)
        .property("held"_str, &Counter::getHeld)
        .property("value"_str, &Counter::value, &Counter::set)
        .property("readOnly"_str, &Counter::value)
        .staticFunction("twice"_str, &twice)
        .trace([](const Counter &counter, binding::Tracer trace) noexcept { trace(counter.held); });
    graphics.function("twice"_str, &twice);
    ctx.module("Graphics"_str).function("another"_str, &twice);
    vm.eval("var counter = new Graphics.Counter(40);"_str);
    REQUIRE(*vm.evalExpr<Integer>("Graphics.another(3)"_str) == 6);
    REQUIRE(*vm.evalExpr<Integer>("counter.add(2)"_str) == 42);
    REQUIRE(*vm.evalExpr<Integer>("counter.value"_str) == 42);
    REQUIRE(*vm.eval<Integer>("counter.value = 7; return counter.value;"_str) == 7);
    REQUIRE(*vm.evalExpr<Integer>("Graphics.twice(4) + Graphics.Counter.twice(5)"_str) == 18);
    REQUIRE(**vm.evalExpr<String *>(R"(counter.echo("hi"))"_str) == "7:hi"_str);
    REQUIRE(*vm.eval<Integer>("var saved = counter.add; return saved(1);"_str) == 8);
    REQUIRE(*vm.eval<Integer>("try { counter.readOnly = 3; } catch(e) { return 1; } return 0;"_str) == 1);
    REQUIRE(*vm.eval<Integer>("try { counter.add(); } catch(e) { return 1; } return 0;"_str) == 1);
    REQUIRE(*vm.eval<Integer>("try { counter.add(9223372036854775807); } catch(e) { return 1; } return 0;"_str) == 1);
    REQUIRE(*vm.eval<Integer>("try { new Graphics.Counter(); } catch(e) { return 1; } return 0;"_str) == 1);

    // 显式重绑到错误类型必须报脚本异常，不能转为不合法的 C++ 指针。
    auto method = ctx.findMethod(nativeTypeKeyFor<Counter>(), rt.atomTable.intern("add"_str));
    Value arg{ Integer{ 1 } };
    REQUIRE_THROWS(binding::thisAs<Counter>(Value{ Integer{ 1 } }));
    REQUIRE_THROWS(method->callDirect(state, Value{ ctx.global() }, 1, &arg));

    // payload 的 Value 经 trace 保根，提取方法保活大于 GC 节点的 C++ payload。
    vm.eval("counter.keep(new Graphics.Counter(17));"_str);
    rt.markSweep.collect([&] { rt.collectAllMark(); });
    REQUIRE(*vm.evalExpr<Integer>("counter.held.value"_str) == 17);
    vm.eval("counter.keep((const)[null][0]);"_str);
    rt.markSweep.collect([&] { rt.collectAllMark(); });
    REQUIRE(*vm.evalExpr<Integer>("!counter.held"_str) == 1);
    auto shared = std::make_shared<Counter>(99);
    ctx.registerGlobal("shared"_str, binding::wrap(ctx, shared));
    REQUIRE(*vm.eval<Integer>("shared.value = 100; return shared.value;"_str) == 100);
    REQUIRE(shared->number == 100);
    graphics.function("factory"_str, []() { return std::make_shared<Counter>(55); });
    REQUIRE(*vm.evalExpr<Integer>("Graphics.factory().value"_str) == 55);
    ctx.registerGlobal("reserved"_str, Value{ Integer{ 1 } });
    REQUIRE_THROWS(ctx.module("reserved"_str));
    ctx.registerGlobal("counter"_str, Value{});
    rt.markSweep.collect([&] { rt.collectAllMark(); });
    REQUIRE(*vm.evalExpr<Integer>("saved(1)"_str) == 9);
    ctx.registerGlobal("saved"_str, Value{});
}

TEST_CASE("Native payload ownership releases on runtime destruction", "[native][binding]") {
    const int before = Counter::destroyed;
    std::weak_ptr<Counter> observed;
    {
        Runtime rt;
        Context ctx{ rt };
        vm::VMState state{ ctx };
        VM vm{ &state };
        binding::Module{ ctx }.class_<Counter>("Counter"_str).constructor<int>();
        vm.eval("var value = new Counter(3);"_str);
        auto shared = std::make_shared<Counter>(4);
        observed = shared;
        ctx.registerGlobal("shared"_str, binding::wrap(ctx, shared));
        shared.reset();
        REQUIRE_FALSE(observed.expired());
    }
    REQUIRE(observed.expired());
    REQUIRE(Counter::destroyed == before + 2);
}

TEST_CASE("Active native accessors survive replacement and nested GC", "[native][binding]") {
    Runtime rt;
    Context ctx{ rt };
    vm::VMState state{ ctx };
    VM vm{ &state };
    binding::Module{ ctx }.class_<Counter>("Counter"_str).constructor<int>();
    {
        auto getter = rt.create<NativeFunction>([&](VM *, const Value &, size_t, Value *) {
            auto replacement =
                rt.create<NativeFunction>([](VM *, const Value &, size_t, Value *) { return Value{ Integer{ 99 } }; });
            ctx.registerAccessor(nativeTypeKeyFor<Counter>(), "changing"_str, replacement.get());
            rt.markSweep.collect([&] { rt.collectAllMark(); });
            return Value{ Integer{ 42 } };
        });
        ctx.registerAccessor(nativeTypeKeyFor<Counter>(), "changing"_str, getter.get());
    }
    vm.eval("var counter = new Counter(1);"_str);
    REQUIRE(*vm.evalExpr<Integer>("counter.changing"_str) == 42);
    REQUIRE(*vm.evalExpr<Integer>("counter.changing"_str) == 99);
}
