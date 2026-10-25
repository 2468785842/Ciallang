// Copyright (c) 2026/10/24
//

#include "RegExp.hpp"

#include "vm/VMState.hpp"

#include "runtime/Runtime.hpp"

namespace cial {

    namespace {
        // TJS2 RegExp 属性名（tjsRegExp.cpp）：matches/start/index/lastIndex/input/
        // lastMatch/lastParen/leftContext/rightContext；其中仅 start 可写。
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
            return RegExpProp::None;
        }
    } // namespace

    // 类对象构造入口：RegExp(pattern, flags?) / new RegExp(...)（TJS2 无 new 亦可）
    void RegExp::call(vm::VMState &vmState, const u32 ret, const size_t argCount) {
        Value *args = vmState.curFrame()->getArgs(argCount);
        String pattern{}, flags{};
        if(argCount >= 1 && !args[0].isVoid()) {
            Value v = args[0];
            if(const auto r = v.toString(); !r.isFailed() && v.isString())
                pattern = *v.asString().unwrap();
        }
        if(argCount >= 2 && !args[1].isVoid()) {
            Value v = args[1];
            if(const auto r = v.toString(); !r.isFailed() && v.isString())
                flags = *v.asString().unwrap();
        }
        vmState.regRef(ret) = Value{ vmState.rt.create<RegExp>(&vmState.rt, pattern, flags).get() };
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
            case RegExpProp::None:
                break;
        }
        throw std::runtime_error("RegExp member not found");
    }

    bool RegExp::hasProp(const Atom a) const {
        return regexpPropName(_rt->wkAtoms, a) != RegExpProp::None;
    }

} // namespace cial
