#include <catch.hpp>

#include "types/Value.hpp"
#include "vm/VM.hpp"

using namespace cial;

TEST_CASE("Value short strings survive copies, moves, growth and register reuse", "[value]") {
    for(const size_t length : { 0, 1, 14, 15, 16, 20, 21, 22, 256 }) {
        const std::string text(length, 'x');
        Value original{ String{ text } };
        REQUIRE(original.isInlineString() == (length <= String::SHORT_STR_LEN));
        Value copy = original;
        Value moved = std::move(copy);
        REQUIRE(copy.isVoid());
        REQUIRE(moved.asString().unwrap()->toStdStr() == text);
        copy = moved;
        moved = Value{ Integer{ 42 } };
        REQUIRE(copy.asString().unwrap()->toStdStr() == text);
        moved = std::move(copy);
        REQUIRE(copy.isVoid());
        REQUIRE(moved.asString().unwrap()->toStdStr() == text);
        moved = moved;
        REQUIRE(moved.asString().unwrap()->toStdStr() == text);
    }
    Value small{ "abc"_str };
    Value independent = small;
    independent.asString().unwrap()->append("abcdefghijklmnop"_str);
    REQUIRE(*small.asString().unwrap() == "abc"_str);
    Value grown = independent;
    REQUIRE(grown.asString().unwrap()->length() == 19);
    String reserved{ "short"_str };
    reserved.reserveExact(100);
    String moved{ std::move(reserved) };
    REQUIRE(moved == "short"_str);
    REQUIRE(reserved.isEmpty());
    moved.append(moved);
    REQUIRE(moved == "shortshort"_str);
}

TEST_CASE("Short string handles remain valid across evaluations", "[value]") {
    Runtime rt;
    Context context{ rt };
    vm::VMState state{ context };
    VM vm{ &state };
    auto first = vm.evalExpr<String *>(R"("hello")"_str);
    for(int i = 0; i < 20; ++i)
        vm.evalExpr(R"("world")"_str);
    REQUIRE(**first == "hello"_str);
}
