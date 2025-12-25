// Copyright (c) 2025/11/16
//
// IEEE 754 Double Precision Floating Point Tests
// Tests modernized C++20 implementation

#include <catch.hpp>
#include <limits>

#include "lexer/IEEETypes.hpp"

using namespace Cial::Syntax::IEEE;

TEST_CASE("IEEE 754 - Basic Constants") {
    SECTION("Constants are correctly defined") {
        REQUIRE(EXP_MAX == 1023);
        REQUIRE(EXP_MIN == -1022);
        REQUIRE(SIGNIFICAND_BITS == 52);
        REQUIRE(EXP_BIAS == 1023);

        REQUIRE(SIGN_MASK == 0x8000000000000000ull);
        REQUIRE(EXP_MASK == 0x7ff0000000000000ull);
        REQUIRE(SIGNIFICAND_MASK == 0x000fffffffffffffull);
        REQUIRE(SIGNIFICAND_MSB_MASK == 0x0008000000000000ull);
    }
}

TEST_CASE("IEEE 754 - Component Extraction") {
    SECTION("Sign bit extraction") {
        REQUIRE(get_sign(P_INF) == false);
        REQUIRE(get_sign(N_INF) == true);
        REQUIRE(get_sign(P_NaN) == false);
        REQUIRE(get_sign(N_NaN) == true);
    }

    SECTION("Exponent extraction") {
        REQUIRE(get_exponent(P_INF) == 1024); // 2047 - 1023 = 1024
        REQUIRE(get_exponent(N_INF) == 1024);
        REQUIRE(get_exponent(P_NaN) == 1024);
        REQUIRE(get_exponent(N_NaN) == 1024);
    }

    SECTION("Significand extraction") {
        REQUIRE(get_significand(P_INF) == 0);
        REQUIRE(get_significand(N_INF) == 0);
        REQUIRE(get_significand(P_NaN) == SIGNIFICAND_MSB_MASK);
        REQUIRE(get_significand(N_NaN) == SIGNIFICAND_MSB_MASK);
    }
}

TEST_CASE("IEEE 754 - Component Composition") {
    SECTION("Sign composition") {
        REQUIRE(make_sign(false) == 0ull);
        REQUIRE(make_sign(true) == SIGN_MASK);
    }

    SECTION("Exponent composition") {
        REQUIRE(make_exponent(0) == (static_cast<uint64_t>(EXP_BIAS) << SIGNIFICAND_BITS));
        REQUIRE(make_exponent(EXP_MAX) == (static_cast<uint64_t>(EXP_MAX + EXP_BIAS) << SIGNIFICAND_BITS));
        REQUIRE(make_exponent(EXP_MIN) == (static_cast<uint64_t>(1) << SIGNIFICAND_BITS));
    }

    SECTION("Significand composition") {
        REQUIRE(make_significand(0) == 0);
        REQUIRE(make_significand(SIGNIFICAND_MASK) == SIGNIFICAND_MASK);
    }
}

TEST_CASE("IEEE 754 - Special Value Checks") {
    SECTION("NaN detection") {
        REQUIRE(check_nan(P_NaN));
        REQUIRE(check_nan(N_NaN));
        REQUIRE_FALSE(check_nan(P_INF));
        REQUIRE_FALSE(check_nan(N_INF));
        REQUIRE_FALSE(check_nan(0));
    }

    SECTION("Infinity detection") {
        REQUIRE(check_inf(P_INF));
        REQUIRE(check_inf(N_INF));
        REQUIRE_FALSE(check_inf(P_NaN));
        REQUIRE_FALSE(check_inf(N_NaN));
        REQUIRE_FALSE(check_inf(0));
    }
}

