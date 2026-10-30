// Copyright (c) 2025/11/16
//
// IEEE 754 双精度位工具测试（对准 src/types/Real.hpp，原 lexer/IEEETypes.hpp 已并入该头）

#include <catch2/catch_all.hpp>

#include "types/Real.hpp"

using namespace cial;

TEST_CASE("IEEE 754 - 常量与掩码") {
    SECTION("常量定义正确") {
        REQUIRE(EXP_MAX == 1023);
        REQUIRE(EXP_MIN == -1022);
        REQUIRE(SIGNIFICAND_BITS == 52);
        REQUIRE(EXP_BIAS == 1023);

        REQUIRE(SIGN_MASK == 0x8000'0000'0000'0000ull);
        REQUIRE(EXP_MASK == 0x7ff0'0000'0000'0000ull);
        REQUIRE(SIGNIFICAND_MASK == 0x000f'ffff'ffff'ffffull);
        REQUIRE(SIGNIFICAND_MSB_MASK == 0x0008'0000'0000'0000ull);
    }

    SECTION("特殊值位模式") {
        REQUIRE(P_INF == EXP_MASK);
        REQUIRE(N_INF == (SIGN_MASK | EXP_MASK));
        REQUIRE(checkInf(P_INF));
        REQUIRE(checkInf(N_INF));
        REQUIRE(checkNan(P_NaN));
        REQUIRE(checkNan(N_NaN));
    }
}

TEST_CASE("IEEE 754 - 特殊值判定") {
    SECTION("NaN 检测") {
        REQUIRE(checkNan(P_NaN));
        REQUIRE(checkNan(N_NaN));
        REQUIRE_FALSE(checkNan(P_INF));
        REQUIRE_FALSE(checkNan(N_INF));
        REQUIRE_FALSE(checkNan(0));
    }

    SECTION("无穷检测") {
        REQUIRE(checkInf(P_INF));
        REQUIRE(checkInf(N_INF));
        REQUIRE_FALSE(checkInf(P_NaN));
        REQUIRE_FALSE(checkInf(N_NaN));
        REQUIRE_FALSE(checkInf(0));
    }

    SECTION("位往返保持判定结果") {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const double inf = std::numeric_limits<double>::infinity();
        REQUIRE(checkNan(doubleToBits(nan)));
        REQUIRE(checkInf(doubleToBits(inf)));
        REQUIRE(checkInf(doubleToBits(-inf)));
        REQUIRE_FALSE(checkNan(doubleToBits(1.0)));
        REQUIRE(bitsToDouble(doubleToBits(nan)) != bitsToDouble(doubleToBits(nan)));
    }
}

TEST_CASE("IEEE 754 - 分量提取") {
    SECTION("符号位提取") {
        REQUIRE_FALSE(hasSign(doubleToBits(1.0)));
        REQUIRE(hasSign(doubleToBits(-1.0)));
        REQUIRE(hasSign(N_INF));
        REQUIRE_FALSE(hasSign(P_NaN));
    }

    SECTION("指数提取") {
        // 规格化数：raw - EXP_BIAS
        REQUIRE(getExponent(doubleToBits(1.0)) == 0);
        REQUIRE(getExponent(doubleToBits(2.0)) == 1);
        REQUIRE(getExponent(doubleToBits(0.5)) == -1);

        // 零/次规格化 raw==0 记为 EXP_MIN
        REQUIRE(getExponent(0) == EXP_MIN);

        // 全指数位：NaN 记 0、无穷记 9999（该头文件的哨兵约定）
        REQUIRE(getExponent(P_NaN) == 0);
        REQUIRE(getExponent(P_INF) == 9999);
        REQUIRE(getExponent(N_INF) == 9999);
    }

    SECTION("尾数提取") {
        REQUIRE(getSignificand(P_INF) == 0);
        REQUIRE(getSignificand(P_NaN) == SIGNIFICAND_MSB_MASK);
        REQUIRE(getSignificand(doubleToBits(1.0)) == 0);
    }
}

TEST_CASE("IEEE 754 - 分量合成") {
    SECTION("符号合成") {
        REQUIRE(makeSign(false) == 0ull);
        REQUIRE(makeSign(true) == SIGN_MASK);
    }

    SECTION("指数合成") {
        REQUIRE(makeExponent(0) == (static_cast<u64>(EXP_BIAS) << SIGNIFICAND_BITS));
        REQUIRE(makeExponent(EXP_MAX) == (static_cast<u64>(EXP_MAX + EXP_BIAS) << SIGNIFICAND_BITS));
        REQUIRE(makeExponent(EXP_MIN) == (static_cast<u64>(1) << SIGNIFICAND_BITS));
    }

    SECTION("尾数合成") {
        REQUIRE(makeSignificand(0) == 0);
        REQUIRE(makeSignificand(SIGNIFICAND_MASK) == SIGNIFICAND_MASK);
        REQUIRE(makeSignificand(~0ull) == SIGNIFICAND_MASK);
    }
}

TEST_CASE("IEEE 754 - Real 类") {
    SECTION("从位模式构造") {
        const Real posInf{ P_INF };
        REQUIRE(posInf.isInfinity());
        REQUIRE_FALSE(posInf.sign());
        REQUIRE(posInf.significand() == 0);

        const Real negInf{ N_INF };
        REQUIRE(negInf.isInfinity());
        REQUIRE(negInf.sign());

        const Real nanVal{ P_NaN };
        REQUIRE(nanVal.isNan());
        REQUIRE_FALSE(nanVal.isFinite());
    }

    SECTION("从 double 构造") {
        const Real zero{ 0.0 };
        REQUIRE(zero.isFinite());
        REQUIRE_FALSE(zero.sign());
        REQUIRE(zero.exponent() == EXP_MIN);

        const Real negZero{ -0.0 };
        REQUIRE(negZero.isFinite());
        REQUIRE(negZero.sign());

        const Real one{ 1.0 };
        REQUIRE(one.isFinite());
        REQUIRE_FALSE(one.sign());
        REQUIRE(one.exponent() == 0);
    }

    SECTION("静态工厂") {
        REQUIRE(Real::positiveInf().isInfinity());
        REQUIRE_FALSE(Real::positiveInf().sign());
        REQUIRE(Real::negativeInf().isInfinity());
        REQUIRE(Real::negativeInf().sign());
        REQUIRE(Real::quietNan().isNan());
        REQUIRE(Real::signalingNan().isNan());
    }

    SECTION("值与位往返") {
        const double values[] = { 1.0, -1.0, 0.5, -0.5, 2.0, -2.0, 42.0, -42.0 };
        for(const double val : values) {
            const Real r{ val };
            REQUIRE(r.isFinite());
            REQUIRE(r.value() == val);
            REQUIRE(r.bits() == doubleToBits(val));
            REQUIRE(bitsToDouble(r.bits()) == val);
        }
    }

    SECTION("算术与比较") {
        const Real a{ 10.5 }, b{ 2.5 };
        REQUIRE((a + b).value() == Catch::Approx(13.0));
        REQUIRE((a - b).value() == Catch::Approx(8.0));
        REQUIRE((a * b).value() == Catch::Approx(26.25));
        REQUIRE((a / b).value() == Catch::Approx(4.2));

        REQUIRE(a == Real{ 10.5 });
        REQUIRE((b < a));
        REQUIRE((a > b));
        REQUIRE_FALSE(Real::quietNan() == Real::quietNan());
    }
}

TEST_CASE("IEEE 754 - constexpr 支持") {
    SECTION("编译期求值") {
        constexpr bool nanIsNan = checkNan(P_NaN);
        constexpr bool infIsInf = checkInf(P_INF);
        constexpr u64 oneBits = doubleToBits(1.0);

        static_assert(nanIsNan);
        static_assert(infIsInf);
        static_assert(oneBits == 0x3ff0'0000'0000'0000ull);
        static_assert(getExponent(doubleToBits(1.0)) == 0);
        static_assert(hasSign(N_INF));

        REQUIRE(nanIsNan);
        REQUIRE(infIsInf);
    }

    SECTION("Real 类 constexpr") {
        constexpr Real posInf{ P_INF };
        static_assert(posInf.isInfinity());
        static_assert(!posInf.sign());

        constexpr Real zero{ 0.0 };
        static_assert(zero.isFinite());
        static_assert(!zero.sign());

        constexpr Real one{ 1.0 };
        static_assert(one.exponent() == 0);

        REQUIRE(posInf.isInfinity());
        REQUIRE(zero.isFinite());
    }
}
