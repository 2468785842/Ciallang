#include <catch2/catch_test_macros.hpp>
#include "stdlib/StandardLibrary.hpp"

#include "common/SourceFile.hpp"
#include "types/Array.hpp"
#include "types/Class.hpp"
#include "types/Octet.hpp"
#include "vm/VM.hpp"

using namespace cial;

namespace {
    struct TrackedObject final : Object {
        int &destroyed;
        explicit TrackedObject(int &destroyed) : destroyed(destroyed) {}
        ~TrackedObject() override { ++destroyed; }
        void call(vm::VMState &, u32, size_t) override {}
    };
} // namespace

TEST_CASE("GC guards retain independent roots across copies and moves", "[runtime][gc]") {
    int destroyed = 0;
    Runtime runtime;
    auto collect = [&] { runtime.markSweep.collect([&] { runtime.collectAllMark(); }); };
    {
        auto copy = [&] {
            auto original = runtime.create<TrackedObject>(destroyed);
            return RootGuard<TrackedObject>{ original };
        }();
        collect();
        REQUIRE(destroyed == 0);
        auto moved = std::move(copy);
        REQUIRE(copy.get() == nullptr);
        collect();
        REQUIRE(destroyed == 0);
        auto assigned = runtime.create<TrackedObject>(destroyed);
        assigned = moved;
        collect();
        REQUIRE(destroyed == 1);
        assigned = std::move(moved);
        REQUIRE(moved.get() == nullptr);
        collect();
        REQUIRE(destroyed == 1);
    }
    collect();
    REQUIRE(destroyed == 2);
    REQUIRE(runtime.handles.empty());
}

TEST_CASE("Value handles keep objects alive after their evaluation and copied handle end", "[runtime][gc]") {
    int destroyed = 0;
    Runtime runtime;
    {
        auto survivor = [&] {
            auto object = runtime.create<TrackedObject>(destroyed);
            VM::Handle<Value> original{ &runtime, Value{ object.get() } };
            auto copy = original;
            return copy;
        }();
        runtime.markSweep.collect([&] { runtime.collectAllMark(); });
        REQUIRE(destroyed == 0);
        REQUIRE(survivor->isObject());
        auto moved = std::move(survivor);
        runtime.markSweep.collect([&] { runtime.collectAllMark(); });
        REQUIRE(destroyed == 0);
        VM::Handle<Value> assigned{ &runtime, Value{} };
        assigned = moved;
        moved = std::move(assigned);
        runtime.markSweep.collect([&] { runtime.collectAllMark(); });
        REQUIRE(destroyed == 0);
    }
    runtime.markSweep.collect([&] { runtime.collectAllMark(); });
    REQUIRE(destroyed == 1);
    REQUIRE(runtime.handles.empty());
}

TEST_CASE("Class registration and builtins survive allocation-triggered collection", "[runtime][gc]") {
    Runtime runtime;
    Context context{ runtime };
    stdlib::registerStandardLibrary(context);
    // Fill the pool with garbage so a subsequent registration triggers GC.
    auto [used, total] = runtime.markSweep.memoryInfo();
    for(auto remaining = (total - used) / NODE_SIZE; remaining > 1; --remaining)
        runtime.create<Array>(&runtime);
    context.registerClass<Array>("Collection"_str, &runtime);
    context.registerProperty<Array>("answer"_str, [](const Value &) { return Value{ Integer{ 42 } }; });
    vm::VMState state{ context };
    VM vm{ &state };
    runtime.markSweep.collect([&] { runtime.collectAllMark(); });
    REQUIRE(*vm.evalExpr<Integer>("Collection.answer"_str) == 42);
    REQUIRE(*vm.evalExpr<Integer>("new Array().count"_str) == 0);
    REQUIRE(*vm.evalExpr<Integer>("new Math.RandomGenerator(1).random32() >= 0"_str) == 1);
}

