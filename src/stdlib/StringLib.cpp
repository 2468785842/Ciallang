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

#include "StringLib.hpp"
#include "vm/VM.hpp"

namespace cial::StdLib {

    String stringCharAt(VM *, const Value &thisObj, size_t, const Integer index) {
        const String *str = thisObj.toString();
        if(str->isEmpty() || index < 0 || index >= str->length())
            return ""_str;
        return String(str->getData()[index]);
    }

    Integer stringIndexOf(VM *, const Value &thisObj, size_t, const String *subStr, const Value &vStart) {
        const String *str = thisObj.toString();
        if(str->isEmpty())
            return -1;

        Integer start{ 0 };

        if(str->isEmpty() || subStr->isEmpty())
            return -1;
        if(!vStart.isVoid())
            start = vStart.toInteger();

        if(start >= str->length()) {
            return -1;
        }

        const char *p = std::strstr(str->getData() + start, subStr->getData());
        if(!p)
            return -1;
        return p - str->getData();
    }

    void stringToUpperCase(VM *, const Value &thisObj, size_t) {
        String *str = thisObj.toString();
        if(str->isEmpty())
            return;
        for(char &c : *str) {
            if(c >= 'a' && c <= 'z')
                c += 'Z' - 'z';
        }
    }

    void stringToLowerCase(VM *, const Value &thisObj, size_t) {
        String *str = thisObj.toString();
        if(str->isEmpty())
            return;
        for(char &c : *str) {
            if(c >= 'A' && c <= 'Z')
                c += 'z' - 'Z';
        }
    }

    String stringSubstring(VM *, const Value &thisObj, size_t, const Integer start, const Value &vLength) {
        const String *str = thisObj.toString();
        const Integer sLen = str->length();

        if(start < 0 || start >= sLen) {
            return ""_str;
        }

        if(!vLength.isVoid()) {
            Integer count = vLength.toInteger();
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
        return formatString(*thisObj.toString(), argCount, args);
    }

    // TODO: need impl RegExp class
    // String stringReplace(VM *, const Value &, size_t, const Value &) {
    //     throw std::runtime_error("Not implemented");
    // }

    String stringEscape(VM *, const Value &thisObj, size_t) { return thisObj.toString()->escapeBackSlash(); }
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
        const String *str = thisObj.toString();
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
        String *str = thisObj.toString();
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
        const String *str = thisObj.toString();
        if(count <= 0 || str->isEmpty())
            return ""_str;

        std::string result;
        result.reserve(str->length() * count);
        for(Integer i = 0; i < count; ++i)
            result.append(str->getData(), str->length());

        return String(result.c_str(), static_cast<std::uint32_t>(result.size()));
    }

} // namespace cial::StdLib
