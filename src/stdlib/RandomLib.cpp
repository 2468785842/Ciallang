// Copyright (c) 2026/10/25
//

#include "RandomLib.hpp"
#include "runtime/Context.hpp"

#include <array>
#include <cstring>
#include <ctime>
#include <utility>

#include "types/Dictionary.hpp"
#include "vm/VM.hpp"
#include "vm/VMState.hpp"

namespace cial {

    namespace {
        // MT19937（对齐 tjsMT19937ar-cok.cpp，含 Shawn Cokus 优化的两段 twist）
        constexpr int MT_M = 397;
        constexpr u32 MATRIX_A = 0x9908b0df;
        constexpr u32 UMASK = 0x80000000;
        constexpr u32 LMASK = 0x7fffffff;

        constexpr u32 mixBits(const u32 u, const u32 v) { return (u & UMASK) | (v & LMASK); }

        constexpr u32 twist(const u32 u, const u32 v) { return (mixBits(u, v) >> 1) ^ (v & 1 ? MATRIX_A : 0); }
    } // namespace

    void RandomGen::initGenRand(const u32 s) {
        ensureState();
        state[0] = s;
        for(int j = 1; j < MT_N; ++j)
            state[j] = 1812433253u * (state[j - 1] ^ (state[j - 1] >> 30)) + static_cast<u32>(j);
        left = 1;
        next = state;
        inited = true;
    }

    void RandomGen::initByArray(const u32 *key, const size_t keyLen) {
        ensureState();
        initGenRand(19650218u);
        int i = 1, j = 0;
        int k = MT_N > static_cast<int>(keyLen) ? MT_N : static_cast<int>(keyLen);
        for(; k; --k) {
            state[i] = (state[i] ^ ((state[i - 1] ^ (state[i - 1] >> 30)) * 1664525u)) + key[j] + static_cast<u32>(j);
            ++i;
            ++j;
            if(i >= MT_N) {
                state[0] = state[MT_N - 1];
                i = 1;
            }
            if(j >= static_cast<int>(keyLen))
                j = 0;
        }
        for(k = MT_N - 1; k; --k) {
            state[i] = (state[i] ^ ((state[i - 1] ^ (state[i - 1] >> 30)) * 1566083941u)) - static_cast<u32>(i);
            ++i;
            if(i >= MT_N) {
                state[0] = state[MT_N - 1];
                i = 1;
            }
        }
        state[0] = 0x80000000;
        left = 1;
        next = state;
        inited = true;
    }

    void RandomGen::nextBlock() {
        left = MT_N;
        next = state;
        u32 *p = state;
        for(int j = MT_N - MT_M + 1; --j; ++p)
            *p = p[MT_M] ^ twist(p[0], p[1]);
        for(int j = MT_M; --j; ++p)
            *p = p[MT_M - MT_N] ^ twist(p[0], p[1]);
        *p = p[MT_M - MT_N] ^ twist(p[0], state[0]);
    }

    u32 RandomGen::int32() {
        if(--left == 0)
            nextBlock();
        u32 y = *next++;
        // tempering 必须分步：后一步的 y 是前一步结果（一行连锁会全程用原始 y）
        y ^= y >> 11;
        y ^= (y << 7) & 0x9d2c5680u;
        y ^= (y << 15) & 0xefc60000u;
        y ^= y >> 18;
        return y;
    }

    double RandomGen::random() {
        if(!inited)
            return 0.0;
        // 对齐 rand_double：两个 int32 拼尾数 52 位，置 [1,2) 位型后减 1
        u64 v = 0;
        *reinterpret_cast<u32 *>(&v) = int32();
        *(reinterpret_cast<u32 *>(&v) + 1) = int32();
        v &= (1ULL << 52) - 1;
        v |= 1023ULL << 52; // 指数域 1023（未偏置 0）→ 1.0 <= v < 2.0
        double d;
        std::memcpy(&d, &v, 8);
        return d - 1.0;
    }

    u32 RandomGen::random32() { return inited ? int32() : 0; }

    i64 RandomGen::random63() {
        if(!inited)
            return 0;
        u64 v = 0;
        *reinterpret_cast<u32 *>(&v) = int32();
        *(reinterpret_cast<u32 *>(&v) + 1) = int32();
        return static_cast<i64>(v & 0x7fffffffffffffffULL);
    }

    i64 RandomGen::random64() {
        if(!inited)
            return 0;
        u64 v = 0;
        *reinterpret_cast<u32 *>(&v) = int32();
        *(reinterpret_cast<u32 *>(&v) + 1) = int32();
        return static_cast<i64>(v);
    }

    Value RandomGen::serialize(VM *vm) {
        auto *dic = vm->create<Dictionary>();
        static constexpr char hex[] = "0123456789abcdef";
        std::string s;
        s.reserve(MT_N * 8);
        for(int i = 0; i < MT_N; ++i)
            for(int shift = 28; shift >= 0; shift -= 4)
                s.push_back(hex[(state[i] >> shift) & 0xf]);
        AtomTable &at = vm->rt().atomTable;
        dic->setProp(at.intern("state"_str), Value{ String(s.c_str(), static_cast<u32>(s.size())) });
        dic->setProp(at.intern("left"_str), Value{ Integer{ left } });
        dic->setProp(at.intern("next"_str), Value{ Integer{ static_cast<Integer>(next - state) } });
        return Value{ dic };
    }

