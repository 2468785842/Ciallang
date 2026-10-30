#include <catch2/catch_test_macros.hpp>
#include "Binding.hpp"
#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace cial;

namespace {
    class HostCounter final : public NativeObjectType<HostCounter> {
    public:
        Integer value{ 41 };
        void call(vm::VMState &, u32, size_t) override { throw std::runtime_error("not callable"); }
        Value getProp(Atom) override { return Value{ Integer{ 7 } }; }
    };
    class OtherHost final : public NativeObjectType<OtherHost> {
    public:
        void call(vm::VMState &, u32, size_t) override { throw std::runtime_error("not callable"); }
    };
} // namespace

TEST_CASE("External native types register without interpreter changes", "[native][registration]") {
    Runtime rt;
    Context ctx{ rt };
    vm::VMState state{ ctx };
    VM vm{ &state };
    ctx.registerClass<HostCounter>("counter"_str);
    ctx.registerMethod<HostCounter>("answer"_str, [](VM *, const Value &self, size_t, Value *) {
        return Value{ binding::thisAs<HostCounter>(self).value + 1 };
    });
    ctx.registerProperty<HostCounter>(
        "value"_str, [](const Value &self) { return Value{ binding::thisAs<HostCounter>(self).value }; });
    REQUIRE(nativeTypeKeyFor<HostCounter>() != nativeTypeKeyFor<OtherHost>());
    ctx.registerClass<OtherHost>("other"_str);
    ctx.registerMethod<OtherHost>("answer"_str,
                                  [](VM *, const Value &, size_t, Value *) { return Value{ Integer{ 17 } }; });
    REQUIRE(*vm.evalExpr<Integer>("other.answer()"_str) == 17);
    REQUIRE(*vm.evalExpr<Integer>("counter.answer()"_str) == 42);
    REQUIRE(*vm.evalExpr<Integer>("counter.value"_str) == 41);
    REQUIRE(*vm.evalExpr<Integer>("counter.fallback"_str) == 7);
    REQUIRE(*vm.eval<Integer>("var saved = counter.answer; return saved();"_str) == 42);

    // 同一脚本调用点反复执行，验证注册版本使缓存失效。
    vm.eval("function readCounter() { return counter.answer(); }"_str);
    REQUIRE(*vm.evalExpr<Integer>("readCounter()"_str) == 42);
    ctx.registerMethod<HostCounter>("answer"_str,
                                    [](VM *, const Value &, size_t, Value *) { return Value{ Integer{ 99 } }; });
    REQUIRE(*vm.evalExpr<Integer>("readCounter()"_str) == 99);
    REQUIRE(*vm.evalExpr<Integer>("saved()"_str) == 42);

    // 同 Runtime 的不同 Context 仍拥有各自的注册表。
    Context second{ rt };
    vm::VMState secondState{ second };
    VM secondVm{ &secondState };
    second.registerClass<HostCounter>("counter"_str);
    second.registerMethod<HostCounter>("answer"_str,
                                       [](VM *, const Value &, size_t, Value *) { return Value{ Integer{ 123 } }; });
    REQUIRE(*secondVm.evalExpr<Integer>("counter.answer()"_str) == 123);
    REQUIRE(*vm.evalExpr<Integer>("readCounter()"_str) == 99);
    ctx.registerGlobal("counter"_str, Value{});
    rt.markSweep.collect([&] { rt.collectAllMark(); });
    REQUIRE(*vm.evalExpr<Integer>("saved()"_str) == 42);
}

TEST_CASE("Explicit native type tokens support host supplied identities", "[native][registration]") {
    static constexpr char token{};
    class ExplicitHost final : public Object {
    public:
        [[nodiscard]] NativeTypeKey nativeTypeKey() const noexcept override { return NativeTypeKey::fromToken(&token); }
        void call(vm::VMState &, u32, size_t) override { throw std::runtime_error("not callable"); }
    };
    Runtime rt;
    Context ctx{ rt };
    vm::VMState state{ ctx };
    VM vm{ &state };
    ctx.registerClass<ExplicitHost>("host"_str);
    ctx.registerMethod(NativeTypeKey::fromToken(&token), "answer"_str,
                       [](VM *, const Value &, size_t, Value *) { return Value{ Integer{ 42 } }; });
    ctx.registerProperty(NativeTypeKey::fromToken(&token), "value"_str,
                         [](const Value &) { return Value{ Integer{ 17 } }; });
    REQUIRE(*vm.evalExpr<Integer>("host.answer()"_str) == 42);
    REQUIRE(*vm.evalExpr<Integer>("host.value"_str) == 17);
}
