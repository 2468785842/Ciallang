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
        // String 是语言内建类型，不走 registerClass；导出与声明生成需要注册名
        context.recordApiTypeName(TypeTraits<String>::key(), "String");
        context.nativeType<String>()
            .doc("字符串；不可变，方法返回新字符串或分析结果。")
            .method("charAt"_str, &stdlib::stringCharAt,
                    api::signature({ { .name = "index", .type = "Integer" } }, "String", "取指定位置的 UTF-16 字符。"))
            .method("indexOf"_str, &stdlib::stringIndexOf,
                    api::signature({ { .name = "subStr", .type = "String" },
                                     { .name = "start", .type = "Integer", .optional = true, .defaultText = "0" } },
                                   "Integer", "查找子串，未找到返回 -1。"))
            .method("toUpperCase"_str, &stdlib::stringToUpperCase, api::signature({}, "String", "转大写。"))
            .method("toLowerCase"_str, &stdlib::stringToLowerCase, api::signature({}, "String", "转小写。"))
            .method("substring"_str, &stdlib::stringSubstring,
                    api::signature({ { .name = "start", .type = "Integer" },
                                     { .name = "length", .type = "Integer", .optional = true, .defaultText = "void" } },
                                   "String", "按 UTF-16 位置与长度取子串。"))
            .method("substr"_str, &stdlib::stringSubstring,
                    api::signature({ { .name = "start", .type = "Integer" },
                                     { .name = "length", .type = "Integer", .optional = true, .defaultText = "void" } },
                                   "String", "substring 的别名。"))
            .method("sprintf"_str, &stdlib::stringSprintf,
                    api::signature({ { .name = "args", .type = "any", .collapse = true } }, "String",
                                   "以当前字符串为格式模板。"))
            .method(
                "replace"_str, &stdlib::stringReplace,
                api::signature(
                    { { .name = "pattern", .type = "RegExp" }, { .name = "replacement", .type = "String | Function" } },
                    "String",
                    "用正则替换文本，返回新字符串。\n@param pattern 匹配规则；g 标志控制全局替换。\n@param replacement "
                    "替换字符串，或接收匹配数组并返回替换文本的函数。\n@returns 新字符串，不修改原字符串。"))
            .method("split"_str, &stdlib::stringSplitWith,
                    api::signature(
                        { { .name = "pattern", .type = "String | RegExp" },
                          { .name = "reserved", .type = "void", .optional = true, .defaultText = "void" },
                          { .name = "purgeempty", .type = "Boolean", .optional = true, .defaultText = "false" } },
                        "String[]",
                        "按模式拆分为字符串数组。\n@param pattern RegExp 按正则匹配；String 作为分隔字符集合。\n@param "
                        "reserved 保留位置，传 void。\n@param purgeempty 是否移除空片段。\n@returns "
                        "Array，所有元素均为 String。"))
            .method("escape"_str, &stdlib::stringEscape, api::signature({}, "String", "转义特殊字符。"))
            .method("trim"_str, &stdlib::stringTrim, api::signature({}, "String", "去除首尾空白。"))
            .method("reverse"_str, &stdlib::stringReverse, api::signature({}, "String", "反转字符串。"))
            .method("repeat"_str, &stdlib::stringRepeat,
                    api::signature({ { .name = "count", .type = "Integer" } }, "String", "重复字符串。"))
            .property(
                "length"_str,
                [](const Value &thisObj) {
                    const auto *str = thisObj.asString().unwrap();
                    return Value{ Integer{
                        static_cast<Integer>(Common::utf16Length({ str->getData(), str->length() })) } };
                },
                ApiMeta{ .ret = "Integer", .doc = "UTF-16 码元数。" });
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
