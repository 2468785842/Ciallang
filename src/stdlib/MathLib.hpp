// Copyright (c) 2026/10/25
//
// Math 内建库（TJS2 tjsMath.cpp 对齐）：16 个静态方法 + 8 个常量属性 +
// Xorshift random（与原版同算法同播散；序列不保证跨版本一致，原版亦然）。
// 承载对象 MathNS 经 TypeId::Math 走 NativeRegister 分派（与 Array/RegExp 同机制）。
#pragma once

#include "types/Object.hpp"
#include "types/String.hpp"
#include "types/TypeId.hpp"
#include "types/Value.hpp"

namespace cial {

    class VM;

    class RandomGen;

    // Math 命名空间承载对象（call 报错——Math 不能 new）；持有 RandomGenerator
    // 类对象（Math.RandomGenerator 构造入口）
    class MathNS final : public Object {
    public:
        MathNS() = default;

        [[nodiscard]] TypeId typeId() const noexcept override { return TypeId::Math; }

        void call(vm::VMState &, u32, size_t) override { throw std::runtime_error("Math is not callable"); }

        void setRandomGenerator(Atom name, RandomGen *rg) {
            rgAtom = name;
            rgClass = rg;
        }

        Value getProp(Atom a) override;

        [[nodiscard]] bool hasProp(Atom a) const override { return a == rgAtom && rgClass; }

        void marked() noexcept override;

    private:
        Atom rgAtom = ATOM_INVALID;
        RandomGen *rgClass{};
    };

} // namespace cial

namespace cial::stdlib {

    // 全部方法语义对齐 tjsMath.cpp：参数经 AsReal 转换、返回 Real
    Value mathAbs(VM *, const Value &, size_t, Value *args);
    Value mathAcos(VM *, const Value &, size_t, Value *args);
    Value mathAsin(VM *, const Value &, size_t, Value *args);
    Value mathAtan(VM *, const Value &, size_t, Value *args);
    Value mathAtan2(VM *, const Value &, size_t, Value *args);
    Value mathCeil(VM *, const Value &, size_t, Value *args);
    Value mathExp(VM *, const Value &, size_t, Value *args);
    Value mathFloor(VM *, const Value &, size_t, Value *args);
    Value mathLog(VM *, const Value &, size_t, Value *args);
    Value mathPow(VM *, const Value &, size_t, Value *args);
    // max/min：NaN 传播 + ±0 语义（对齐原版位级判断）
    Value mathMax(VM *, const Value &, size_t, Value *args);
    Value mathMin(VM *, const Value &, size_t, Value *args);
    Value mathRandom(VM *, const Value &, size_t, Value *args);
    Value mathRound(VM *, const Value &, size_t, Value *args);
    Value mathSin(VM *, const Value &, size_t, Value *args);
    Value mathCos(VM *, const Value &, size_t, Value *args);
    Value mathSqrt(VM *, const Value &, size_t, Value *args);
    Value mathTan(VM *, const Value &, size_t, Value *args);

} // namespace cial::stdlib
