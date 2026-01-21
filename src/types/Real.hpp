/*
 * Copyright (c) 2024/5/6 下午8:16
 *
 * /\  _` \   __          /\_ \  /\_ \
 * \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
 *  \ \ \/_/_\/\ \  /'__`\  \ \ \  \ \ \   /'__`\  /' _ `\  /'_ `\
 *   \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
 *    \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
 *     \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
 *                                                            /\____/
 *                                                            \_/__/
 *
 */

#pragma once

#include <bit>
#include <complex>
#include <cstdint>

// 单精度（32位）：  S EEEEEEEE MMMMMMMMMMMMMMMMMMMMMMM
//                31  30-23            22-0
//
// 双精度（64位）：  S EEEEEEEEEEE MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
//                63    62-52                 51-0
// S = 符号位（0=正，1=负）
// E = 指数（偏置编码，不是原码或补码）
// M = 尾数（fraction），也叫有效数（significand）

namespace cial {

    // double related constants
    static constexpr int32_t EXP_MAX = 1023;
    static constexpr int32_t EXP_MIN = -1022;
    static constexpr int64_t SIGNIFICAND_BITS = 52;
    static constexpr uint64_t EXP_BIAS = 1023;

    // component extraction bit masks
    static constexpr uint64_t SIGN_MASK = 0x8000'0000'0000'0000ull;
    static constexpr uint64_t EXP_MASK = 0x7ff0'0000'0000'0000ull;
    static constexpr uint64_t SIGNIFICAND_MASK = 0x000f'ffff'ffff'ffffull;
    static constexpr uint64_t SIGNIFICAND_MSB_MASK = 0x0008'0000'0000'0000ull;

    // Special values
    static constexpr uint64_t P_NaN = EXP_MASK | SIGNIFICAND_MSB_MASK;
    static constexpr uint64_t N_NaN = SIGN_MASK | P_NaN;
    static constexpr uint64_t P_INF = EXP_MASK;
    static constexpr uint64_t N_INF = SIGN_MASK | P_INF;

    // Special value check functions (replacing macros)
    [[nodiscard]] constexpr bool checkNan(const uint64_t bits) noexcept {
        return (bits & EXP_MASK) == EXP_MASK && (bits & SIGNIFICAND_MASK) != 0;
    }

    [[nodiscard]] constexpr bool checkInf(const uint64_t bits) noexcept {
        return (bits & EXP_MASK) == EXP_MASK && (bits & SIGNIFICAND_MASK) == 0;
    }

    [[nodiscard]] constexpr uint64_t doubleToBits(double value) noexcept { return std::bit_cast<uint64_t>(value); }

    [[nodiscard]] constexpr double bitsToDouble(const uint64_t bits) noexcept { return std::bit_cast<double>(bits); }

    // Component extraction functions
    [[nodiscard]] constexpr bool hasSign(const uint64_t bits) noexcept { return (bits & SIGN_MASK) != 0; }

    [[nodiscard]] constexpr int32_t getExponent(const uint64_t bits) noexcept {
        const uint32_t raw = bits >> SIGNIFICAND_BITS & (1 << 11) - 1;
        if(raw == 0) {
            return EXP_MIN;
        }
        if(raw == 0x7FF) {
            return checkNan(bits) ? 0 : 9999;
        }
        return static_cast<int32_t>(raw) - EXP_MAX;
    }

    [[nodiscard]] constexpr uint64_t getSignificand(const uint64_t bits) noexcept { return bits & SIGNIFICAND_MASK; }

    // Component composition functions
    [[nodiscard]] constexpr uint64_t makeSign(const bool negative) noexcept { return negative ? SIGN_MASK : 0ull; }

    [[nodiscard]] constexpr uint64_t makeExponent(const int32_t exp) noexcept {
        return (exp + EXP_BIAS) << SIGNIFICAND_BITS;
    }

    [[nodiscard]] constexpr uint64_t makeSignificand(const uint64_t significand) noexcept {
        return significand & SIGNIFICAND_MASK;
    }

    // High-level IEEE 754 double precision utilities
    class Real {
    public:
        constexpr explicit Real() noexcept = default;
        constexpr explicit Real(const uint64_t bits) noexcept : _bits(bits) {}
        constexpr explicit Real(const double value) noexcept : _bits(doubleToBits(value)) {}

        [[nodiscard]] constexpr bool sign() const noexcept { return hasSign(_bits); }
        [[nodiscard]] constexpr int32_t exponent() const noexcept { return getExponent(_bits); }
        [[nodiscard]] constexpr uint64_t significand() const noexcept { return getSignificand(_bits); }

        [[nodiscard]] constexpr bool isNan() const noexcept { return checkNan(_bits); }
        [[nodiscard]] constexpr bool isInfinity() const noexcept { return checkInf(_bits); }
        [[nodiscard]] constexpr bool isFinite() const noexcept { return !isNan() && !isInfinity(); }

        [[nodiscard]] constexpr double value() const noexcept { return bitsToDouble(_bits); }
        [[nodiscard]] constexpr uint64_t bits() const noexcept { return _bits; }

        Real operator+(const Real &real) const { return Real(this->value() + real.value()); }

        Real operator-(const Real &real) const { return Real(this->value() - real.value()); }

        Real operator*(const Real &real) const { return Real(this->value() * real.value()); }

        Real operator/(const Real &real) const { return Real(this->value() / real.value()); }

        bool operator==(const Real &real) const noexcept { return value() == real.value(); }

        std::partial_ordering operator<=>(const Real &real) const { return value() <=> real.value(); }

        // Static factory methods for special values
        [[nodiscard]] static constexpr Real positiveInf() noexcept { return Real(P_INF); }
        [[nodiscard]] static constexpr Real negativeInf() noexcept { return Real(N_INF); }
        [[nodiscard]] static constexpr Real quietNan() noexcept { return Real(P_NaN); }
        [[nodiscard]] static constexpr Real signalingNan() noexcept { return Real(N_NaN); }

        friend std::ostream &operator<<(std::ostream &os, const Real &rhs) { return os << rhs.value(); }

    private:
        std::uint64_t _bits{ 0 };
    };

} // namespace cial
