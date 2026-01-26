// Copyright (c) 2024/5/23 下午9:40
//
// /\  _` \   __          /\_ \  /\_ \
// \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
//  \ \ \/_/_\/\ \  /'__`\  \ \ \  \ \ \   /'__`\  /' _ `\  /'_ `\
//   \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
//    \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
//     \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
//                                                            /\____
//                                                            \_/__/
//

#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace cial {

    using u8 = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;

    using i8 = std::int8_t;
    using i16 = std::int16_t;
    using i32 = std::int32_t;
    using i64 = std::int64_t;

    enum class ValueType : u8 {
        Void, // empty
        Object,
        String,
        Octet, // octet binary data
        Integer,
        Real
    };

    using Integer = i64;
    using UInteger = u64;
    class Real;
    class String;
    class Octet;
    class Object;

    class Value;

    template <typename K, typename V>
    using Map = std::unordered_map<K, V>;

    template <typename T>
    using Vec = std::vector<T>;

    template <typename T>
    using Opt = std::optional<T>;

    template <typename T>
    using Box = std::unique_ptr<T>;
} // namespace cial
