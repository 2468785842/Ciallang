// Copyright (c) 2026/10/25
//

#define _USE_MATH_DEFINES

#include "MathLib.hpp"
#include "RandomLib.hpp"
#include "runtime/Context.hpp"


#include <cmath>
#include <cstdint>
#include <ctime>
#include <limits>

namespace cial::stdlib {
    void registerMathLib(Context &context) {
        context.nativeType<MathNS>().method("abs"_str, &stdlib::mathAbs);
        context.nativeType<MathNS>().method("acos"_str, &stdlib::mathAcos);
        context.nativeType<MathNS>().method("asin"_str, &stdlib::mathAsin);
        context.nativeType<MathNS>().method("atan"_str, &stdlib::mathAtan);
        context.nativeType<MathNS>().method("atan2"_str, &stdlib::mathAtan2);
        context.nativeType<MathNS>().method("ceil"_str, &stdlib::mathCeil);
        context.nativeType<MathNS>().method("exp"_str, &stdlib::mathExp);
        context.nativeType<MathNS>().method("floor"_str, &stdlib::mathFloor);
        context.nativeType<MathNS>().method("log"_str, &stdlib::mathLog);
        context.nativeType<MathNS>().method("pow"_str, &stdlib::mathPow);
        context.nativeType<MathNS>().method("max"_str, &stdlib::mathMax);
        context.nativeType<MathNS>().method("min"_str, &stdlib::mathMin);
        context.nativeType<MathNS>().method("random"_str, &stdlib::mathRandom);
        context.nativeType<MathNS>().method("round"_str, &stdlib::mathRound);
        context.nativeType<MathNS>().method("sin"_str, &stdlib::mathSin);
        context.nativeType<MathNS>().method("cos"_str, &stdlib::mathCos);
        context.nativeType<MathNS>().method("sqrt"_str, &stdlib::mathSqrt);
        context.nativeType<MathNS>().method("tan"_str, &stdlib::mathTan);
        // 常量属性 getter 须为无捕获 lambda（NativePropGetter 是裸函数指针）
        context.nativeType<MathNS>().property("E"_str, [](const Value &) { return Value{ Real{ M_E } }; });
        context.nativeType<MathNS>().property("LOG2E"_str, [](const Value &) { return Value{ Real{ M_LOG2E } }; });
        context.nativeType<MathNS>().property("LOG10E"_str, [](const Value &) { return Value{ Real{ M_LOG10E } }; });
        context.nativeType<MathNS>().property("LN10"_str, [](const Value &) { return Value{ Real{ M_LN10 } }; });
        context.nativeType<MathNS>().property("LN2"_str, [](const Value &) { return Value{ Real{ M_LN2 } }; });
        context.nativeType<MathNS>().property("PI"_str, [](const Value &) { return Value{ Real{ M_PI } }; });
        context.nativeType<MathNS>().property("SQRT1_2"_str, [](const Value &) { return Value{ Real{ M_SQRT2 / 2 } }; });
        context.nativeType<MathNS>().property("SQRT2"_str, [](const Value &) { return Value{ Real{ M_SQRT2 } }; });
        auto *mathNS = context.registerClass<MathNS>("Math"_str);
        // Math.RandomGenerator 类对象（new Math.RandomGenerator(seed) 构造入口）
        mathNS->setRandomGenerator(context.rt().atomTable.intern("RandomGenerator"_str),
                                   context.rt().create<RandomGen>().get());
    }

    namespace {
        // TJS2 tTJSXorshift 同款（tjsMath.cpp）：4 种子 Xorshift，time 播散
        struct Xorshift {
            std::uint32_t seeds[4]{};

            Xorshift() {
                auto seed = static_cast<std::uint32_t>(std::time(nullptr));
                for(int i = 0; i < 4; ++i)
                    seeds[i] = seed = 1812433253 * (seed ^ (seed >> 30)) + static_cast<std::uint32_t>(i);
            }

            std::uint32_t next() {
                const std::uint32_t t = seeds[0] ^ (seeds[0] << 11);
                seeds[0] = seeds[1];
                seeds[1] = seeds[2];
                seeds[2] = seeds[3];
                seeds[3] = (seeds[3] ^ (seeds[3] >> 19)) ^ (t ^ (t >> 8));
                return seeds[3];
            }

            double real() { // [0,1)
                return static_cast<double>(next()) / 4294967296.0; // / (2^32)
            }
        };

        Xorshift &rng() {
            static Xorshift instance{};
            return instance;
        }

        // 参数取 Real（TJS2 AsReal 语义；void 得 0.0）
        double arg(const Value &v) {
            if(v.isVoid())
                return 0.0;
            const auto r = v.asReal();
            return r.isFailed() ? 0.0 : r.value().value();
        }

        Value realValue(const double v) { return Value{ Real{ v } }; }
    } // namespace

