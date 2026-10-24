// Copyright (c) 2026/10/24
//

#include "Array.hpp"

#include "runtime/Runtime.hpp"

namespace cial {

    bool Array::isCountAtom(const Atom a) const {
        const Atom count = rt->atomTable.intern("count"_str);
        const Atom length = rt->atomTable.intern("length"_str);
        return a == count || a == length;
    }

    void Array::call(vm::VMState &, u32, size_t) {
        throw std::runtime_error("Not implemented Array call");
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
