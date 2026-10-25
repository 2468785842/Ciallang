// Copyright (c) 2026/10/25
//
// Math.RandomGenerator（TJS2 tjsRandomGenerator.cpp 对齐）：MT19937 随机源类。
// MT 为标准实现自写（std::mt19937 内部布局不可移植），保证：
// ① 固定种子序列与原版逐值一致；② serialize 的 Dictionary{state(hex 624×8),
// left, next} 跨引擎可恢复。0 参种子 = 当前时间；整数种子 = (低32, 高32) 数组
// 初始化（init_by_array）；Dictionary 种子 = 状态恢复。
#pragma once

#include "runtime/AtomTable.hpp"
#include "types/Object.hpp"
#include "types/String.hpp"
#include "types/TypeId.hpp"
#include "types/Value.hpp"

namespace cial {

    class VM;

    class RandomGen final : public Object {
    public:
        static constexpr int MT_N = 624;

        RandomGen() = default;

        [[nodiscard]] TypeId typeId() const noexcept override { return TypeId::RandomGen; }

        // 类对象构造入口：new Math.RandomGenerator([seed])（同 randomize 语义）
        void call(vm::VMState &vmState, u32 ret, size_t argCount) override;

        // randomize 的三种形态（stdlib::rgRandomize 与 call 共用）
        void randomizeNoArg(); // init_genrand(time)，对齐原版 0 参
        void randomizeByKey(const u32 key[2]); // init_by_array((低32, 高32))
        void randomizeFrom(AtomTable &at, const Value &dict); // serialize 状态恢复
        void restoreFrom(const String &stateHex, int leftVal, int nextIdx);

        [[nodiscard]] double random(); // 53 位 [0,1)
        [[nodiscard]] u32 random32();
        [[nodiscard]] i64 random63();
        [[nodiscard]] i64 random64();
        [[nodiscard]] Value serialize(VM *vm); // Dictionary{state, left, next}

        void marked() noexcept override;

    private:
        // MT19937 状态（对齐 tTJSMersenneTwisterData：state + left + next 指针语义）。
        // state 体积 2.5KB 超 GC 固定节点（512B），堆分离持有，析构释放（sweep 调析构）
        u32 *state{};
        int left{ 0 };
        u32 *next{ nullptr };
        bool inited{ false };

    public:
        ~RandomGen() override { delete[] state; }

    private:
        void ensureState() {
            if(!state)
                state = new u32[MT_N]{};
        }

        void initGenRand(u32 s);
        void initByArray(const u32 *key, size_t keyLen);
        void nextBlock();
        [[nodiscard]] u32 int32();
    };

} // namespace cial

namespace cial::stdlib {

    Value rgRandomize(VM *, const Value &thisObj, size_t argCount, Value *args);
    Value rgRandom(VM *, const Value &thisObj, size_t, Value *);
    Value rgRandom32(VM *, const Value &thisObj, size_t, Value *);
    Value rgRandom63(VM *, const Value &thisObj, size_t, Value *);
    Value rgRandom64(VM *, const Value &thisObj, size_t, Value *);
    Value rgSerialize(VM *vm, const Value &thisObj, size_t, Value *);

} // namespace cial::stdlib