    Value mathAbs(VM *, const Value &, size_t n, Value *args) {
        if(n < 1)
            throw std::runtime_error("Math.abs expects a number");
        return realValue(std::fabs(arg(args[0])));
    }

    Value mathAcos(VM *, const Value &, size_t n, Value *args) {
        if(n < 1)
            throw std::runtime_error("Math.acos expects a number");
        return realValue(std::acos(arg(args[0])));
    }

    Value mathAsin(VM *, const Value &, size_t n, Value *args) {
        if(n < 1)
            throw std::runtime_error("Math.asin expects a number");
        return realValue(std::asin(arg(args[0])));
    }

    Value mathAtan(VM *, const Value &, size_t n, Value *args) {
        if(n < 1)
            throw std::runtime_error("Math.atan expects a number");
        return realValue(std::atan(arg(args[0])));
    }

    Value mathAtan2(VM *, const Value &, size_t n, Value *args) {
        if(n < 2)
            throw std::runtime_error("Math.atan2 expects two numbers");
        return realValue(std::atan2(arg(args[0]), arg(args[1])));
    }

    Value mathCeil(VM *, const Value &, size_t n, Value *args) {
        if(n < 1)
            throw std::runtime_error("Math.ceil expects a number");
        return realValue(std::ceil(arg(args[0])));
    }

    Value mathExp(VM *, const Value &, size_t n, Value *args) {
        if(n < 1)
            throw std::runtime_error("Math.exp expects a number");
        return realValue(std::exp(arg(args[0])));
    }

    Value mathFloor(VM *, const Value &, size_t n, Value *args) {
        if(n < 1)
            throw std::runtime_error("Math.floor expects a number");
        return realValue(std::floor(arg(args[0])));
    }

    Value mathLog(VM *, const Value &, size_t n, Value *args) {
        if(n < 1)
            throw std::runtime_error("Math.log expects a number");
        return realValue(std::log(arg(args[0])));
    }

    Value mathPow(VM *, const Value &, size_t n, Value *args) {
        if(n < 2)
            throw std::runtime_error("Math.pow expects two numbers");
        return realValue(std::pow(arg(args[0]), arg(args[1])));
    }

    // NaN 传播与 ±0 语义对齐原版位级实现（max：+0 压负值；min：-0 压正值）
    Value mathMax(VM *, const Value &, size_t n, Value *args) {
        double r = -std::numeric_limits<double>::infinity();
        for(size_t i = 0; i < n; ++i) {
            const double v = arg(args[i]);
            if(std::isnan(v))
                return realValue(std::numeric_limits<double>::quiet_NaN());
            if(v == 0.0) {
                if(std::signbit(r))
                    r = v; // +0 与负 r
            } else if(r < v) {
                r = v;
            }
        }
        return realValue(r);
    }

    Value mathMin(VM *, const Value &, size_t n, Value *args) {
        double r = std::numeric_limits<double>::infinity();
        for(size_t i = 0; i < n; ++i) {
            const double v = arg(args[i]);
            if(std::isnan(v))
                return realValue(std::numeric_limits<double>::quiet_NaN());
            if(v == 0.0) {
                if(!std::signbit(r))
                    r = v; // -0 与正 r
            } else if(v < r) {
                r = v;
            }
        }
        return realValue(r);
    }

    Value mathRandom(VM *, const Value &, size_t, Value *) { return realValue(rng().real()); }

    // TJS2：floor(x + 0.5)
    Value mathRound(VM *, const Value &, size_t n, Value *args) {
        if(n < 1)
            throw std::runtime_error("Math.round expects a number");
        return realValue(std::floor(arg(args[0]) + 0.5));
    }

    Value mathSin(VM *, const Value &, size_t n, Value *args) {
        if(n < 1)
            throw std::runtime_error("Math.sin expects a number");
        return realValue(std::sin(arg(args[0])));
    }

    Value mathCos(VM *, const Value &, size_t n, Value *args) {
        if(n < 1)
            throw std::runtime_error("Math.cos expects a number");
        return realValue(std::cos(arg(args[0])));
    }

    Value mathSqrt(VM *, const Value &, size_t n, Value *args) {
        if(n < 1)
            throw std::runtime_error("Math.sqrt expects a number");
        return realValue(std::sqrt(arg(args[0])));
    }

    Value mathTan(VM *, const Value &, size_t n, Value *args) {
        if(n < 1)
            throw std::runtime_error("Math.tan expects a number");
        return realValue(std::tan(arg(args[0])));
    }

} // namespace cial::stdlib

namespace cial {
    Value MathNS::getProp(const Atom a) {
        if(a == rgAtom && rgClass)
            return Value{ rgClass };
        throw std::runtime_error("Math member not found");
    }

    void MathNS::marked() noexcept {
        if(_marked)
            return;
        Object::marked();
        if(rgClass)
            rgClass->marked();
    }
} // namespace cial
