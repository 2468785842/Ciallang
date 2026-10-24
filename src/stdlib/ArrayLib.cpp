// Copyright (c) 2026/10/24
//

#include "ArrayLib.hpp"

#include "types/Array.hpp"
#include "types/TypeConverter.hpp"

namespace cial::stdlib {

    namespace {
        Array *asArray(const Value &thisObj) {
            return dynamic_cast<Array *>(thisObj.asObject().unwrap());
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

    void arrayErase(VM *, const Value &thisObj, size_t) {
        asArray(thisObj)->elements.clear();
    }

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

} // namespace cial::stdlib
