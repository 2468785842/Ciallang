// Copyright (c) 2026/10/24
//

#include "RegExp.hpp"

#include <algorithm>
#include <bit>
#include <oniguruma.h>

#include "vm/VMState.hpp"

#include "runtime/Runtime.hpp"

namespace cial {

    struct RegExp::CompiledPattern {
        OnigRegex expression{};
        ~CompiledPattern() {
            if(expression)
                onig_free(expression);
        }
    };

    RegExp::RegExp(Runtime *runtime, const String &expression, const String &flags) : _rt(runtime) {
        compilePattern(expression.toStdStr(), flags.toStdStr());
    }

    void RegExp::compilePattern(const std::string &expression, const std::string &flags) {
        // Oniguruma 全局表必须覆盖所有已编译表达式的生命周期，不在局部对象销毁时 onig_end。
        static const int initialized = [] {
            OnigEncoding encodings[] = { ONIG_ENCODING_UTF16_LE, ONIG_ENCODING_UTF16_BE };
            return onig_initialize(encodings, 2);
        }();
        if(initialized != ONIG_NORMAL)
            throw std::runtime_error("cannot initialize Oniguruma");
        const bool ignoreCase = flags.find('i') != std::string::npos;
        const std::string source = expression.empty() ? "(?:)" : expression;
        const auto units = Common::toUtf16(source);
        const auto *begin = reinterpret_cast<const OnigUChar *>(units.data());
        const auto encoding =
            std::endian::native == std::endian::little ? ONIG_ENCODING_UTF16_LE : ONIG_ENCODING_UTF16_BE;
        auto compiled = std::make_shared<CompiledPattern>();
        OnigErrorInfo error{};
        const auto options = ONIG_OPTION_CAPTURE_GROUP | ONIG_OPTION_FIND_NOT_EMPTY |
            (ignoreCase ? ONIG_OPTION_IGNORECASE : ONIG_OPTION_NONE);
        const int code = onig_new(&compiled->expression, begin, begin + units.size() * sizeof(char16_t), options,
                                  encoding, ONIG_SYNTAX_PERL, &error);
        if(code != ONIG_NORMAL) {
            OnigUChar message[ONIG_MAX_ERROR_MESSAGE_LEN]{};
            onig_error_code_to_str(message, code, &error);
            throw std::runtime_error("invalid regex /" + source + "/: " + reinterpret_cast<const char *>(message));
        }
        // 编译失败保留原表达式、标志和匹配状态。
        pattern = source;
        flagGlobal = flags.find('g') != std::string::npos;
        flagIgnoreCase = ignoreCase;
        _compiled = std::move(compiled);
    }

    bool RegExp::search(const std::u16string_view target, const size_t offset, RegexMatch &match,
                        const Pattern &snapshot) const {
        match.groups.clear();
        if(offset >= target.size())
            return false;
        const auto &compiled = snapshot ? snapshot : _compiled;
        auto region = std::unique_ptr<OnigRegion, void (*)(OnigRegion *)>(
            onig_region_new(), [](OnigRegion *value) { onig_region_free(value, 1); });
        if(!region)
            throw std::bad_alloc();
        const auto *begin = reinterpret_cast<const OnigUChar *>(target.data());
        const auto *end = begin + target.size() * sizeof(char16_t);
        const int result = onig_search(compiled->expression, begin, end, begin + offset * sizeof(char16_t), end,
                                       region.get(), ONIG_OPTION_NONE);
        if(result == ONIG_MISMATCH)
            return false;
        if(result < 0)
            throw std::runtime_error("regular expression search failed: " + std::to_string(result));
        for(int i = 0; i < region->num_regs; ++i) {
            const bool matched = region->beg[i] >= 0;
            const auto first = matched ? static_cast<size_t>(region->beg[i]) / sizeof(char16_t) : 0;
            const auto last = matched ? static_cast<size_t>(region->end[i]) / sizeof(char16_t) : 0;
            match.groups.push_back({ matched, first, last, target.substr(first, last - first) });
        }
        return true;
    }

    bool RegExp::searchFrom(const std::u16string_view target, RegexMatch &match) const {
        return start >= 0 && search(target, static_cast<size_t>(start), match);
    }

