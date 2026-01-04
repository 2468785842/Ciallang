/*
 * Copyright (c) 2026/1/4
 *
 * 本地函数单元测试
 */

#include <catch.hpp>

#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace Cial;

String stringCharAt(VM *, const Value &thisObj, const Integer index) {
    const String *str = thisObj.toString();
    if(str->isEmpty() || index < 0 || index >= str->length())
        return ""_str;
    return String(str->getData()[index]);
}

TEST_CASE("本地函数声明 - 简单函数定义") {
    Runtime rt{};
    Context context{ rt };
    context.registerMethod<String>("charAt"_str, &stringCharAt, false);
    Bytecode::VMState vmState{ context };
    const VM vm{ &vmState };
    REQUIRE(**vm.evalExpr<String *>(R"("123".charAt(2))"_str) == "3"_str);
    REQUIRE(**vm.evalExpr<String *>(R"("cab".charAt(0))"_str) == "c"_str);
    REQUIRE(**vm.evalExpr<String *>(R"("cab".charAt(3))"_str) == ""_str);
}