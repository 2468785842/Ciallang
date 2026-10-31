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

#include <regex>
#include "common/UTF8.hpp"
#include "runtime/Context.hpp"

#include "RegExpLib.hpp"
#include "StringLib.hpp"
#include "types/RegExp.hpp"
#include "vm/VM.hpp"

// TODO: UTF-8 support
// TODO: length property support
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
            // 字符串长度按 UTF-8 码点计数。
            .property("length"_str, [](const Value &thisObj) {
                // TJS2 语义：字符数（UTF-16 码元数）；UTF-8 下按码点计数（"abc漢字" == 5）
                const String *str = thisObj.asString().unwrap();
                return Value{ Integer{ Common::utf8Strlen(std::string(str->getData(), str->length())) } };
            });
    }


    String stringCharAt(VM *, const Value &thisObj, size_t, const Integer index) {
        const String *str = thisObj.asString().unwrap();
        if(str->isEmpty() || index < 0 || index >= str->length())
            return ""_str;
        return String(str->getData()[index]);
    }

    Integer stringIndexOf(VM *, const Value &thisObj, size_t, const String *subStr, const Value &vStart) {
        const String *str = thisObj.asString().unwrap();
        if(str->isEmpty())
            return -1;

        Integer start{ 0 };

        if(str->isEmpty() || subStr->isEmpty())
            return -1;
        if(!vStart.isVoid())
            start = vStart.asInteger().unwrap();

        if(start < 0 || start >= str->length()) {
            return -1;
        }

        const char *p = std::strstr(str->getData() + start, subStr->getData());
        if(!p)
            return -1;
        return p - str->getData();
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
        // TODO: UTF-8 support
        const String *str = thisObj.asString().unwrap();
        const Integer sLen = str->length();

        if(start < 0 || start >= sLen) {
            return ""_str;
        }

        if(!vLength.isVoid()) {
            Integer count = vLength.asInteger().unwrap();
            if(count < 0) {
                return ""_str;
            }
            if(count > sLen - start)
                count = sLen - start;
            return String(str->getData() + start, count);
        }

        return String(str->getData() + start, sLen - start);
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
        std::string result;
        result.reserve(str->length());
        const char *data = str->getData();
        size_t end = str->length();
        while(end) {
            size_t begin = end - 1;
            while(begin && (static_cast<unsigned char>(data[begin]) & 0xc0) == 0x80)
                --begin;
            result.append(data + begin, end - begin);
            end = begin;
        }
        return String{ result };
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
