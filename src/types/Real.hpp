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
#include <concepts>
#include <cstdint>
#include <limits>
#include <type_traits>

/* IEEE double manipulation support
 * (TJS requires IEEE double(64-bit float) native support on machine or C++ compiler)
 */

// 63 62       52 51                         0
// +-+-----------+---------------------------+
// |s|    exp    |         significand       |
// +-+-----------+---------------------------+
// s = sign,  negative if this is 1, otherwise positive.

namespace cial {

    // Concepts for IEEE floating point operations
    template <typename T>
    concept IEEE754Double = std::same_as<T, double> && sizeof(T) == 8 && std::numeric_limits<T>::is_iec559;

    // double related constants
    static constexpr int64_t EXP_MAX = 1023;
    static constexpr int64_t EXP_MIN = -1022;
    static constexpr int64_t SIGNIFICAND_BITS = 52;
    static constexpr uint64_t EXP_BIAS = 1023;

    // component extraction bit masks
    static constexpr uint64_t SIGN_MASK = 0x8000000000000000ull;
    static constexpr uint64_t EXP_MASK = 0x7ff0000000000000ull;
    static constexpr uint64_t SIGNIFICAND_MASK = 0x000fffffffffffffull;
    static constexpr uint64_t SIGNIFICAND_MSB_MASK = 0x0008000000000000ull;

    // Type-safe bit manipulation utilities
    template <typename T>
    [[nodiscard]] constexpr std::enable_if_t<std::is_unsigned_v<T>, T> bitExtract(T value, int start,
                                                                                  int width) noexcept {
        return (value >> start) & ((T{ 1 } << width) - 1);
    }

    template <typename T>
    [[nodiscard]] constexpr std::enable_if_t<std::is_unsigned_v<T>, T> bit_insert(T value, int start, int width,
                                                                                  T insert) noexcept {
        T mask = ((T{ 1 } << width) - 1) << start;
        return (value & ~mask) | ((insert << start) & mask);
    }

    // Component extraction functions
    [[nodiscard]] constexpr bool getSign(const uint64_t bits) noexcept { return (bits & SIGN_MASK) != 0; }

    [[nodiscard]] constexpr int32_t getExponent(const uint64_t bits) noexcept {
        return static_cast<int32_t>(bitExtract(bits, SIGNIFICAND_BITS, 11) - EXP_BIAS);
    }

    [[nodiscard]] constexpr uint64_t getSignificand(const uint64_t bits) noexcept { return bits & SIGNIFICAND_MASK; }

    // Component composition functions
    [[nodiscard]] constexpr uint64_t makeSign(const bool negative) noexcept { return negative ? SIGN_MASK : 0ull; }

    [[nodiscard]] constexpr uint64_t makeExponent(const int32_t exp) noexcept {
        return (exp + EXP_BIAS) << SIGNIFICAND_BITS;
    }

    [[nodiscard]] constexpr uint64_t make_significand(const uint64_t significand) noexcept {
        return significand & SIGNIFICAND_MASK;
    }

    // Special values
    static constexpr uint64_t P_NaN = EXP_MASK | SIGNIFICAND_MSB_MASK;
    static constexpr uint64_t N_NaN = SIGN_MASK | P_NaN;
    static constexpr uint64_t P_INF = EXP_MASK;
    static constexpr uint64_t N_INF = SIGN_MASK | P_INF;

    // Special value check functions (replacing macros)
    [[nodiscard]] constexpr bool checkNan(const uint64_t bits) noexcept {
        const bool exp_all_set = (bits & EXP_MASK) == EXP_MASK;
        const bool significand_nonzero = (bits & SIGNIFICAND_MASK) != 0;
        return exp_all_set && significand_nonzero;
    }

    [[nodiscard]] constexpr bool checkInf(const uint64_t bits) noexcept {
        const bool exp_all_set = (bits & EXP_MASK) == EXP_MASK;
        const bool significand_zero = (bits & SIGNIFICAND_MASK) == 0;
        return exp_all_set && significand_zero;
    }

    // Modern C++20 bit cast support for double <-> uint64_t conversion
    template <IEEE754Double T = double>
    [[nodiscard]] constexpr uint64_t doubleToBits(T value) noexcept {
        return std::bit_cast<uint64_t>(value);
    }

    template <IEEE754Double T = double>
    [[nodiscard]] constexpr T bitsToDouble(const uint64_t bits) noexcept {
        return std::bit_cast<T>(bits);
    }

    // High-level IEEE 754 double precision utilities
    class Real {
    public:
        constexpr explicit Real() noexcept = default;
        constexpr explicit Real(const uint64_t bits) noexcept : _bits(bits) {}
        constexpr explicit Real(const double value) noexcept : _bits(doubleToBits(value)) {}

        [[nodiscard]] constexpr bool sign() const noexcept { return getSign(_bits); }
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
