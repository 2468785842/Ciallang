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

#include <utility>

#define SCOPE_GUARD_CONCATENATE_IMPL(s1, s2) s1##s2
#define SCOPE_GUARD_CONCATENATE(s1, s2) SCOPE_GUARD_CONCATENATE_IMPL(s1, s2)
// ScopeGuard for C++11
namespace clover {
    template<typename Fun>
    class ScopeGuard {
    public:
        explicit ScopeGuard(Fun &&f) : _fun(std::forward<Fun>(f)), _active(true) {
        }

        ~ScopeGuard() { if (_active) _fun(); }

        void dismiss() { _active = false; }

        ScopeGuard() = delete;

        ScopeGuard(const ScopeGuard &) = delete;

        ScopeGuard &operator=(const ScopeGuard &) = delete;

        ScopeGuard(ScopeGuard &&rhs) noexcept: _fun(std::move(rhs._fun)), _active(rhs._active) {
            rhs.dismiss();
        }

    private:
        Fun _fun;
        bool _active;
    };

    namespace detail {
        enum class ScopeGuardOnExit {
        };

        template<typename Fun>
        ScopeGuard<Fun> operator+(ScopeGuardOnExit, Fun &&fn) {
            return ScopeGuard<Fun>(std::forward<Fun>(fn));
        }
    } // namespace detail
} // namespace clover

// Helper macro
#define DEFER \
auto SCOPE_GUARD_CONCATENATE(ext_exitBlock_, __LINE__) = \
        clover::detail::ScopeGuardOnExit() + [&]()