TEST_CASE("Reloading source resets decoding position and marks", "[source]") {
    Common::Result diagnostics;
    Common::SourceFile source;
    source.load(diagnostics, "first\nlong source");
    source.next(diagnostics);
    source.pushMark();
    while(!source.eof())
        source.next(diagnostics);
    source.load(diagnostics, "x");
    REQUIRE_FALSE(diagnostics.isFailed());
    REQUIRE(source.pos() == 0);
    REQUIRE(source.next(diagnostics) == 'x');
    REQUIRE(source.columnByIndex(0) == 0);
    REQUIRE(source.numberOfLines() == 1);
}

TEST_CASE("Host expression evaluation preserves postfix results and side effects", "[runtime]") {
    Runtime runtime;
    Context context{ runtime };
    vm::VMState state{ context };
    VM vm{ &state };
    vm.eval("var counter = 0;"_str);
    REQUIRE(*vm.evalExpr<Integer>("counter++"_str) == 0);
    REQUIRE(*vm.evalExpr<Integer>("counter"_str) == 1);
    REQUIRE(*vm.evalExpr<Integer>("counter--"_str) == 1);
    REQUIRE(*vm.evalExpr<Integer>("counter"_str) == 0);
    vm.eval("class A {} var instance = new A();"_str);
    REQUIRE(*vm.evalExpr<Integer>("instance isvalid"_str) == 1);
    vm.eval("invalidate instance;"_str);
    REQUIRE(*vm.evalExpr<Integer>("instance isvalid"_str) == 0);
}

TEST_CASE("Promoting compiled chunks protects metadata when the GC pool is full", "[runtime][gc]") {
    Runtime runtime;
    Context context{ runtime };
    vm::VMState state{ context };
    Common::Result diagnostics;
    Common::SourceFile source;
    source.load(diagnostics, "function answer() { return 42; } var result = answer();");
    syntax::Parser parser{ source, context.pp() };
    auto *node = parser.parse(diagnostics);
    REQUIRE_FALSE(diagnostics.isFailed());
    inter::IRGenerator generator{ diagnostics, runtime, source };
    OptReg ignored;
    auto compiled = generator.parseAst(node, ignored);
    REQUIRE(compiled.has_value());
    auto [used, total] = runtime.markSweep.memoryInfo();
    for(auto remaining = (total - used) / NODE_SIZE; remaining > 0; --remaining)
        runtime.create<Array>(&runtime);
    auto chunk = std::move(*compiled).toHeap(runtime);
    state.allocCallFrame(chunk.get());
    state.makeClosure();
    state.run();
    state.freeCallFrame();
    REQUIRE(state.global("result").asInteger().unwrap() == 42);
    REQUIRE(context.stackTop() == 0);
}

TEST_CASE("Moving call frames retains invocation metadata and transfers register ownership", "[runtime]") {
    vm::FastRegisterPool pool;
    vm::Chunk chunk;
    chunk.setRegCount(1);
    {
        CallFrame frame{ &chunk, {}, pool };
        frame.actualArgc = 7;
        frame.withDepth = 3;
        frame.setThisObj(Value{ String{ std::string(128, 'x') } });
        CallFrame moved{ std::move(frame) };
        REQUIRE(moved.actualArgc == 7);
        REQUIRE(moved.withDepth == 3);
        REQUIRE(moved.thisObj.asString().unwrap()->length() == 128);
        REQUIRE(pool.used() == 1);
    }
    REQUIRE(pool.used() == 0);
}

TEST_CASE("Global proxies remain rooted across repeated collections", "[runtime][gc]") {
    Runtime runtime;
    Context context{ runtime };
    vm::VMState state{ context };
    VM vm{ &state };
    vm.eval("class Proxy { var answer = 42; } var proxy = new Proxy();"_str);
    auto *proxy = static_cast<DataObject *>(state.global("proxy").asObject().unwrap());
    context.global()->proxy = proxy;
    REQUIRE(context.global()->delProp(runtime.atomTable.intern("proxy"_str)));
    for(int i = 0; i < 3; ++i) {
        runtime.markSweep.collect([&] { runtime.collectAllMark(); });
        REQUIRE(proxy->getProp(runtime.atomTable.intern("answer"_str)).asInteger().unwrap() == 42);
    }
}

