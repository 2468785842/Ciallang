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
        context.nativeType<MathNS>()
            .doc("数学方法、常量与随机数；作为命名空间直接 Math.abs(...) 调用。")
            .method("abs"_str, &stdlib::mathAbs, api::signature({ { .name = "n", .type = "Real" } }, "Real", "绝对值"))
            .method("acos"_str, &stdlib::mathAcos,
                    api::signature({ { .name = "n", .type = "Real" } }, "Real", "反余弦"))
            .method("asin"_str, &stdlib::mathAsin,
                    api::signature({ { .name = "n", .type = "Real" } }, "Real", "反正弦"))
            .method("atan"_str, &stdlib::mathAtan,
                    api::signature({ { .name = "n", .type = "Real" } }, "Real", "反正切"))
            .method("atan2"_str, &stdlib::mathAtan2,
                    api::signature({ { .name = "y", .type = "Real" }, { .name = "x", .type = "Real" } }, "Real",
                                   "二参数反正切"))
            .method("ceil"_str, &stdlib::mathCeil,
                    api::signature({ { .name = "n", .type = "Real" } }, "Real", "向上取整（返回 Real）"))
            .method("exp"_str, &stdlib::mathExp, api::signature({ { .name = "n", .type = "Real" } }, "Real", "e 的幂"))
            .method("floor"_str, &stdlib::mathFloor,
                    api::signature({ { .name = "n", .type = "Real" } }, "Real", "向下取整（返回 Real）"))
            .method("log"_str, &stdlib::mathLog,
                    api::signature({ { .name = "n", .type = "Real" } }, "Real", "自然对数"))
            .method("pow"_str, &stdlib::mathPow,
                    api::signature({ { .name = "base", .type = "Real" }, { .name = "exponent", .type = "Real" } },
                                   "Real", "幂"))
            .method("max"_str, &stdlib::mathMax,
                    api::signature({ { .name = "values", .type = "Real", .collapse = true } }, "Real", "最大值"))
            .method("min"_str, &stdlib::mathMin,
                    api::signature({ { .name = "values", .type = "Real", .collapse = true } }, "Real", "最小值"))
            .method("random"_str, &stdlib::mathRandom, api::signature({}, "Real", "[0,1) 随机数"))
            .method("round"_str, &stdlib::mathRound,
                    api::signature({ { .name = "n", .type = "Real" } }, "Real", "四舍五入"))
            .method("sin"_str, &stdlib::mathSin, api::signature({ { .name = "n", .type = "Real" } }, "Real", "正弦"))
            .method("cos"_str, &stdlib::mathCos, api::signature({ { .name = "n", .type = "Real" } }, "Real", "余弦"))
            .method("sqrt"_str, &stdlib::mathSqrt,
                    api::signature({ { .name = "n", .type = "Real" } }, "Real", "平方根"))
            .method("tan"_str, &stdlib::mathTan, api::signature({ { .name = "n", .type = "Real" } }, "Real", "正切"))
            // 常量属性 getter 须为无捕获 lambda（NativePropGetter 是裸函数指针）
            .property(
                "E"_str, [](const Value &) { return Value{ Real{ M_E } }; }, api::signature({}, "Real", "自然对数底 e"))
            .property(
                "LOG2E"_str, [](const Value &) { return Value{ Real{ M_LOG2E } }; },
                api::signature({}, "Real", "log2(e)，2 为底对数的换底常数"))
            .property(
                "LOG10E"_str, [](const Value &) { return Value{ Real{ M_LOG10E } }; },
                api::signature({}, "Real", "log10(e)，10 为底对数的换底常数"))
            .property(
                "LN10"_str, [](const Value &) { return Value{ Real{ M_LN10 } }; },
                api::signature({}, "Real", "10 的自然对数"))
            .property(
                "LN2"_str, [](const Value &) { return Value{ Real{ M_LN2 } }; },
                api::signature({}, "Real", "2 的自然对数"))
            .property(
                "PI"_str, [](const Value &) { return Value{ Real{ M_PI } }; }, api::signature({}, "Real", "圆周率 π"))
            .property(
                "SQRT1_2"_str, [](const Value &) { return Value{ Real{ M_SQRT2 / 2 } }; },
                api::signature({}, "Real", "1/2 的平方根"))
            .property(
                "SQRT2"_str, [](const Value &) { return Value{ Real{ M_SQRT2 } }; },
                api::signature({}, "Real", "2 的平方根"));
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

    void MathNS::markChildren() noexcept {
        if(rgClass)
            rgClass->marked();
    }
} // namespace cial
