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

#include "StringLib.hpp"
#include "types/RegExp.hpp"
#include "vm/VM.hpp"

// TODO: UTF-8 support
// TODO: length property support
namespace cial::stdlib {

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

        if(start >= str->length()) {
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
            if(start + count > sLen)
                count = sLen - start;
            return String(str->getData() + start, count);
        }

        return String(str->getData() + start, sLen - start);
    }

    String stringSprintf(VM *, const Value &thisObj, const size_t argCount, Value *args) {
        return formatString(*thisObj.asString().unwrap(), argCount, args);
    }

    String stringReplace(VM *, const Value &thisObj, size_t, const Value &reVal, const Value &replacement) {
        const String *str = thisObj.asString().unwrap();
        auto *re = dynamic_cast<const RegExp *>(reVal.asObject().unwrap());
        if(!re)
            throw std::runtime_error("replace expects a regexp");
        const std::string in(str->getData(), str->length());
        std::smatch m;
        if(!std::regex_search(in, m, re->re))
            return *str; // 无匹配原样返回
        String rep{};
        if(replacement.isString()) {
            rep = *replacement.asString().unwrap();
        } else if(!replacement.isVoid()) {
            Value t = replacement;
            if(const auto r = t.toString(); !r.isFailed() && t.isString())
                rep = *t.asString().unwrap();
        }
        std::string out = m.prefix().str();
        out.append(rep.getData(), rep.length());
        out += m.suffix().str();
        return String(out.c_str(), static_cast<u32>(out.size()));
    }

    String stringEscape(VM *, const Value &thisObj, size_t) { return thisObj.asString().unwrap()->escapeBackSlash(); }
    // TODO: need impl Array class
    // split(pattern, reserved, purgeempty)
    // Array stringSplit(VM *, const Value &thisObj, size_t, const String *pattern, const Value &vReserved, const Value
    // &vPurgeEmpty) {
    //     const String *str = thisObj.toString();
    //     Array result;
    //
    //     if (!pattern || pattern->isEmpty()) {
    //         // split by empty string -> split into single chars
    //         for (char c : *str)
    //             result.push(String(c));
    //         return result;
    //     }
    //
    //     const char *s = str->getData();
    //     const char *patt = pattern->getData();
    //     size_t pattLen = pattern->length();
    //     size_t i = 0;
    //
    //     while (i < str->length()) {
    //         const char *found = std::strstr(s + i, patt);
    //         if (!found) {
    //             // no more matches, push the rest
    //             String part(s + i, str->length() - i);
    //             if (!vPurgeEmpty.isVoid() && vPurgeEmpty.toBool() && part.isEmpty())
    //                 break;
    //             result.push(part);
    //             break;
    //         }
    //
    //         size_t index = found - s;
    //         String part(s + i, index - i);
    //         if (!vPurgeEmpty.isVoid() && vPurgeEmpty.toBool() && part.isEmpty()) {
    //             // skip empty
    //         } else {
    //             result.push(part);
    //         }
    //         i = index + pattLen;
    //     }
    //
    //     return result;
    // }

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
    void stringReverse(VM *, const Value &thisObj, size_t) {
        String *str = thisObj.asString().unwrap();
        if(str->isEmpty())
            return;

        char *begin = str->getBuffer();
        char *end = begin + str->length() - 1;
        while(begin < end) {
            std::swap(*begin, *end);
            ++begin;
            --end;
        }
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
