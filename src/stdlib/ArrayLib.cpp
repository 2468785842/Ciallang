// Copyright (c) 2026/10/24
//

#include <algorithm>
#include <cstring>
#include "RegExpLib.hpp"
#include "runtime/Context.hpp"

#include "ArrayLib.hpp"

#include "types/Array.hpp"
#include "types/TypeConverter.hpp"
#include "vm/VM.hpp"

namespace cial::stdlib {
    void registerArrayLib(Context &context) {
        context.registerMethod<Array>("push"_str, &stdlib::arrayPush);
        context.registerMethod<Array>("pop"_str, &stdlib::arrayPop);
        context.registerMethod<Array>("unshift"_str, &stdlib::arrayUnshift);
        context.registerMethod<Array>("shift"_str, &stdlib::arrayShift);
        context.registerMethod<Array>("insert"_str, &stdlib::arrayInsert);
        context.registerMethod<Array>("remove"_str, &stdlib::arrayRemove);
        context.registerMethod<Array>("erase"_str, &stdlib::arrayErase);
        context.registerMethod<Array>("reverse"_str, &stdlib::arrayReverse);
        context.registerMethod<Array>("join"_str, &stdlib::arrayJoin);
        context.registerMethod<Array>("split"_str, &stdlib::arraySplitWith);
        context.registerMethod<Array>("sort"_str, &stdlib::arraySort);
        // count/length 属性（get；写走 Array::setProp）
        const auto arrayCount = [](const Value &thisObj) {
            return Value{ Integer{
                static_cast<Integer>(dynamic_cast<Array &>(*thisObj.asObject().unwrap()).elements.size()) } };
        };
        context.registerProperty<Array>("count"_str, arrayCount);
        context.registerProperty<Array>("length"_str, arrayCount);
        context.registerClass<Array>("Array"_str, &context.rt());
    }

    namespace {
        Array *asArray(const Value &thisObj) {
            // typeId 直判替代 dynamic_cast（Array 为 final，二者等价）：
            // 原生方法热路径每次调用省一次 libc++abi RTTI 遍历
            auto *obj = thisObj.asObject().unwrap();
            return obj->typeId() == TypeId::Array ? static_cast<Array *>(obj) : nullptr;
        }

        // 数组元素 → 字符串（join 用）；TJS2 的字符串化语义
        String elementToString(const Value &v) {
            if(v.isString())
                return *v.asString().unwrap();
            if(v.isVoid())
                return ""_str;
            if(v.isInteger())
                return String(std::to_string(v.asIntegerUnsafe()).c_str());
            if(v.isObject())
                return "<object>"_str;
            Value tmp = v;
            if(const auto r = tmp.toString(); !r.isFailed() && tmp.isString())
                return *tmp.asString().unwrap();
            return ""_str;
        }
    } // namespace

    Value arrayPush(VM *, const Value &thisObj, const size_t argCount, Value *args) {
        auto *arr = asArray(thisObj);
        for(size_t i = 0; i < argCount; ++i)
            arr->elements.push_back(args[i]);
        return Value{ Integer{ static_cast<Integer>(arr->elements.size()) } };
    }

    Value arrayPop(VM *, const Value &thisObj, size_t) {
        auto *arr = asArray(thisObj);
        if(arr->elements.empty())
            return Value{};
        Value back = arr->elements.back();
        arr->elements.pop_back();
        return back;
    }

    Value arrayUnshift(VM *, const Value &thisObj, const size_t argCount, Value *args) {
        auto *arr = asArray(thisObj);
        for(size_t i = 0; i < argCount; ++i)
            arr->elements.insert(arr->elements.begin() + static_cast<i64>(i), args[i]);
        return Value{ Integer{ static_cast<Integer>(arr->elements.size()) } };
    }

    Value arrayShift(VM *, const Value &thisObj, size_t) {
        auto *arr = asArray(thisObj);
        if(arr->elements.empty())
            return Value{};
        Value front = arr->elements.front();
        arr->elements.erase(arr->elements.begin());
        return front;
    }

    Value arrayInsert(VM *, const Value &thisObj, const size_t argCount, Value *args) {
        auto *arr = asArray(thisObj);
        if(argCount < 1)
            throw std::runtime_error("Array.insert expects an index");
        const auto idx = static_cast<i64>(args[0].isVoid() ? 0 : args[0].asInteger().unwrap());
        auto pos = idx < 0 ? arr->elements.size() : static_cast<size_t>(idx);
        if(pos > arr->elements.size())
            pos = arr->elements.size();
        for(size_t i = 1; i < argCount; ++i)
            arr->elements.insert(arr->elements.begin() + static_cast<i64>(pos + i - 1), args[i]);
        return Value{ Integer{ static_cast<Integer>(arr->elements.size()) } };
    }

    Value arrayRemove(VM *, const Value &thisObj, const size_t argCount, Value *args) {
        auto *arr = asArray(thisObj);
        if(argCount < 1)
            throw std::runtime_error("Array.remove expects an index");
        const auto idx = static_cast<i64>(args[0].isVoid() ? 0 : args[0].asInteger().unwrap());
        const auto n = static_cast<i64>(arr->elements.size());
        if(idx < -n || idx >= n)
            throw std::runtime_error("Array.remove: index out of range");
        const size_t pos = idx < 0 ? static_cast<size_t>(n + idx) : static_cast<size_t>(idx);
        Value removed = arr->elements[pos];
        arr->elements.erase(arr->elements.begin() + static_cast<i64>(pos));
        return removed;
    }

