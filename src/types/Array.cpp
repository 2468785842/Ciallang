// Copyright (c) 2026/10/24
//

#include "Array.hpp"

#include "vm/VMState.hpp"

#include "runtime/Runtime.hpp"

namespace cial {

    bool Array::isCountAtom(const Atom a) const {
        // WellKnownAtoms 构造期驻留：比较零哈希（旧实现每次 intern ×2）
        return a == rt->wkAtoms.count || a == rt->wkAtoms.length;
    }

    // 类对象构造入口：Array() / new Array()（TJS2 构造器为空——参数被忽略，
    // 无 JS 的 Array(n) 尺寸语义；语料惯用 [] 字面量）
    void Array::call(vm::VMState &vmState, const u32 ret, size_t) {
        vmState.regRef(ret) = Value{ vmState.rt.create<Array>(&vmState.rt).get() };
    }

    // count/length 属性可写（TJS2 语义）：调大补 void、调小截断；其余命名成员当前不支持
    void Array::setProp(const Atom a, const Value &v) {
        if(isCountAtom(a)) {
            const auto n = static_cast<size_t>(v.isVoid() ? 0 : v.asInteger().unwrap());
            elements.resize(n);
            return;
        }
        throw std::runtime_error("Array does not support named member");
    }

    Value Array::getProp(const Atom a) {
        if(isCountAtom(a)) {
            return Value{ Integer{ static_cast<Integer>(elements.size()) } };
        }
        throw std::runtime_error("Array does not support named member");
    }

    bool Array::hasProp(const Atom a) const { return isCountAtom(a); }

    bool Array::delProp(const Atom) { return false; }

} // namespace cial
