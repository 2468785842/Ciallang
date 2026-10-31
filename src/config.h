//
// Created by LiDong on 2026/1/29.
//
#pragma once

#if defined(DEBUG) || defined(_DEBUG) || !defined(NDEBUG)
#define CIAL_DEBUG 1
#else
#define CIAL_DEBUG 0
#endif

#if defined(_MSC_VER)
#define CIAL_INLINE __forceinline
#define CIAL_COLD_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define CIAL_INLINE inline __attribute__((always_inline))
#define CIAL_COLD_NOINLINE __attribute__((cold, noinline))
#else
#define CIAL_INLINE inline
#define CIAL_COLD_NOINLINE
#endif