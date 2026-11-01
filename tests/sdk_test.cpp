#include <catch2/catch_test_macros.hpp>

#include "Binding.hpp"
#include "cial/Engine.hpp"
#include "types/Dictionary.hpp"

using namespace cial;

TEST_CASE("SDK reports compilation diagnostics and recovers after script exceptions", "[sdk]") {
    Engine engine;
    Common::Result invalid;
    auto ignored = engine.execute("var = ;", invalid, "xp3://scripts/broken.tjs");
    REQUIRE(invalid.isFailed());
    REQUIRE_FALSE(invalid.messages().empty());
    REQUIRE(invalid.messages()[0].source() == "xp3://scripts/broken.tjs");
    REQUIRE(engine.context().stackTop() == 0);

    Common::Result thrown;
    try {
        auto value = engine.execute("throw 123;", thrown);
        for(const auto &message : thrown.messages())
            INFO(message.message());
        REQUIRE_FALSE(thrown.isFailed());
        FAIL("throw must escape the host boundary");
    } catch(const vm::ScriptExceptionCarrier &exception) {
        REQUIRE(exception.value.asInteger().unwrap() == 123);
    }
    REQUIRE_FALSE(thrown.isFailed());
    REQUIRE(engine.context().stackTop() == 0);
    Common::Result valid;
    auto value = engine.execute("6 * 7", valid, "host://expression", true);
    REQUIRE_FALSE(valid.isFailed());
    REQUIRE(value->asInteger().unwrap() == 42);
}

TEST_CASE("SDK execution contexts retain declarations and their function receivers", "[sdk][gc]") {
    Engine engine;
    auto context = engine.runtime().create<Dictionary>();
    const Value receiver{ context.get() };
    Common::Result result;
    auto ignored = engine.execute("var answer = 41; function read() { return answer + 1; } var empty = null; "
                                  "property p { getter { return answer + 1; } setter(value) { answer = value - 1; } }",
                                  result, "host://context", false, receiver);
    REQUIRE_FALSE(result.isFailed());
    REQUIRE_FALSE(engine.context().global()->hasProp(engine.runtime().atomTable.intern("answer"_str)));
    engine.runtime().markSweep.collect([&] { engine.runtime().collectAllMark(); });
    const auto function = context->getProp(engine.runtime().atomTable.intern("read"_str));
    REQUIRE(engine.vm().callValue(function, 0, nullptr).asInteger().unwrap() == 42);
    Common::Result expression;
    auto answer = engine.execute("answer + 1", expression, "host://context-expression", true, receiver);
    REQUIRE_FALSE(expression.isFailed());
    REQUIRE(answer->asInteger().unwrap() == 42);
    engine.context().registerGlobal("isolated"_str, receiver);
    Common::Result property;
    auto propertyValue = engine.execute("isolated.p", property, "host://property", true);
    REQUIRE_FALSE(property.isFailed());
    REQUIRE(propertyValue->asInteger().unwrap() == 42);
    Common::Result setter;
    auto assigned = engine.execute("p = 51;", setter, "host://setter", false, receiver);
    REQUIRE_FALSE(setter.isFailed());
    REQUIRE(engine.vm().callValue(function, 0, nullptr).asInteger().unwrap() == 51);
}

TEST_CASE("SDK nested execution preserves thrown values and caller control flow", "[sdk][gc]") {
    Engine engine;
    engine.context().module("Host"_str).function("execute"_str, [&] {
        Common::Result result;
        return *engine.execute("throw 73;", result, "host://nested");
    });
    Common::Result result;
    auto value = engine.execute("var seen = 0; try { Host.execute(); seen = -1; } catch(e) { seen = e; }", result,
                                "host://outer");
    REQUIRE_FALSE(result.isFailed());
    REQUIRE(engine.vm().getGlobal<Integer>("seen"_str).get() == 73);
    REQUIRE(engine.context().stackTop() == 0);
}

TEST_CASE("GC traces null in class properties and native receivers", "[sdk][gc]") {
    Engine engine;
    auto native = engine.runtime().create<NativeFunction>(
        [](VM *, const Value &, size_t, Value *) { return Value{ Integer{ 42 } }; });
    native->setThisObj(Value{ static_cast<Object *>(nullptr) });
    Common::Result result;
    auto value = engine.execute("class C { var empty = null; } var c = new C(); c.empty = null;", result);
    REQUIRE_FALSE(result.isFailed());
    engine.runtime().markSweep.collect([&] { engine.runtime().collectAllMark(); });
    Common::Result expression;
    auto isNull = engine.execute("c.empty === null", expression, "host://after-gc", true);
    REQUIRE_FALSE(expression.isFailed());
    REQUIRE(isNull->asBool());
    REQUIRE(engine.vm().callValue(Value{ native.get() }, 0, nullptr).asInteger().unwrap() == 42);
}
