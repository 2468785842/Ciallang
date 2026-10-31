/*
 * Copyright (c) 2024/12/18
 *
 * 宿主 VM API 的跨编译单元状态保留；纯脚本集成场景见 tests/tjs/integration/。
 */

#include <catch2/catch_all.hpp>
#include "stdlib/StandardLibrary.hpp"

#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace cial;

TEST_CASE("集成 - 同一 VM 跨脚本共享全局与函数") {
    Runtime rt{};
    Context context{ rt };
    stdlib::registerStandardLibrary(context);
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval("var acc = 0; function add(n) { acc += n; return acc; }"_str);
    vm.eval("add(2);"_str);
    vm.eval("add(3);"_str);

    REQUIRE(*vm.evalExpr<Integer>("acc"_str) == 5);
    REQUIRE(*vm.evalExpr<Integer>("add(4)"_str) == 9);
}
