// Copyright (c) 2026/10/24
//

#include "RegExp.hpp"

#include "runtime/Runtime.hpp"

namespace cial {

    namespace {
        // TJS2 RegExp 属性名（tjsRegExp.cpp）：matches/start/index/lastIndex/input/
        // lastMatch/lastParen/leftContext/rightContext；其中仅 start 可写
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

        RegExpProp regexpPropName(AtomTable &atomTable, const Atom a) {
            if(a == atomTable.intern("matches"_str))
                return RegExpProp::Matches;
            if(a == atomTable.intern("start"_str))
                return RegExpProp::Start;
            if(a == atomTable.intern("index"_str))
                return RegExpProp::Index;
            if(a == atomTable.intern("lastIndex"_str))
                return RegExpProp::LastIndex;
            if(a == atomTable.intern("input"_str))
                return RegExpProp::Input;
            if(a == atomTable.intern("lastMatch"_str))
                return RegExpProp::LastMatch;
            if(a == atomTable.intern("lastParen"_str))
                return RegExpProp::LastParen;
            if(a == atomTable.intern("leftContext"_str))
                return RegExpProp::LeftContext;
            if(a == atomTable.intern("rightContext"_str))
                return RegExpProp::RightContext;
            return RegExpProp::None;
        }
    } // namespace

    void RegExp::setProp(const Atom a, const Value &v) {
        if(regexpPropName(_rt->atomTable, a) == RegExpProp::Start) {
            start = v.isVoid() ? Integer{ 0 } : v.asInteger().unwrap();
            return;
        }
        throw std::runtime_error("RegExp member is read-only");
    }

    Value RegExp::getProp(const Atom a) {
        switch(regexpPropName(_rt->atomTable, a)) {
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
        return regexpPropName(_rt->atomTable, a) != RegExpProp::None;
    }

} // namespace cial
