// Copyright (c) 2026/10/24
//
// VM 内部类型标识；NativeRegister 使用独立 NativeTypeKey，内置类型复用此编号。
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
        Date,
        RandomGen,
        Scripts,
        // 以下不进 NativeRegister 方法表，仅作 GProp/DProp 的分派标识
        // （替代热路径上的 dynamic_cast）
        Global,
        Class,
        // 脚本函数/原生函数/属性闭包专有标识（CallV 直写路径与
        // loadGlobal/propObjectGet·Set 等热路径用一次虚调用替代 dynamic_cast）
        Function,
        NativeFunction,
        Property,
    };

} // namespace cial