    bool RegExp::execState(const String &targetString, Array *result) {
        const auto target = Common::toUtf16(targetString.toStdStr());
        RegexMatch match;
        input = targetString;
        if(!searchFrom(target, match)) {
            index = lastIndex = start;
            lastMatch = String{};
            lastParen = String{};
            const auto prefix = start < 0 ? target.size() : std::min(static_cast<size_t>(start), target.size());
            leftContext = String{ Common::fromUtf16(std::u16string_view(target).substr(0, prefix)) };
            return false;
        }
        index = static_cast<Integer>(match.position(0));
        size_t last = 0;
        for(const auto &group : match.groups) {
            if(group.matched)
                last = std::max(last, group.end);
            if(result)
                result->elements.emplace_back(String{ group.str() });
        }
        lastIndex = static_cast<Integer>(last);
        lastMatch = String{ match[0].str() };
        lastParen = String{ match.size() > 1 ? match[match.size() - 1].str() : std::string{} };
        leftContext = String{ Common::fromUtf16(std::u16string_view(target).substr(0, static_cast<size_t>(index))) };
        rightContext = String{ Common::fromUtf16(std::u16string_view(target).substr(last)) };
        if(flagGlobal)
            start = lastIndex;
        return true;
    }


    namespace {
        // TJS2 RegExp 属性名（tjsRegExp.cpp）：matches/start/index/lastIndex/input/
        // lastMatch/lastParen/leftContext/rightContext；其中仅 start 可写。
        // last 为类对象静态只读属性（tTJSNC_RegExp::LastRegExp），此处挂在全局
        // "RegExp" 占位实例上应答（实例上也可读，超出原版但无害）。
        // 比较走 Runtime::wkAtoms 构造期驻留（旧实现每次 getProp intern ×9）
        enum class RegExpProp {
            None,
            Matches,
            Start,
            Index,
            LastIndex,
            Input,
            LastMatch,
            LastParen,
            LeftContext,
            RightContext,
            Last,
        };

        RegExpProp regexpPropName(const WellKnownAtoms &wk, const Atom a) {
            if(a == wk.reMatches)
                return RegExpProp::Matches;
            if(a == wk.reStart)
                return RegExpProp::Start;
            if(a == wk.reIndex)
                return RegExpProp::Index;
            if(a == wk.reLastIndex)
                return RegExpProp::LastIndex;
            if(a == wk.reInput)
                return RegExpProp::Input;
            if(a == wk.reLastMatch)
                return RegExpProp::LastMatch;
            if(a == wk.reLastParen)
                return RegExpProp::LastParen;
            if(a == wk.reLeftContext)
                return RegExpProp::LeftContext;
            if(a == wk.reRightContext)
                return RegExpProp::RightContext;
            if(a == wk.reLast)
                return RegExpProp::Last;
            return RegExpProp::None;
        }
    } // namespace

    // 类对象构造入口：RegExp(pattern, flags?) / new RegExp(...)（TJS2 无 new 亦可）
    void RegExp::call(vm::VMState &vmState, const u32 ret, const size_t argCount) {
        Value *args = vmState.curFrame()->getArgs(argCount);
        String sourcePattern{}, flags{};
        if(argCount >= 1 && !args[0].isVoid()) {
            Value v = args[0];
            if(const auto r = v.toString(); !r.isFailed() && v.isString())
                sourcePattern = *v.asString().unwrap();
        }
        if(argCount >= 2 && !args[1].isVoid()) {
            Value v = args[1];
            if(const auto r = v.toString(); !r.isFailed() && v.isString())
                flags = *v.asString().unwrap();
        }
        vmState.regRef(ret) = Value{ vmState.rt.create<RegExp>(&vmState.rt, sourcePattern, flags).get() };
    }

    void RegExp::setProp(const Atom a, const Value &v) {
        if(regexpPropName(_rt->wkAtoms, a) == RegExpProp::Start) {
            start = v.isVoid() ? Integer{ 0 } : v.asInteger().unwrap();
            return;
        }
        throw std::runtime_error("RegExp member is read-only");
    }

    Value RegExp::getProp(const Atom a) {
        switch(regexpPropName(_rt->wkAtoms, a)) {
            case RegExpProp::Matches:
                return matches ? Value{ matches } : Value{};
            case RegExpProp::Start:
                return Value{ start };
            case RegExpProp::Index:
                return Value{ index };
            case RegExpProp::LastIndex:
                return Value{ lastIndex };
            case RegExpProp::Input:
                return Value{ input };
            case RegExpProp::LastMatch:
                return Value{ lastMatch };
            case RegExpProp::LastParen:
                return Value{ lastParen };
            case RegExpProp::LeftContext:
                return Value{ leftContext };
            case RegExpProp::RightContext:
                return Value{ rightContext };
            case RegExpProp::Last:
                // tTJSNC_RegExp::LastRegExp：未执行过任何 test/exec 时为 void
                if(_rt->lastRegExp)
                    return Value{ static_cast<Object *>(_rt->lastRegExp) };
                return Value{};
            case RegExpProp::None:
                break;
        }
        throw std::runtime_error("RegExp member not found");
    }

    bool RegExp::hasProp(const Atom a) const { return regexpPropName(_rt->wkAtoms, a) != RegExpProp::None; }

} // namespace cial
