// Copyright (c) 2026/10/25
//

#define _CRT_SECURE_NO_WARNINGS

#include "DateLib.hpp"

#include <ctime>

#include "vm/VMState.hpp"

namespace cial::stdlib {

    namespace {
        DateNS *asDate(const Value &thisObj) {
            return dynamic_cast<DateNS *>(thisObj.asObject().unwrap());
        }

        Integer intArg(const Value &v, const Integer fallback) {
            return v.isVoid() ? fallback : v.asInteger().unwrap();
        }

        // set 系公共骨架：localtime 读 -> 改字段 -> mktime 写
        enum class DateField { Year, Month, Day, Hours, Minutes, Seconds };

        void mutateDate(DateNS *d, const DateField f, const Integer v) {
            tm t = *localtime(&d->dateTime);
            switch(f) {
                case DateField::Year:
                    t.tm_year = static_cast<int>(v) - 1900;
                    break;
                case DateField::Month:
                    t.tm_mon = static_cast<int>(v);
                    break;
                case DateField::Day:
                    t.tm_mday = static_cast<int>(v);
                    break;
                case DateField::Hours:
                    t.tm_hour = static_cast<int>(v);
                    break;
                case DateField::Minutes:
                    t.tm_min = static_cast<int>(v);
                    break;
                case DateField::Seconds:
                    t.tm_sec = static_cast<int>(v);
                    break;
            }
            d->dateTime = mktime(&t);
            if(d->dateTime == static_cast<time_t>(-1))
                throw std::runtime_error("invalid value for timestamp");
        }

        // 常用子集："YYYY/MM/DD HH:MM[:SS]"（也接受 YYYY-M-D）；完整 tjsdate.y
        // 文法（英文月名/相对表达等）未实现
        time_t parseDateString(const std::string &s) {
            tm t{};
            int y = 1970, mon = 0, day = 1, h = 0, mi = 0, sec = 0;
            if(const int n = std::sscanf(s.c_str(), "%d/%d/%d %d:%d:%d", &y, &mon, &day, &h, &mi, &sec);
               n < 3) {
                if(const int n2 = std::sscanf(s.c_str(), "%d-%d-%d %d:%d:%d", &y, &mon, &day, &h, &mi, &sec);
                   n2 < 3)
                    return static_cast<time_t>(-1);
            }
            t.tm_year = y - 1900;
            t.tm_mon = mon;
            t.tm_mday = day;
            t.tm_hour = h;
            t.tm_min = mi;
            t.tm_sec = sec;
            return mktime(&t);
        }
    } // namespace

    Value dateGetYear(VM *, const Value &thisObj, size_t, Value *) {
        const tm *t = localtime(&asDate(thisObj)->dateTime);
        return Value{ Integer{ t->tm_year + 1900 } };
    }

    Value dateGetMonth(VM *, const Value &thisObj, size_t, Value *) {
        const tm *t = localtime(&asDate(thisObj)->dateTime);
        return Value{ Integer{ t->tm_mon } };
    }

    Value dateGetDate(VM *, const Value &thisObj, size_t, Value *) {
        const tm *t = localtime(&asDate(thisObj)->dateTime);
        return Value{ Integer{ t->tm_mday } };
    }

    Value dateGetDay(VM *, const Value &thisObj, size_t, Value *) {
        const tm *t = localtime(&asDate(thisObj)->dateTime);
        return Value{ Integer{ t->tm_wday } };
    }

    Value dateGetHours(VM *, const Value &thisObj, size_t, Value *) {
        const tm *t = localtime(&asDate(thisObj)->dateTime);
        return Value{ Integer{ t->tm_hour } };
    }

    Value dateGetMinutes(VM *, const Value &thisObj, size_t, Value *) {
        const tm *t = localtime(&asDate(thisObj)->dateTime);
        return Value{ Integer{ t->tm_min } };
    }

    Value dateGetSeconds(VM *, const Value &thisObj, size_t, Value *) {
        const tm *t = localtime(&asDate(thisObj)->dateTime);
        return Value{ Integer{ t->tm_sec } };
    }

    Value dateGetTime(VM *, const Value &thisObj, size_t, Value *) {
        return Value{ Integer{ static_cast<Integer>(asDate(thisObj)->dateTime) * 1000 } };
    }

    Value dateSetYear(VM *, const Value &thisObj, size_t n, Value *args) {
        if(n < 1)
            throw std::runtime_error("Date.setYear expects a value");
        mutateDate(asDate(thisObj), DateField::Year, args[0].asInteger().unwrap());
        return Value{};
    }

