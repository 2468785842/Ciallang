//
// Created by LiDong on 2026/1/14.
//

#include <catch2/catch_all.hpp>

#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace cial;

TEST_CASE("preprocessor basic @if/@endif") {
    Runtime rt{};
    Context context{ rt };
    context.pp().setVar("version"_str, 0x02040009);
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        @if (version >= 0x02040009)
            var a = "version >= 0x02040009";
        @endif
    )"_str);

    REQUIRE(**vm.evalExpr<String *>("a"_str) == "version >= 0x02040009"_str);
}

TEST_CASE("preprocessor nested @if") {
    Runtime rt{};
    Context context{ rt };
    context.pp().setVar("version"_str, 0x02040009);
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        var c;
        @if(version >= 0x02040009)
            @if(0)
                c = "should not appear";
            @endif
            var d = "nested if works";
        @endif
    )"_str);

    REQUIRE(vm.evalExpr<Value>("c"_str)->isVoid()); // 不应该被定义
    REQUIRE(**vm.evalExpr<String *>("d"_str) == "nested if works"_str);
}

TEST_CASE("preprocessor integer boundaries and invalid directives", "[fuzz][preprocessor]") {
    PreProcessor pp;
    pp.onSet("a=2147483647+1"_str);
    REQUIRE(pp.getVar("a"_str) == -2147483647 - 1);
    pp.onSet("a=-2147483648/-1"_str);
    REQUIRE(pp.getVar("a"_str) == -2147483647 - 1);
    pp.onSet("a=-2147483648%-1"_str);
    REQUIRE(pp.getVar("a"_str) == 0);
    pp.onSet("a=0xffffffff+2"_str);
    REQUIRE(pp.getVar("a"_str) == 1);
    pp.onSet("a=2147483647*2147483647"_str);
    REQUIRE(pp.getVar("a"_str) == 1);
    REQUIRE_THROWS_AS(pp.onSet("a=1%0"_str), std::runtime_error);
    REQUIRE_THROWS_AS(pp.onSet("a=1/0"_str), std::runtime_error);
    for(const auto &input : { "@set(x=1/0)", "@set(x=1%0)", "@if(0x)", "@set(a=)", "class 0 { function f() {} }",
                              "class A { function 0() {} }", "property foo {" }) {
        Common::SourceFile source;
        Common::Result result;
        source.load(result, input);
        syntax::Parser parser{ source, pp };
        REQUIRE_NOTHROW(parser.parse(result));
        REQUIRE(result.isFailed());
    }
}