TEST_CASE("Setter-only properties have no uninitialized getter during GC or access", "[runtime][gc]") {
    Runtime runtime;
    Context context{ runtime };
    vm::VMState state{ context };
    VM vm{ &state };
    vm.eval(R"(
        var received = 0;
        property writeOnly { setter(v) { received = v; } }
        class Receiver {
            property value { setter(v) { received = v; } }
        }
        var receiver = new Receiver();
    )"_str);
    runtime.markSweep.collect([&] { runtime.collectAllMark(); });
    vm.eval("writeOnly = 41; receiver.value = 42;"_str);
    REQUIRE(*vm.evalExpr<Integer>("received"_str) == 42);
    REQUIRE_THROWS(vm.evalExpr<Value>("receiver.value"_str));
}

TEST_CASE("Nested dynamic compilation releases temporary roots before script execution continues", "[runtime][gc]") {
    Runtime runtime;
    Context context{ runtime };
    vm::VMState state{ context };
    VM vm{ &state };
    vm.eval(R"(
        var total = 0;
        for (var i = 0; i < 300; ++i) {
            var callback = "function() { return 1; }"!;
            total += callback();
        }
    )"_str);
    REQUIRE(*vm.evalExpr<Integer>("total"_str) == 300);
    REQUIRE(runtime.handles.empty());
}

TEST_CASE("Source substrings respect byte ranges and embedded NUL characters", "[source]") {
    Common::SourceFile source;
    Common::Result diagnostics;
    source.load(diagnostics, std::string{ "a\0b", 3 });
    REQUIRE(source.substring(0, 3) == std::string{ "a\0b", 3 });
    REQUIRE(source.substring(1, 2) == std::string(1, '\0'));
    REQUIRE(source.substring(3, 3).empty());
    REQUIRE_THROWS_AS(source.substring(2, 1), std::out_of_range);
    REQUIRE_THROWS_AS(source.substring(0, 4), std::out_of_range);
}

TEST_CASE("Empty octets concatenate safely and persist to byte-aligned buffers", "[value]") {
    Octet empty{ nullptr, 0 };
    const u8 bytes[]{ 0xde, 0xad };
    Octet left{ nullptr, 0, bytes, 2 };
    Octet right{ bytes, 2, nullptr, 0 };
    Octet joined{ empty, left };
    REQUIRE(empty.getSize() == 0);
    REQUIRE(left.getSize() == 2);
    REQUIRE(right.getData()[1] == 0xad);
    REQUIRE(joined.getData()[0] == 0xde);
    alignas(u32) u8 buffer[7]{};
    joined.persist(buffer + 1);
    u32 size{};
    std::memcpy(&size, buffer + 1, sizeof(size));
    REQUIRE(size == 2);
    REQUIRE(buffer[5] == 0xde);
    REQUIRE(buffer[6] == 0xad);
}

TEST_CASE("Native exceptions release constructed argument copies and preserve subsequent evaluation", "[runtime]") {
    Runtime runtime;
    Context context{ runtime };
    vm::VMState state{ context };
    VM vm{ &state };
    auto failure = runtime.create<NativeFunction>(
        [](VM *, const Value &, size_t, Value *) -> Value { throw std::runtime_error("native failure"); });
    context.registryGlobalFunc("fail"_str, failure.get());
    vm.eval(R"(
        var caught = 0;
        for (var i = 0; i < 10; ++i) {
            try { fail("first long string argument", "second long string argument"); }
            catch(e) { caught++; }
        }
    )"_str);
    REQUIRE(*vm.evalExpr<Integer>("caught"_str) == 10);
    REQUIRE(context.stackTop() == 0);
}

TEST_CASE("constant container graphs survive compilation and collection", "[runtime][gc]") {
    Runtime runtime{ 8 };
    Context context{ runtime };
    vm::VMState state{ context };
    VM vm{ &state };
    vm.eval(R"(
        function shared() { return (const)[(const)%["nested", (const)[11]]]; }
        var value = shared();
        value[0]["nested"][0] = 42;
    )"_str);
    runtime.markSweep.collect([&] { runtime.collectAllMark(); });
    REQUIRE(*vm.evalExpr<Integer>(R"(shared()[0]["nested"][0])"_str) == 42);
    REQUIRE(runtime.handles.empty());
}