    Value dateSetMonth(VM *, const Value &thisObj, size_t n, Value *args) {
        if(n < 1)
            throw std::runtime_error("Date.setMonth expects a value");
        mutateDate(asDate(thisObj), DateField::Month, args[0].asInteger().unwrap());
        return Value{};
    }

    Value dateSetDate(VM *, const Value &thisObj, size_t n, Value *args) {
        if(n < 1)
            throw std::runtime_error("Date.setDate expects a value");
        mutateDate(asDate(thisObj), DateField::Day, args[0].asInteger().unwrap());
        return Value{};
    }

    Value dateSetHours(VM *, const Value &thisObj, size_t n, Value *args) {
        if(n < 1)
            throw std::runtime_error("Date.setHours expects a value");
        mutateDate(asDate(thisObj), DateField::Hours, args[0].asInteger().unwrap());
        return Value{};
    }

    Value dateSetMinutes(VM *, const Value &thisObj, size_t n, Value *args) {
        if(n < 1)
            throw std::runtime_error("Date.setMinutes expects a value");
        mutateDate(asDate(thisObj), DateField::Minutes, args[0].asInteger().unwrap());
        return Value{};
    }

    Value dateSetSeconds(VM *, const Value &thisObj, size_t n, Value *args) {
        if(n < 1)
            throw std::runtime_error("Date.setSeconds expects a value");
        mutateDate(asDate(thisObj), DateField::Seconds, args[0].asInteger().unwrap());
        return Value{};
    }

    Value dateSetTime(VM *, const Value &thisObj, size_t n, Value *args) {
        if(n < 1)
            throw std::runtime_error("Date.setTime expects milliseconds");
        asDate(thisObj)->dateTime = static_cast<time_t>(args[0].asInteger().unwrap() / 1000);
        return Value{};
    }

    Value dateParse(VM *, const Value &thisObj, size_t n, Value *args) {
        if(n < 1 || !args[0].isString())
            throw std::runtime_error("Date.parse expects a date string");
        const String *s = args[0].asString().unwrap();
        const time_t t = parseDateString(std::string(s->getData(), s->length()));
        if(t == static_cast<time_t>(-1))
            throw std::runtime_error("invalid value for timestamp");
        asDate(thisObj)->dateTime = t;
        return Value{};
    }

    Value dateGetTimezoneOffset(VM *, const Value &, size_t, Value *) {
#ifdef _WIN32
        long tzSeconds = 0;
        _get_timezone(&tzSeconds);
        return Value{ Integer{ tzSeconds / 60 } }; // TJS2 = TJS_timezone/60（西正东负）
#else
        return Value{ Integer{ timezone / 60 } };
#endif
    }

} // namespace cial::stdlib

namespace cial {
    // 类对象构造入口：0 参 now；字符串解析；分量 mktime（见 DateLib.hpp）
    void DateNS::call(vm::VMState &vmState, const u32 ret, const size_t argCount) {
        Value *args = vmState.curFrame()->getArgs(argCount);
        time_t t{};

        if(argCount == 0) {
            t = std::time(nullptr);
        } else if(args[0].isString()) {
            const String *s = args[0].asString().unwrap();
            t = stdlib::parseDateString(std::string(s->getData(), s->length()));
            if(t == static_cast<time_t>(-1))
                throw std::runtime_error("invalid value for timestamp");
        } else {
            tm d{};
            d.tm_year = static_cast<int>(stdlib::intArg(args[0], Integer{ 1970 })) - 1900;
            if(argCount >= 2 && !args[1].isVoid())
                d.tm_mon = static_cast<int>(args[1].asInteger().unwrap());
            d.tm_mday = argCount >= 3 && !args[2].isVoid() ? static_cast<int>(args[2].asInteger().unwrap()) : 1;
            if(argCount >= 4 && !args[3].isVoid())
                d.tm_hour = static_cast<int>(args[3].asInteger().unwrap());
            if(argCount >= 5 && !args[4].isVoid())
                d.tm_min = static_cast<int>(args[4].asInteger().unwrap());
            if(argCount >= 6 && !args[5].isVoid())
                d.tm_sec = static_cast<int>(args[5].asInteger().unwrap());
            t = mktime(&d);
            if(t == static_cast<time_t>(-1))
                throw std::runtime_error("invalid value for timestamp");
        }

        vmState.regRef(ret) = Value{ vmState.rt.create<DateNS>(t).get() };
    }
} // namespace cial