TEST_CASE("IEEE 754 - Double Class") {
    SECTION("Construction from bits") {
        Double pos_inf(P_INF);
        REQUIRE(pos_inf.is_infinity());
        REQUIRE_FALSE(pos_inf.sign());
        REQUIRE(pos_inf.exponent() == 1024); // 2047 - 1023 = 1024
        REQUIRE(pos_inf.significand() == 0);

        Double neg_inf(N_INF);
        REQUIRE(neg_inf.is_infinity());
        REQUIRE(neg_inf.sign());
        REQUIRE(neg_inf.exponent() == 1024); // 2047 - 1023 = 1024
        REQUIRE(neg_inf.significand() == 0);

        Double nan_val(P_NaN);
        REQUIRE(nan_val.is_nan());
        REQUIRE_FALSE(nan_val.is_finite());
    }

    SECTION("Construction from double") {
        Double zero(0.0);
        REQUIRE(zero.is_finite());
        REQUIRE_FALSE(zero.sign());
        REQUIRE(zero.exponent() == -EXP_BIAS);

        Double neg_zero(-0.0);
        REQUIRE(neg_zero.is_finite());
        REQUIRE(neg_zero.sign());

        Double one(1.0);
        REQUIRE(one.is_finite());
        REQUIRE_FALSE(one.sign());
        REQUIRE(one.exponent() == 0);
    }

    SECTION("Static factory methods") {
        auto pos_inf = Double::positive_infinity();
        REQUIRE(pos_inf.is_infinity());
        REQUIRE_FALSE(pos_inf.sign());

        auto neg_inf = Double::negative_infinity();
        REQUIRE(neg_inf.is_infinity());
        REQUIRE(neg_inf.sign());

        auto qnan = Double::quiet_nan();
        REQUIRE(qnan.is_nan());

        auto snan = Double::signaling_nan();
        REQUIRE(snan.is_nan());
    }
}

TEST_CASE("IEEE 754 - Bit Manipulation Utilities") {
    SECTION("Bit extraction") {
        uint64_t test_val = 0b1010'1100'1110'0000ull;
        REQUIRE(bit_extract(test_val, 4, 4) == 0b1110);
        REQUIRE(bit_extract(test_val, 8, 4) == 0b1100);
        REQUIRE(bit_extract(test_val, 0, 16) == test_val);
    }

    SECTION("Bit insertion") {
        uint64_t base = 0b0000'0000'0000'0000ull;
        uint64_t modified = bit_insert(base, 4, 4, 0b1010ull);
        REQUIRE(bit_extract(modified, 4, 4) == 0b1010);

        uint64_t complex = bit_insert(base, 8, 8, 0b1111'0000ull);
        REQUIRE(bit_extract(complex, 8, 8) == 0b1111'0000);
    }
}

TEST_CASE("IEEE 754 - C++20 Concepts") {
    SECTION("IEEE754Double concept") {
        static_assert(IEEE754Double<double>);
        static_assert(!IEEE754Double<float>);
        static_assert(!IEEE754Double<int>);
        static_assert(!IEEE754Double<long double>);
    }
}

TEST_CASE("IEEE 754 - Constexpr Support") {
    SECTION("Compile-time evaluation") {
        // Test that functions can be evaluated at compile time
        constexpr bool test1 = check_nan(P_NaN);
        constexpr bool test2 = check_inf(P_INF);
        constexpr int32_t exp = get_exponent(P_INF);
        constexpr bool sign = get_sign(N_INF);
        constexpr uint64_t bits = double_to_bits(1.0);

        static_assert(test1 == true);
        static_assert(test2 == true);
        static_assert(exp == 1024); // 2047 - 1023 = 1024
        static_assert(sign == true);

        REQUIRE(test1);
        REQUIRE(test2);
        REQUIRE(exp == 1024); // 2047 - 1023 = 1024
        REQUIRE(sign);
    }

    SECTION("Double class constexpr") {
        constexpr Double pos_inf(P_INF);
        static_assert(pos_inf.is_infinity());
        static_assert(!pos_inf.sign());

        constexpr Double zero(0.0);
        static_assert(zero.is_finite());
        static_assert(!zero.sign());

        REQUIRE(pos_inf.is_infinity());
        REQUIRE(zero.is_finite());
    }
}

TEST_CASE("IEEE 754 - Real-world Values") {
    SECTION("Common double values") {
        // Test with actual double values
        double values[] = { 1.0, -1.0, 0.5, -0.5, 2.0, -2.0, 42.0, -42.0 };

        for(double val : values) {
            Double d(val);
            REQUIRE(d.is_finite());
            REQUIRE(d.value() == val);

            uint64_t bits = double_to_bits(val);
            REQUIRE(d.bits() == bits);

            double reconstructed = bits_to_double(bits);
            REQUIRE(reconstructed == val);
        }
    }

    SECTION("Special double values") {
        double inf = std::numeric_limits<double>::infinity();
        double neg_inf = -std::numeric_limits<double>::infinity();
        double nan = std::numeric_limits<double>::quiet_NaN();

        Double d_inf(inf);
        REQUIRE(d_inf.is_infinity());
        REQUIRE_FALSE(d_inf.sign());

        Double d_neg_inf(neg_inf);
        REQUIRE(d_neg_inf.is_infinity());
        REQUIRE(d_neg_inf.sign());

        Double d_nan(nan);
        REQUIRE(d_nan.is_nan());
        REQUIRE_FALSE(d_nan.is_finite());
    }
}