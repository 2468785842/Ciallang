#pragma once

#include <functional>
#include <type_traits>
#include "TypeId.hpp"

namespace cial {
    // 内部对象种类与宿主类型身份分离；显式 token 必须覆盖所有使用者的生命周期。
    class NativeTypeKey {
    public:
        constexpr NativeTypeKey(TypeId builtin = TypeId::None) noexcept : _builtin(builtin) {}
        static constexpr NativeTypeKey fromToken(const void *token) noexcept {
            NativeTypeKey key;
            key._token = token;
            return key;
        }
        bool operator==(const NativeTypeKey &) const = default;
        [[nodiscard]] size_t hash() const noexcept {
            return std::hash<const void *>{}(_token) ^ static_cast<size_t>(_builtin);
        }

    private:
        TypeId _builtin;
        const void *_token{};
    };

    // 默认键仅用于同一链接映像；跨 DLL 应共享由宿主提供的显式 token。
    template <typename T>
    NativeTypeKey nativeTypeKeyFor() noexcept {
        static constexpr char token{};
        return NativeTypeKey::fromToken(&token);
    }
} // namespace cial

template <>
struct std::hash<cial::NativeTypeKey> {
    size_t operator()(cial::NativeTypeKey key) const noexcept { return key.hash(); }
};