    void RandomGen::restoreFrom(const String &stateHex, const int leftVal, const int nextIdx) {
        // 刚播种为 {left=1,next=0}；取数后 next + left = MT_N + 1。
        const bool initial = leftVal == 1 && nextIdx == 0;
        const bool active = nextIdx >= 1 && nextIdx <= MT_N && leftVal == MT_N - nextIdx + 1;
        if(stateHex.length() != MT_N * 8 || (!initial && !active))
            throw std::runtime_error("not reconstructible randomize data");
        std::array<u32, MT_N> restoredState{};
        const char *p = stateHex.getData();
        for(int i = 0; i < MT_N; ++i) {
            u32 n = 0;
            for(int j = 0; j < 8; ++j) {
                const char c = p[i * 8 + j];
                const int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
                if(d < 0)
                    throw std::runtime_error("not reconstructible randomize data");
                n = (n << 4) | static_cast<u32>(d);
            }
            restoredState[i] = n;
        }
        // 所有校验成功后才替换状态，失败的 randomize 不破坏原有序列。
        ensureState();
        std::memcpy(state, restoredState.data(), sizeof(u32) * MT_N);
        left = leftVal;
        next = state + nextIdx;
        inited = true;
    }

    void RandomGen::marked() noexcept {
        if(_marked)
            return;
        Object::marked();
    }

    void RandomGen::randomizeNoArg() { initGenRand(static_cast<u32>(std::time(nullptr))); }

    void RandomGen::randomizeByKey(const u32 key[2]) { initByArray(key, 2); }

    void RandomGen::randomizeFrom(AtomTable &at, const Value &dict) {
        auto *dic = dynamic_cast<Dictionary *>(dict.asObject().unwrap());
        if(!dic)
            throw std::runtime_error("not reconstructible randomize data");
        const Value stateVal = dic->getProp(at.intern("state"_str));
        const Value leftVal = dic->getProp(at.intern("left"_str));
        const Value nextVal = dic->getProp(at.intern("next"_str));
        if(!stateVal.isString() || !leftVal.isInteger() || !nextVal.isInteger())
            throw std::runtime_error("not reconstructible randomize data");
        const Integer leftCount = leftVal.asInteger().unwrap();
        const Integer nextIndex = nextVal.asInteger().unwrap();
        if(!std::in_range<int>(leftCount) || !std::in_range<int>(nextIndex))
            throw std::runtime_error("not reconstructible randomize data");
        restoreFrom(*stateVal.asString().unwrap(), static_cast<int>(leftCount), static_cast<int>(nextIndex));
    }

    void RandomGen::call(vm::VMState &vmState, const u32 ret, const size_t argCount) {
        Value *args = argCount > 0 ? vmState.curFrame()->getArgs(argCount) : nullptr;
        auto *rg = vmState.rt.create<RandomGen>().get();
        if(argCount == 0) {
            rg->randomizeNoArg();
        } else if(args[0].isObject()) {
            rg->randomizeFrom(vmState.rt.atomTable, args[0]);
        } else {
            const u64 n = static_cast<u64>(args[0].asInteger().value());
            const u32 key[2] = { static_cast<u32>(n), static_cast<u32>(n >> 32) };
            rg->randomizeByKey(key);
        }
        vmState.regRef(ret) = Value{ rg };
    }

} // namespace cial

namespace cial::stdlib {
    void registerRandomLib(Context &context) {
        context.nativeType<RandomGen>()
            .method("randomize"_str, &stdlib::rgRandomize)
            .method("random"_str, &stdlib::rgRandom)
            .method("random32"_str, &stdlib::rgRandom32)
            .method("random63"_str, &stdlib::rgRandom63)
            .method("random64"_str, &stdlib::rgRandom64)
            .method("serialize"_str, &stdlib::rgSerialize);
    }


    Value rgRandomize(VM *vm, const Value &thisObj, const size_t argCount, Value *args) {
        auto *rg = dynamic_cast<RandomGen *>(thisObj.asObject().unwrap());
        if(argCount == 0) {
            rg->randomizeNoArg();
        } else if(args[0].isObject()) {
            rg->randomizeFrom(vm->rt().atomTable, args[0]);
        } else {
            const u64 n = static_cast<u64>(args[0].asInteger().value());
            const u32 key[2] = { static_cast<u32>(n), static_cast<u32>(n >> 32) };
            rg->randomizeByKey(key);
        }
        return Value{};
    }

    Value rgRandom(VM *, const Value &thisObj, size_t, Value *) {
        return Value{ Real{ dynamic_cast<RandomGen &>(*thisObj.asObject().unwrap()).random() } };
    }

    Value rgRandom32(VM *, const Value &thisObj, size_t, Value *) {
        return Value{ Integer{ static_cast<i64>(dynamic_cast<RandomGen &>(*thisObj.asObject().unwrap()).random32()) } };
    }

    Value rgRandom63(VM *, const Value &thisObj, size_t, Value *) {
        return Value{ Integer{ dynamic_cast<RandomGen &>(*thisObj.asObject().unwrap()).random63() } };
    }

    Value rgRandom64(VM *, const Value &thisObj, size_t, Value *) {
        return Value{ Integer{ dynamic_cast<RandomGen &>(*thisObj.asObject().unwrap()).random64() } };
    }

    Value rgSerialize(VM *vm, const Value &thisObj, size_t, Value *) {
        return dynamic_cast<RandomGen &>(*thisObj.asObject().unwrap()).serialize(vm);
    }

} // namespace cial::stdlib
