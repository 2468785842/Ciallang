//
// Created by LiDong on 2026/1/10.
//

#include <catch.hpp>

#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace cial;

TEST_CASE("switch - execution") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    SECTION("empty switch body") {
        vm.eval(R"(
        function testSwitch(x) {
            var r;
            switch (x) {
            }
            return r;
        }
        )"_str);
        REQUIRE(vm.evalExpr<Value>("testSwitch(1)"_str)->isVoid());
    }

    SECTION("depth break") {
        vm.eval(R"(
        function testSwitch(x, y) {
            var r = 0;
            switch (x) {
                case 1:
                    if (y > 10)
                        break;
                    r = 1;
                    break;
            }
            return r;
        }
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("testSwitch(1, 1)"_str) == 1);
        REQUIRE(*vm.evalExpr<Integer>("testSwitch(2, 10)"_str) == 0);
        REQUIRE(*vm.evalExpr<Integer>("testSwitch(1, 10)"_str) == 1);
        REQUIRE(*vm.evalExpr<Integer>("testSwitch(1, 11)"_str) == 0);
    }

    SECTION("single case + default") {
        vm.eval(R"(
        function testSwitch(x) {
            var r;
            switch (x) {
                case 1:
                    r = 1;
                    break;
                default:
                    r = 0;
            }
            return r;
        }
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("testSwitch(1)"_str) == 1);
        REQUIRE(*vm.evalExpr<Integer>("testSwitch(0)"_str) == 0);
    }

    SECTION("multi case + default") {
        vm.eval(R"(
            function testSwitch(x) {
                var r;
                switch (x) {
                    case 1:
                    case 2:
                    case 3:
                        r = 123;
                        break;
                    default:
                        r = 0;
                }
                return r;
            }
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("testSwitch(1)"_str) == 123);
        REQUIRE(*vm.evalExpr<Integer>("testSwitch(2)"_str) == 123);
        REQUIRE(*vm.evalExpr<Integer>("testSwitch(3)"_str) == 123);
        REQUIRE(*vm.evalExpr<Integer>("testSwitch(4)"_str) == 0);
    }

    SECTION("exclude default") {
        vm.eval(R"(
            function testSwitch(x) {
                var r;
                switch (x) {
                    case 1:
                        r = 1;
                        break;
                    case 2:
                        r = 2;
                        break;
                }
                return r;
            }
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("testSwitch(1)"_str) == 1);
        REQUIRE(*vm.evalExpr<Integer>("testSwitch(2)"_str) == 2);
        REQUIRE(vm.evalExpr<Value>("testSwitch(3)"_str)->isVoid());
    }

    SECTION("optional break") {
        vm.eval(R"(
            function testSwitch(x) {
                var r = 0;
                switch (x) {
                    case 1:
                        r += 1;
                    case 2:
                        r += 2;
                        break;
                    default:
                        r = 0;
                }
                return r;
            }
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("testSwitch(1)"_str) == 3);
        REQUIRE(*vm.evalExpr<Integer>("testSwitch(2)"_str) == 2);
        REQUIRE(*vm.evalExpr<Integer>("testSwitch(3)"_str) == 0);
    }

    SECTION("middle break") {
        vm.eval(R"(
            function testSwitch(x) {
                var r;
                switch (x) {
                    case 1:
                        r = 1;
                        break;
                        r = 999;
                    default:
                        r = 0;
                }
                return r;
            }
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("testSwitch(1)"_str) == 1);
        REQUIRE(*vm.evalExpr<Integer>("testSwitch(0)"_str) == 0);
    }

    SECTION("nested switch") {
        vm.eval(R"(
            function testSwitch(x, y) {
                var r;
                switch (x) {
                    case 1:
                        switch (y) {
                            case 10:
                                r = 110;
                                break;
                            case 20:
                                r = 111;
                        }
                        break;
                    default:
                        r = 0;
                }
                return r;
            }
        )"_str);

        REQUIRE(*vm.evalExpr<Integer>("testSwitch(1, 10)"_str) == 110);
        REQUIRE(*vm.evalExpr<Integer>("testSwitch(1, 20)"_str) == 111);
        REQUIRE(vm.evalExpr<Value>("testSwitch(1, 30)"_str)->isVoid());
        REQUIRE(*vm.evalExpr<Integer>("testSwitch(0)"_str) == 0);
    }

    SECTION("function call expr") {
        vm.eval(R"(
            function testSwitch(f, g, h) {
                var r;
                switch (f()) {
                    case g():
                        r = 1;
                        break;
                    case h():
                        r = 2;
                        break;
                }
                return r;
            }
        )"_str);

        REQUIRE(*vm.evalExpr<Integer>("testSwitch("
                                      "function { return true; },"
                                      "function { return true; },"
                                      "function { return true; }"
                                      ")"_str) == 1);
        REQUIRE(*vm.evalExpr<Integer>("testSwitch("
                                      "function { return true; },"
                                      "function { return false; },"
                                      "function { return true; }"
                                      ")"_str) == 2);
        REQUIRE(vm.evalExpr<Value>("testSwitch("
                                   "function { return false; },"
                                   "function { return true; },"
                                   "function { return true; }"
                                   ")"_str)
                    ->isVoid());
        REQUIRE(*vm.evalExpr<Integer>("testSwitch("
                                      "function { return false; },"
                                      "function { return false; },"
                                      "function { return true; }"
                                      ")"_str) == 1);
    }
}