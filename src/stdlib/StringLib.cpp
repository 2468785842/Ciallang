// Copyright (c) 2026/1/5 16:33
//
// /\  _` \   __          /\_ \  /\_ \
// \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
//  \ \ \/_/_\/\ \  /'__`\  \ \ \  \ \ \   /'__`\  /' _ `\  /'_ `\
//   \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
//    \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
//     \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
//                                                            /\____
//                                                            \_/__/
//

#include <algorithm>
#include <limits>
#include "common/UTF16.hpp"
#include "common/UTF8.hpp"
#include "runtime/Context.hpp"

#include "RegExpLib.hpp"
#include "StringLib.hpp"
#include "types/RegExp.hpp"
#include "vm/VM.hpp"

namespace cial::stdlib {
    void registerStringLib(Context &context) {
        context.nativeType<String>()
            .method("charAt"_str, &stdlib::stringCharAt)
            .method("indexOf"_str, &stdlib::stringIndexOf)
            .method("toUpperCase"_str, &stdlib::stringToUpperCase)
            .method("toLowerCase"_str, &stdlib::stringToLowerCase)
            .method("substring"_str, &stdlib::stringSubstring)
            .method("substr"_str, &stdlib::stringSubstring)
            .method("sprintf"_str, &stdlib::stringSprintf)
            .method("replace"_str, &stdlib::stringReplace)
            .method("split"_str, &stdlib::stringSplitWith)
            .method("escape"_str, &stdlib::stringEscape)
            .method("trim"_str, &stdlib::stringTrim)
            .method("reverse"_str, &stdlib::stringReverse)
            .method("repeat"_str, &stdlib::stringRepeat)
            .property("length"_str, [](const Value &thisObj) {
                const auto *str = thisObj.asString().unwrap();
                return Value{ Integer{ static_cast<Integer>(Common::utf16Length({ str->getData(), str->length() })) } };
            });
    }


    String stringCharAt(VM *, const Value &thisObj, size_t, const Integer index) {
        const String *str = thisObj.asString().unwrap();
        if(index < 0 || static_cast<u64>(index) >= Common::utf16Length({ str->getData(), str->length() }))
            return ""_str;
        return String{ Common::utf16Slice({ str->getData(), str->length() }, static_cast<size_t>(index), 1) };
    }

    Integer stringIndexOf(VM *, const Value &thisObj, size_t, const String *subStr, const Value &vStart) {
        const String *str = thisObj.asString().unwrap();
        const Integer start = vStart.isVoid() ? 0 : vStart.asInteger().unwrap();
        if(start < 0 || str->isEmpty() || subStr->isEmpty())
            return -1;
        const auto units = Common::toUtf16({ str->getData(), str->length() });
        const auto needle = Common::toUtf16({ subStr->getData(), subStr->length() });
        if(static_cast<u64>(start) >= units.size())
            return -1;
        const auto found = units.find(needle, static_cast<size_t>(start));
        return found == std::u16string::npos ? -1 : static_cast<Integer>(found);
    }

    String stringToUpperCase(VM *, const Value &thisObj, size_t) {
        const String *str = thisObj.asString().unwrap();
        std::string out(str->getData(), str->length());
        for(char &c : out) {
            if(c >= 'a' && c <= 'z')
                c += 'Z' - 'z';
        }
        return String(out.c_str(), static_cast<u32>(out.size()));
    }

    String stringToLowerCase(VM *, const Value &thisObj, size_t) {
        const String *str = thisObj.asString().unwrap();
        std::string out(str->getData(), str->length());
        for(char &c : out) {
            if(c >= 'A' && c <= 'Z')
                c += 'z' - 'Z';
        }
        return String(out.c_str(), static_cast<u32>(out.size()));
    }

    String stringSubstring(VM *, const Value &thisObj, size_t, const Integer start, const Value &vLength) {
        const String *str = thisObj.asString().unwrap();
        if(start < 0 || static_cast<u64>(start) >= Common::utf16Length({ str->getData(), str->length() }))
            return ""_str;
        const Integer count = vLength.isVoid() ? std::numeric_limits<Integer>::max() : vLength.asInteger().unwrap();
        if(count <= 0)
            return ""_str;
        return String{ Common::utf16Slice({ str->getData(), str->length() }, static_cast<size_t>(start),
                                          static_cast<size_t>(count)) };
    }

    String stringSprintf(VM *, const Value &thisObj, const size_t argCount, Value *args) {
        return formatString(*thisObj.asString().unwrap(), argCount, args);
    }

    // TJS2 语义：委托 pattern.replace(target, replacement)——g 标志全局替换、否则仅第一处；
    // replacement 可为函数（回调收到匹配数组，返回替换串，见 regexpReplaceIn）
    String stringReplace(VM *vm, const Value &thisObj, size_t, const Value &reVal, const Value &replacement) {
        const String *str = thisObj.asString().unwrap();
        if(!reVal.isObject())
            throw std::runtime_error("replace expects a regexp");
        const auto *re = dynamic_cast<const RegExp *>(reVal.asObject().unwrap());
        if(!re)
            throw std::runtime_error("replace expects a regexp");
        return regexpReplaceIn(vm, re, *str, replacement);
    }

    String stringEscape(VM *, const Value &thisObj, size_t) { return thisObj.asString().unwrap()->escapeBackSlash(); }

    // split(pattern, reserved, purgeempty)：实现在 RegExpLib（正则/分隔字符集双模式）

    // trim()
    String stringTrim(VM *, const Value &thisObj, size_t) {
        const String *str = thisObj.asString().unwrap();
        const char *s = str->getData();
        const size_t len = str->length();

        size_t start = 0;
        while(start < len && static_cast<unsigned char>(s[start]) <= 0x20)
            ++start;

        size_t end = len;
        while(end > start && static_cast<unsigned char>(s[end - 1]) <= 0x20)
            --end;

        return String(s + start, end - start);
    }

    // reverse()
    String stringReverse(VM *, const Value &thisObj, size_t) {
        // TJS2 tjsInterCodeExec.cpp:2603：返回独立结果，不修改接收者。
        const auto *str = thisObj.asString().unwrap();
        auto units = Common::toUtf16({ str->getData(), str->length() });
        std::reverse(units.begin(), units.end());
        return String{ Common::fromUtf16(units) };
    }

    // repeat(count)
    String stringRepeat(VM *, const Value &thisObj, size_t, const Integer count) {
        const String *str = thisObj.asString().unwrap();
        if(count <= 0 || str->isEmpty())
            return ""_str;

        std::string result;
        result.reserve(str->length() * count);
        for(Integer i = 0; i < count; ++i)
            result.append(str->getData(), str->length());

        return String(result.c_str(), static_cast<u32>(result.size()));
    }

} // namespace cial::stdlib
