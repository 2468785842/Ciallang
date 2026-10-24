// Copyright (c) 2026/10/24
//

#include "RegExpLib.hpp"

#include "common/UTF8.hpp"
#include "types/Array.hpp"
#include "vm/VM.hpp"

namespace cial::stdlib {

    namespace {
        RegExp *asRegExp(const Value &thisObj) {
            return dynamic_cast<RegExp *>(thisObj.asObject().unwrap());
        }

        // 值 → 字符串（TJS2 ttstr(*param) 语义：void 得空串）
        String valueToString(const Value &v) {
            if(v.isString())
                return *v.asString().unwrap();
            if(v.isVoid())
                return ""_str;
            Value tmp = v;
            if(const auto r = tmp.toString(); !r.isFailed() && tmp.isString())
                return *tmp.asString().unwrap();
            return ""_str;
        }

        String makeString(const std::string &s) { return String(s.c_str(), static_cast<u32>(s.size())); }

        void pushString(Array *arr, const std::string &s) { arr->elements.push_back(Value{ makeString(s) }); }

        // TJS2 split_regex：反复搜索，把每段匹配前的片段入数组；purgeempty 丢弃空片段。
        // 空匹配时前进 1 字节防死循环（TJS2 依赖 oniguruma FIND_NOT_EMPTY 规避，std::regex 无此选项）
        void splitRegexInto(const RegExp *re, const std::string &target, const bool purgeempty, Array *out) {
            size_t offset = 0;
            std::smatch m;
            while(offset < target.size() &&
                  std::regex_search(target.begin() + static_cast<i64>(offset), target.end(), m, re->re)) {
                const size_t pos = offset + static_cast<size_t>(m.position(0));
                const size_t end = pos + static_cast<size_t>(m.length(0));
                if(!purgeempty || pos > offset)
                    pushString(out, target.substr(offset, pos - offset));
                offset = end > pos ? end : pos + 1;
            }
            if(!purgeempty || offset < target.size())
                pushString(out, target.substr(offset));
        }

        // TJS2 Array.split 的字符串模式：pattern 的每个字符都是分隔符（UTF-8 逐字符比较）
        void splitCharsetInto(const String &pattern, const std::string &target, const bool purgeempty,
                              Array *out) {
            Vec<std::string_view> delims;
            const char *p = pattern.getData();
            const u32 plen = pattern.length();
            u32 i = 0;
            while(i < plen) {
                const auto cp = Common::utf8Decode(p + i, plen - i);
                const u32 w = cp.width <= 0 ? 1 : static_cast<u32>(cp.width);
                delims.emplace_back(p + i, w);
                i += w;
            }

            size_t segStart = 0;
            size_t pos = 0;
            while(pos < target.size()) {
                const auto cp = Common::utf8Decode(target.data() + pos, target.size() - pos);
                const size_t w = cp.width <= 0 ? 1 : static_cast<size_t>(cp.width);
                const std::string_view ch(target.data() + pos, w);
                if(std::find(delims.begin(), delims.end(), ch) != delims.end()) {
                    if(!purgeempty || pos > segStart)
                        pushString(out, target.substr(segStart, pos - segStart));
                    pos += w;
                    segStart = pos;
                } else {
                    pos += w;
                }
            }
            if(!purgeempty || pos > segStart)
                pushString(out, target.substr(segStart));
        }

        void splitInto(const Value &pattern, const std::string &target, const bool purgeempty, Array *out) {
            if(pattern.isObject()) {
                if(const auto *re = dynamic_cast<const RegExp *>(pattern.asObject().unwrap())) {
                    splitRegexInto(re, target, purgeempty, out);
                    return;
                }
            }
            splitCharsetInto(valueToString(pattern), target, purgeempty, out);
        }
    } // namespace

    Value regexpTest(VM *vm, const Value &thisObj, const size_t argCount, Value *args) {
        auto *re = asRegExp(thisObj);
        if(argCount < 1)
            throw std::runtime_error("RegExp.test expects a target string");
        auto *arr = vm->create<Array>(&vm->rt());
        const bool matched = re->execState(valueToString(args[0]), arr);
        re->matches = arr;
        return Value{ Integer{ matched ? 1 : 0 } };
    }

