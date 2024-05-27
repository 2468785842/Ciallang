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
/*---------------------------------------------------------------------------*/
/* "TJS2" type definitions                                                   */
/*---------------------------------------------------------------------------*/
#include <cstdint>

/* IEEE double manipulation support
 * (TJS requires IEEE double(64-bit float) native support on machine or C++ compiler)
 */

// 63 62       52 51                         0
// +-+-----------+---------------------------+
// |s|    exp    |         significand       |
// +-+-----------+---------------------------+
// s = sign,  negative if this is 1, otherwise positive.

/* double related constants */
static constexpr int64_t IEEE_D_EXP_MAX = 1023;
static constexpr int64_t IEEE_D_EXP_MIN = -1022;
static constexpr int64_t IEEE_D_SIGNIFICAND_BITS = 52;

static constexpr uint64_t IEEE_D_EXP_BIAS = 1023;

/* component extraction */
static constexpr uint64_t IEEE_D_SIGN_MASK = 0x8000000000000000ull;
static constexpr uint64_t IEEE_D_EXP_MASK = 0x7ff0000000000000ull;
static constexpr uint64_t IEEE_D_SIGNIFICAND_MASK = 0x000fffffffffffffull;
static constexpr uint64_t IEEE_D_SIGNIFICAND_MSB_MASK = 0x0008000000000000ull;

#define IEEE_D_GET_SIGN(x) (0 != (x & IEEE_D_SIGN_MASK))

#define IEEE_D_GET_EXP(x) ((std::int32_t)(( \
        (x & IEEE_D_EXP_MASK) >> IEEE_D_SIGNIFICAND_BITS \
        ) - IEEE_D_EXP_BIAS))

#define IEEE_D_GET_SIGNIFICAND(x) (x & IEEE_D_SIGNIFICAND_MASK)

/* component composition */
#define IEEE_D_MAKE_SIGN(x)  ((x) ? 0x8000000000000000ull : 0ull)
#define IEEE_D_MAKE_EXP(x)   ((std::uint64_t)(x + IEEE_D_EXP_BIAS) << 52)
#define IEEE_D_MAKE_SIGNIFICAND(x) ((std::uint64_t)(x))

/* special expression */
/* (quiet) NaN */
static constexpr uint64_t IEEE_D_P_NaN = IEEE_D_EXP_MASK | IEEE_D_SIGNIFICAND_MSB_MASK;
static constexpr uint64_t IEEE_D_N_NaN = IEEE_D_SIGN_MASK | IEEE_D_P_NaN;
/* infinite */

static constexpr uint64_t IEEE_D_P_INF = IEEE_D_EXP_MASK;
static constexpr uint64_t IEEE_D_N_INF = IEEE_D_SIGN_MASK | IEEE_D_P_INF;

/* special expression check */
#define IEEE_D_IS_NaN(x) ((IEEE_D_EXP_MASK & (x)) == IEEE_D_EXP_MASK) && \
			(((x) & IEEE_D_SIGNIFICAND_MSB_MASK) || \
			(!((x) & IEEE_D_SIGNIFICAND_MSB_MASK) && \
			((x) & (IEEE_D_SIGNIFICAND_MASK ^ IEEE_D_SIGNIFICAND_MSB_MASK))))

#define IEEE_D_IS_INF(x) (((IEEE_D_EXP_MASK & (x)) == IEEE_D_EXP_MASK) && \
			(!((x) & IEEE_D_SIGNIFICAND_MASK)))