TEST_CASE("Exhausting the rooted GC pool throws instead of aborting", "[runtime][gc]") {
    Runtime rt{ 4 };
    Vec<RootGuard<Array>> keep;
    keep.reserve(4);
    for(int i = 0; i < 4; i++)
        keep.push_back(rt.create<Array>(&rt));
    // 全部节点被句柄持有：收集无法腾出节点，分配以运行期错误呈现
    REQUIRE_THROWS_AS(rt.create<Array>(&rt), std::runtime_error);
    keep.pop_back();
    auto guard = rt.create<Array>(&rt);
    REQUIRE(guard.get() != nullptr);
    const auto [used, total] = rt.markSweep.memoryInfo();
    REQUIRE(total == 4 * NODE_SIZE);
    REQUIRE(used == 4 * NODE_SIZE);
}

TEST_CASE("Pending finalizers extend the pool instead of failing allocation", "[runtime][gc]") {
    struct Baggable final : MarkSweepHeader {
        bool needsFinalization() const noexcept override { return true; }
    };
    Runtime rt{ 8 };
    for(int i = 0; i < 8; i++)
        rt.create<Baggable>(); // 临时句柄立即析构：对象不入根，等待回收
    // 第 9 次分配触发收集：待终结对象标记后仍占满节点 → 扩池 64 而非分配失败
    auto guard = rt.create<Baggable>();
    REQUIRE(guard.get() != nullptr);
    REQUIRE(rt.markSweep.pendingFinalizers.size() == 8);
    const auto [used, total] = rt.markSweep.memoryInfo();
    REQUIRE(total == (8 + 64) * NODE_SIZE);
    REQUIRE(used == 9 * NODE_SIZE);
    auto next = rt.create<Baggable>();
    REQUIRE(next.get() != nullptr);
}

TEST_CASE("Deep nested arrays survive marking across collection", "[runtime][gc]") {
    Runtime rt{ 32768 };
    Context context{ rt };
    vm::VMState state{ context };
    VM vm{ &state };
    vm.eval(R"(
        var deep = [];
        for(var i = 0; i < 20000; i++) deep = [deep];
        var depth = 0;
        var cur = deep;
        while(cur.count > 0) {
            cur = cur[0];
            depth++;
        }
    )"_str);
    REQUIRE(state.global("depth").asInteger().unwrap() == 20000);
    // 标记 2 万层嵌套不栈溢出，且深链在收集中保持完整
    rt.markSweep.collect([&] { rt.collectAllMark(); });
    vm.eval(R"(
        var survived = deep.count;
        var after = 0;
        cur = deep;
        while(cur.count > 0) {
            cur = cur[0];
            after++;
        }
    )"_str);
    REQUIRE(state.global("survived").asInteger().unwrap() == 1);
    REQUIRE(state.global("after").asInteger().unwrap() == 20000);
    REQUIRE(context.stackTop() == 0);
}

TEST_CASE("Allocation churn returns pool usage to baseline after collection", "[runtime][gc]") {
    Runtime rt;
    Context context{ rt };
    stdlib::registerStandardLibrary(context);
    vm::VMState state{ context };
    VM vm{ &state };
    vm.eval(R"(
        function churn() {
            var sink = 0;
            for(var i = 0; i < 64; i++) {
                var a = [i, i * 2];
                var s = "n" + i;
                sink += a[1] + s.length;
            }
            return sink;
        }
    )"_str);
    const auto baseline = *vm.evalExpr<Integer>("churn()"_str);
    // 池未满前垃圾滞留节点，先强制收集取得净基线
    rt.markSweep.collect([&] { rt.collectAllMark(); });
    const auto [used0, total0] = rt.markSweep.memoryInfo();
    for(int round = 0; round < 20; round++)
        REQUIRE(*vm.evalExpr<Integer>("churn()"_str) == baseline);
    rt.markSweep.collect([&] { rt.collectAllMark(); });
    const auto [used, total] = rt.markSweep.memoryInfo();
    // 多轮收集后回到基线：churn 垃圾全部回收，句柄与寄存器不滞留
    REQUIRE(total == total0);
    REQUIRE(used == used0);
    REQUIRE(context.stackTop() == 0);
    REQUIRE(state.getRegPoolTop() == 0);
}
