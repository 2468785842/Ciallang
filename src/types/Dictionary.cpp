// Copyright (c) 2026/10/24
//

#include "Dictionary.hpp"

#include "vm/VMState.hpp"

namespace cial {

    // 类对象构造入口：Dictionary() / new Dictionary()（TJS2 构造器为空，参数被忽略）
    void Dictionary::call(vm::VMState &vmState, const u32 ret, size_t) {
        vmState.regRef(ret) = Value{ vmState.rt.create<Dictionary>().get() };
    }

} // namespace cial
