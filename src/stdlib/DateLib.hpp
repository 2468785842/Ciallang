// Copyright (c) 2026/10/25
//
// Date 内建库（TJS2 tjsDate.cpp 对齐）：构造（now / 分量 mktime / 字符串解析）+
// get 系（localtime 字段）/ set 系（读改 mktime）/ getTime(ms)/setTime(ms) /
// getTimezoneOffset(分钟)。内部表示 time_t 秒，与原版一致。
// 字符串 parse 走 DateParser（tjsdate.y 完整文法）。
#pragma once

#include "types/Object.hpp"
#include "types/String.hpp"
#include "types/TypeId.hpp"
#include "types/Value.hpp"

namespace cial {

    class VM;

    class DateNS final : public Object {
    public:
        time_t dateTime{};

        DateNS() = default;

        explicit DateNS(const time_t t) : dateTime(t) {}

        [[nodiscard]] TypeId typeId() const noexcept override { return TypeId::Date; }

        // 类对象构造入口：Date() / new Date(...)——0 参 now；字符串解析；
        // (year[, mon=0, day=1, h=0, m=0, s=0]) 走 mktime
        void call(vm::VMState &vmState, u32 ret, size_t argCount) override;
    };

} // namespace cial

namespace cial::stdlib {

    // 实例方法（thisObj 为 DateNS；get 系走 localtime，set 系读改 mktime）
    Value dateGetYear(VM *, const Value &, size_t, Value *);
    Value dateGetMonth(VM *, const Value &, size_t, Value *);
    Value dateGetDate(VM *, const Value &, size_t, Value *);
    Value dateGetDay(VM *, const Value &, size_t, Value *);
    Value dateGetHours(VM *, const Value &, size_t, Value *);
    Value dateGetMinutes(VM *, const Value &, size_t, Value *);
    Value dateGetSeconds(VM *, const Value &, size_t, Value *);
    Value dateGetTime(VM *, const Value &, size_t, Value *);
    Value dateSetYear(VM *, const Value &, size_t, Value *args);
    Value dateSetMonth(VM *, const Value &, size_t, Value *args);
    Value dateSetDate(VM *, const Value &, size_t, Value *args);
    Value dateSetHours(VM *, const Value &, size_t, Value *args);
    Value dateSetMinutes(VM *, const Value &, size_t, Value *args);
    Value dateSetSeconds(VM *, const Value &, size_t, Value *args);
    Value dateSetTime(VM *, const Value &, size_t, Value *args);
    Value dateParse(VM *, const Value &, size_t, Value *args);
    // static：本地时区与 UTC 的偏移（分钟，与 getTimezoneOffset 同号语义）
    Value dateGetTimezoneOffset(VM *, const Value &, size_t, Value *);

} // namespace cial::stdlib
