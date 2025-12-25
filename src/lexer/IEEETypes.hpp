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
#include <concepts>
#include <cstdint>
#include <limits>
#include <type_traits>

/*---------------------------------------------------------------------------*/
/* "TJS2" type definitions                                                   */
/*---------------------------------------------------------------------------*/

/* IEEE double manipulation support
 * (TJS requires IEEE double(64-bit float) native support on machine or C++ compiler)
 */

// 63 62       52 51                         0
// +-+-----------+---------------------------+
// |s|    exp    |         significand       |
// +-+-----------+---------------------------+
// s = sign,  negative if this is 1, otherwise positive.

namespace Cial::Syntax::IEEE {

    // Concepts for IEEE floating point operations
    template <typename T>
    concept IEEE754Double = std::same_as<T, double> && (sizeof(T) == 8) && (std::numeric_limits<T>::is_iec559);

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
    [[nodiscard]] constexpr std::enable_if_t<std::is_unsigned_v<T>, T> bit_extract(T value, int start,
                                                                                   int width) noexcept {
        return (value >> start) & ((T{ 1 } << width) - 1);
    }

    template <typename T>
    [[nodiscard]] constexpr std::enable_if_t<std::is_unsigned_v<T>, T> bit_insert(T value, int start, int width,
                                                                                  T insert) noexcept {
        T mask = ((T{ 1 } << width) - 1) << start;
        return (value & ~mask) | ((insert << start) & mask);
    }

    // Component extraction functions (replacing macros)
    [[nodiscard]] constexpr bool get_sign(uint64_t bits) noexcept { return (bits & SIGN_MASK) != 0; }

    [[nodiscard]] constexpr int32_t get_exponent(uint64_t bits) noexcept {
        return static_cast<int32_t>(bit_extract(bits, SIGNIFICAND_BITS, 11) - EXP_BIAS);
    }

    [[nodiscard]] constexpr uint64_t get_significand(uint64_t bits) noexcept { return bits & SIGNIFICAND_MASK; }

    // Component composition functions (replacing macros)
    [[nodiscard]] constexpr uint64_t make_sign(bool negative) noexcept { return negative ? SIGN_MASK : 0ull; }

    [[nodiscard]] constexpr uint64_t make_exponent(int32_t exp) noexcept {
        return static_cast<uint64_t>(exp + EXP_BIAS) << SIGNIFICAND_BITS;
    }

    [[nodiscard]] constexpr uint64_t make_significand(uint64_t significand) noexcept {
        return significand & SIGNIFICAND_MASK;
    }

    // Special values
    static constexpr uint64_t P_NaN = EXP_MASK | SIGNIFICAND_MSB_MASK;
    static constexpr uint64_t N_NaN = SIGN_MASK | P_NaN;
    static constexpr uint64_t P_INF = EXP_MASK;
    static constexpr uint64_t N_INF = SIGN_MASK | P_INF;

    // Special value check functions (replacing macros)
    [[nodiscard]] constexpr bool check_nan(uint64_t bits) noexcept {
        const bool exp_all_set = (bits & EXP_MASK) == EXP_MASK;
        const bool significand_nonzero = (bits & SIGNIFICAND_MASK) != 0;
        return exp_all_set && significand_nonzero;
    }

    [[nodiscard]] constexpr bool check_inf(uint64_t bits) noexcept {
        const bool exp_all_set = (bits & EXP_MASK) == EXP_MASK;
        const bool significand_zero = (bits & SIGNIFICAND_MASK) == 0;
        return exp_all_set && significand_zero;
    }

    // Modern C++20 bit cast support for double <-> uint64_t conversion
    template <IEEE754Double T = double>
    [[nodiscard]] constexpr uint64_t double_to_bits(T value) noexcept {
        return std::bit_cast<uint64_t>(value);
    }

    template <IEEE754Double T = double>
    [[nodiscard]] constexpr T bits_to_double(uint64_t bits) noexcept {
        return std::bit_cast<T>(bits);
    }

    // High-level IEEE 754 double precision utilities
    class Double {
        uint64_t _bits;

    public:
        constexpr explicit Double(uint64_t bits) noexcept : _bits(bits) {}
        constexpr explicit Double(double value) noexcept : _bits(double_to_bits(value)) {}

        [[nodiscard]] constexpr bool sign() const noexcept { return get_sign(_bits); }
        [[nodiscard]] constexpr int32_t exponent() const noexcept { return get_exponent(_bits); }
        [[nodiscard]] constexpr uint64_t significand() const noexcept { return get_significand(_bits); }

        [[nodiscard]] constexpr bool is_nan() const noexcept { return check_nan(_bits); }
        [[nodiscard]] constexpr bool is_infinity() const noexcept { return check_inf(_bits); }
        [[nodiscard]] constexpr bool is_finite() const noexcept { return !is_nan() && !is_infinity(); }

        [[nodiscard]] constexpr double value() const noexcept { return bits_to_double(_bits); }
        [[nodiscard]] constexpr uint64_t bits() const noexcept { return _bits; }

        // Static factory methods for special values
        [[nodiscard]] static constexpr Double positive_infinity() noexcept { return Double(P_INF); }
        [[nodiscard]] static constexpr Double negative_infinity() noexcept { return Double(N_INF); }
        [[nodiscard]] static constexpr Double quiet_nan() noexcept { return Double(P_NaN); }
        [[nodiscard]] static constexpr Double signaling_nan() noexcept { return Double(N_NaN); }
    };

} // namespace Cial::Syntax::IEEE