    Value regexpMatch(VM *vm, const Value &thisObj, const size_t argCount, Value *args) {
        const auto *re = asRegExp(thisObj);
        if(argCount < 1)
            throw std::runtime_error("RegExp.match expects a target string");
        const String targetStr = valueToString(args[0]);
        const std::string target(targetStr.getData(), targetStr.length());
        auto *arr = vm->create<Array>(&vm->rt());
        std::smatch m;
        if(re->searchFrom(target, m)) {
            for(size_t i = 0; i < m.size(); ++i)
                pushString(arr, m[i].str()); // 未匹配组为空串（TJS2 全量填充语义）
        }
        return Value{ arr };
    }

    Value regexpExec(VM *vm, const Value &thisObj, const size_t argCount, Value *args) {
        auto *re = asRegExp(thisObj);
        if(argCount < 1)
            throw std::runtime_error("RegExp.exec expects a target string");
        auto *arr = vm->create<Array>(&vm->rt());
        re->execState(valueToString(args[0]), arr);
        re->matches = arr;
        return Value{ arr };
    }

    void regexpCompile(VM *, const Value &thisObj, const size_t argCount, Value *args) {
        auto *re = asRegExp(thisObj);
        if(argCount < 1 || args[0].isVoid())
            throw std::runtime_error("RegExp.compile expects a pattern");
        re->compilePattern(std::string(valueToString(args[0]).getData(), valueToString(args[0]).length()),
                           argCount >= 2 && !args[1].isVoid()
                               ? std::string(valueToString(args[1]).getData(), valueToString(args[1]).length())
                               : std::string{});
    }

    Value regexpReplace(VM *, const Value &thisObj, const size_t argCount, Value *args) {
        const auto *re = asRegExp(thisObj);
        if(argCount < 2)
            throw std::runtime_error("RegExp.replace expects (target, replacement)");
        return Value{ regexpReplaceIn(re, valueToString(args[0]), valueToString(args[1])) };
    }

    Value regexpSplit(VM *vm, const Value &thisObj, const size_t argCount, Value *args) {
        const auto *re = asRegExp(thisObj);
        if(argCount < 1)
            throw std::runtime_error("RegExp.split expects a target string");
        const String targetStr = valueToString(args[0]);
        const bool purgeempty = argCount >= 3 && args[2].asBool();
        auto *arr = vm->create<Array>(&vm->rt());
        splitRegexInto(re, std::string(targetStr.getData(), targetStr.length()), purgeempty, arr);
        return Value{ arr };
    }

    String regexpReplaceIn(const RegExp *re, const String &targetStr, const String &rep) {
        const std::string target(targetStr.getData(), targetStr.length());
        std::string out;
        out.reserve(target.size());
        size_t last = 0;
        for(auto it = std::sregex_iterator(target.begin(), target.end(), re->re);
            it != std::sregex_iterator(); ++it) {
            const std::smatch &m = *it;
            const size_t pos = static_cast<size_t>(m.position(0));
            const size_t end = pos + static_cast<size_t>(m.length(0));
            out.append(target, last, pos - last);
            out.append(rep.getData(), rep.length());
            last = end;
            if(!re->flagGlobal)
                break; // TJS2：无 g 仅替换第一处
        }
        out.append(target, last, target.size() - last);
        return makeString(out);
    }

    Value stringSplitWith(VM *vm, const Value &thisObj, const size_t argCount, Value *args) {
        if(argCount < 1)
            throw std::runtime_error("String.split expects a pattern");
        const String *str = thisObj.asString().unwrap();
        const bool purgeempty = argCount >= 3 && args[2].asBool();
        auto *arr = vm->create<Array>(&vm->rt());
        splitInto(args[0], std::string(str->getData(), str->length()), purgeempty, arr);
        return Value{ arr };
    }

    Value arraySplitWith(VM *, const Value &thisObj, const size_t argCount, Value *args) {
        auto *arr = dynamic_cast<Array *>(thisObj.asObject().unwrap());
        if(argCount < 2)
            throw std::runtime_error("Array.split expects (pattern, string)");
        const bool purgeempty = argCount >= 4 && args[3].asBool();
        arr->elements.clear();
        const String targetStr = valueToString(args[1]);
        splitInto(args[0], std::string(targetStr.getData(), targetStr.length()), purgeempty, arr);
        return Value{ arr };
    }

} // namespace cial::stdlib
