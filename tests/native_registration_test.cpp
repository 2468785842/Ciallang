#include <algorithm>
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

namespace {
    // API 元数据推导断言辅助：按名字查找成员
    const ApiMeta *findMeta(const std::vector<ApiMeta> &members, const std::string &name) {
        const auto it =
            std::find_if(members.begin(), members.end(), [&](const ApiMeta &meta) { return meta.name == name; });
        return it == members.end() ? nullptr : &*it;
    }
} // namespace

TEST_CASE("Native registrations record API metadata with derivable signatures", "[native][registration][api]") {
    Runtime rt;
    Context ctx{ rt };
    ctx.registerClass<HostCounter>("counter"_str);
    ctx.nativeType<HostCounter>()
        .method("bump"_str,
                [](VM *, const Value &self, size_t, Value *) {
                    return Value{ binding::thisAs<HostCounter>(self).value + 1 };
                })
        .method("five"_str, [](VM *, const Value &, size_t, Value *) { return Integer{ 5 }; })
        .method("variadic"_str, [](VM *, const Value &, size_t, Value *) { return Value{}; })
        .property("value"_str, [](const Value &self) { return Value{ binding::thisAs<HostCounter>(self).value }; });

    const auto &catalog = ctx.apiCatalog();
    const auto key = TypeTraits<HostCounter>::key();
    REQUIRE(catalog.typeNames.at(key) == "counter");
    const auto &members = catalog.typeMembers.at(key);
    REQUIRE(members.size() == 4);

    // 返回 Value 的方法推导为 any；返回具体类型（Integer）按 C++ 类型输出
    const auto *bump = findMeta(members, "bump");
    REQUIRE(bump != nullptr);
    CHECK(bump->kind == ApiKind::Method);
    CHECK(bump->ret == "any");
    const auto *five = findMeta(members, "five");
    REQUIRE(five != nullptr);
    CHECK(five->ret == "Integer");
    // 变长方法（Value* 尾参形态）标记 variadic
    const auto *variadic = findMeta(members, "variadic");
    REQUIRE(variadic != nullptr);
    CHECK(variadic->variadic);

    // property 由 getter 返回类型推导（此处返回 Value → any）
    const auto *value = findMeta(members, "value");
    REQUIRE(value != nullptr);
    REQUIRE(value->kind == ApiKind::Property);
    CHECK(value->ret == "any");
}

TEST_CASE("Binding module functions record global metadata with typed signatures", "[native][registration][api]") {
    Runtime rt;
    Context ctx{ rt };
    binding::Module root{ ctx };
    auto math = root.module("Math"_str);
    // makeTypedFunction 的 callable 只收业务参数（NativeFunTraits 已剥 VM*/this/argc 前缀）
    math.function("abs"_str, [](const Real v) {
        const auto x = v.value();
        return Real{ x < 0 ? -x : x };
    });
    root.module("Nested"_str).module("Deep"_str).function("fn"_str, [] { return Value{}; });

    const auto &catalog = ctx.apiCatalog();
    // Math 命名空间 + abs + Nested/Deep 命名空间 + fn
    REQUIRE(catalog.globals.size() == 5);
    const auto *abs = findMeta(catalog.globals, "abs");
    REQUIRE(abs != nullptr);
    CHECK(abs->kind == ApiKind::Global);
    CHECK(abs->owner == "Math");
    CHECK(abs->ret == "Real");
    REQUIRE(abs->paramTypes.size() == 1);
    CHECK(abs->paramTypes[0] == "Real");
    const auto *fn = findMeta(catalog.globals, "fn");
    REQUIRE(fn != nullptr);
    CHECK(fn->owner == "Nested.Deep");
    CHECK(fn->ret == "any");
}
