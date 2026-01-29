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
#elif defined(__GNUC__) || defined(__clang__)
#define CIAL_INLINE inline __attribute__((always_inline))
#else
#define CIAL_INLINE inline
#endif