// Copyright (c) 2026/10/23
//
// Exception 内建类：以 TJS 源码自举（kirikiri 原版的 Exception 同样由 TJS 定义），
// 宿主仅负责在全局上下文 eval 这段源码。
#pragma once

#include "types/Types.hpp"
#include "vm/VM.hpp"

namespace cial::stdlib {

    // 在全局上下文注册 Exception 类（需在脚本执行前调用一次）。
    // TJS2 语义：message 成员，构造器缺省为 "Exception"
    inline void initExceptionClass(VM &vm) {
        vm.eval(R"(class Exception {
    var message;
    function Exception(message = "Exception") {
        this.message = message;
    }
}
)"_str);
    }

} // namespace cial::stdlib
