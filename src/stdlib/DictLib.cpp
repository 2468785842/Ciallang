// Copyright (c) 2026/10/24
//

#include "DictLib.hpp"
#include "runtime/Context.hpp"

#include "types/Dictionary.hpp"
#include "vm/VM.hpp"

namespace cial::stdlib {
    void registerDictLib(Context &context) {
        context.nativeType<Dictionary>().method("assign"_str, &stdlib::dictAssign);
        context.nativeType<Dictionary>().method("clear"_str, &stdlib::dictClear);
        context.nativeType<Dictionary>().method("erase"_str, &stdlib::dictErase);
        // count 属性
        context.nativeType<Dictionary>().property("count"_str, [](const Value &thisObj) {
            return Value{ Integer{
                static_cast<Integer>(dynamic_cast<Dictionary &>(*thisObj.asObject().unwrap()).members.size()) } };
        });
        context.registerClass<Dictionary>("Dictionary"_str);
    }

    namespace {
        Dictionary *asDict(const Value &thisObj) {
            // typeId 直判替代 dynamic_cast（Dictionary 为 final，二者等价），见 asArray 同注
            auto *obj = thisObj.asObject().unwrap();
            return obj->typeId() == TypeId::Dictionary ? static_cast<Dictionary *>(obj) : nullptr;
        }

        // 键统一字符串化（Atom 化）
        Atom keyToAtom(VM *vm, const Value &key) {
            if(key.isString())
                return vm->rt().atomTable.intern(*key.asString().unwrap());
            Value s = key;
            if(const auto r = s.toString(); r.isFailed())
                throw std::runtime_error("dictionary key must be convertible to string");
            return vm->rt().atomTable.intern(*s.asString().unwrap());
        }
    } // namespace

    void dictAssign(VM *, const Value &thisObj, const size_t argCount, Value *args) {
        if(argCount < 1)
            throw std::runtime_error("Dictionary.assign expects a dictionary");
        auto *dst = asDict(thisObj);
        auto *src = dynamic_cast<Dictionary *>(args[0].asObject().unwrap());
        if(!src)
            throw std::runtime_error("Dictionary.assign expects a dictionary");
        dst->members = src->members;
    }

    void dictClear(VM *, const Value &thisObj, size_t) { asDict(thisObj)->members.clear(); }

    Value dictErase(VM *vm, const Value &thisObj, const size_t argCount, Value *args) {
        if(argCount < 1)
            throw std::runtime_error("Dictionary.erase expects a key");
        auto *dict = asDict(thisObj);
        return Value{ Integer{ dict->members.erase(keyToAtom(vm, args[0])) > 0 } };
    }

} // namespace cial::stdlib
