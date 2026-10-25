// Copyright (c) 2026/10/24
//
// 动态类型标识：NativeRegister 原生方法表的分派键 + Value 运行时类型判别。
// 值类型（Integer/Real/Octet/String）按 Value 自身 tag 判别；
// 对象类型经 Object::typeId() 虚调用取得（替代热路径上的 dynamic_cast 链）。
// 独立成头以打破 Object.hpp ↔ NativeRegister.hpp 的循环依赖。
#pragma once

namespace cial {

    enum class TypeId {
        None,
        Integer,
        Real,
        Object,
        Octet,
        String,
        Array,
        Dictionary,
        RegExp,
        Math,
        // 以下不进 NativeRegister 方法表，仅作 GProp/DProp 的分派标识
        //（替代热路径上的 dynamic_cast）
        Global,
        Class,
    };

} // namespace cial