    void arrayErase(VM *, const Value &thisObj, size_t) { asArray(thisObj)->elements.clear(); }

    void arrayReverse(VM *, const Value &thisObj, size_t) {
        auto *arr = asArray(thisObj);
        std::reverse(arr->elements.begin(), arr->elements.end());
    }

    Value arrayJoin(VM *, const Value &thisObj, const size_t argCount, Value *args) {
        auto *arr = asArray(thisObj);
        String delim = argCount >= 1 && args[0].isString() ? *args[0].asString().unwrap() : String(","_str);
        std::string out;
        for(size_t i = 0; i < arr->elements.size(); ++i) {
            if(i > 0)
                out.append(delim.getData(), delim.length());
            const String s = elementToString(arr->elements[i]);
            out.append(s.getData(), s.length());
        }
        return Value{ String(out.c_str(), static_cast<u32>(out.size())) };
    }

    // TJS2 sort（tjsArray.cpp）：参数0 为排序字母 '+'（默认升序）'-'（降序）
    // '0'/'9'（数值升/降）'a'/'z'（字符串升/降）或比较函数对象（回调返回真值 → lhs 在前）；
    // 参数1 为是否稳定排序；无返回值（原地排序）
    Value arraySort(VM *vm, const Value &thisObj, const size_t argCount, Value *args) {
        auto *arr = asArray(thisObj);
        char method = '+';
        bool stable = false;
        Value comparator{};

        if(argCount >= 1 && !args[0].isVoid()) {
            if(args[0].isObject()) {
                comparator = args[0];
                method = 0;
            } else {
                const String m = elementToString(args[0]);
                method = m.isEmpty() ? '+' : m.getData()[0];
                switch(method) {
                    case '+':
                    case '-':
                    case '0':
                    case '9':
                    case 'a':
                    case 'z':
                        break;
                    default:
                        method = '+';
                        break;
                }
            }
        }
        if(argCount >= 2 && !args[1].isVoid())
            stable = args[1].asBool();

        auto &items = arr->elements;
        const auto normalAsc = [](const Value &l, const Value &r) { return l.littlerThan(r).unwrap(); };
        const auto normalDesc = [](const Value &l, const Value &r) { return l.greaterThan(r).unwrap(); };
        // 数值序：双字符串先转数值再比（TJS2 tonumber 语义），其余按常规序
        const auto numAsc = [](const Value &l, const Value &r) {
            if(l.isString() && r.isString()) {
                const auto lr = l.asReal(), rr = r.asReal();
                if(!lr.isFailed() && !rr.isFailed())
                    return lr.value() < rr.value();
            }
            return l.littlerThan(r).unwrap();
        };
        const auto numDesc = [](const Value &l, const Value &r) {
            if(l.isString() && r.isString()) {
                const auto lr = l.asReal(), rr = r.asReal();
                if(!lr.isFailed() && !rr.isFailed())
                    return lr.value() > rr.value();
            }
            return l.greaterThan(r).unwrap();
        };
        // 字符串序：非字符串对按字符串化后比较（memcmp 字典序，String 无 operator<）
        const auto strCmp = [](const String &a, const String &b) {
            const size_t n = std::min(a.length(), b.length());
            const int c = n ? std::memcmp(a.getData(), b.getData(), n) : 0;
            if(c != 0)
                return c;
            return a.length() < b.length() ? -1 : a.length() > b.length() ? 1 : 0;
        };
        const auto strAsc = [&](const Value &l, const Value &r) {
            if(l.isString() && r.isString())
                return l.littlerThan(r).unwrap();
            return strCmp(elementToString(l), elementToString(r)) < 0;
        };
        const auto strDesc = [&](const Value &l, const Value &r) {
            if(l.isString() && r.isString())
                return l.greaterThan(r).unwrap();
            return strCmp(elementToString(r), elementToString(l)) < 0;
        };
        // 比较函数：回调返回真值 → lhs 在前（TJS2 tTJSArraySortCompare_Functional）
        const auto functional = [vm, &comparator](const Value &l, const Value &r) {
            const Value params[2] = { l, r };
            return vm->callValue(comparator, 2, params).asBool();
        };

        switch(method) {
            case '+':
                stable ? std::stable_sort(items.begin(), items.end(), normalAsc)
                       : std::sort(items.begin(), items.end(), normalAsc);
                break;
            case '-':
                stable ? std::stable_sort(items.begin(), items.end(), normalDesc)
                       : std::sort(items.begin(), items.end(), normalDesc);
                break;
            case '0':
                stable ? std::stable_sort(items.begin(), items.end(), numAsc)
                       : std::sort(items.begin(), items.end(), numAsc);
                break;
            case '9':
                stable ? std::stable_sort(items.begin(), items.end(), numDesc)
                       : std::sort(items.begin(), items.end(), numDesc);
                break;
            case 'a':
                stable ? std::stable_sort(items.begin(), items.end(), strAsc)
                       : std::sort(items.begin(), items.end(), strAsc);
                break;
            case 'z':
                stable ? std::stable_sort(items.begin(), items.end(), strDesc)
                       : std::sort(items.begin(), items.end(), strDesc);
                break;
            case 0:
                stable ? std::stable_sort(items.begin(), items.end(), functional)
                       : std::sort(items.begin(), items.end(), functional);
                break;
        }
        return Value{};
    }

} // namespace cial::stdlib
