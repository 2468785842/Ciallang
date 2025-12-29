//
// Created by LiDong on 2025/10/6.
//
#pragma once

#include <fmt/ostream.h>

#include "gc/GC.hpp"

namespace Cial {
    class String final : public GCObject {
        static constexpr int G_ShortStrLen = 21;

    public:
        explicit String(const char *str, std::uint32_t len);

        [[nodiscard]] std::uint32_t length() const { return _len; }

        [[nodiscard]] const char *getData() const { return _longStr ? _longStr : _shortStr; }

        [[nodiscard]] std::string toStdStr() const { return std::string{ getData(), static_cast<size_t>(_len) }; }

        bool operator==(const String &str) const { return toStdStr() == str.toStdStr(); }

        Opt<Vec<GCObject *>> getRefs() override { return {}; }

        friend std::ostream &operator<<(std::ostream &os, const String &d) { return os << d.toStdStr(); }

        [[nodiscard]] bool isEmpty() const { return _len == 0; }

        template <size_t N>
        static Value create(const char (&arr)[N]) {
            return Value{ new String(arr, N) };
        }

        static Value create(const char *str, std::uint32_t size);

    private:
        char *_longStr = nullptr;
        char _shortStr[G_ShortStrLen + 1]{};
        int _len;
    };

    inline String operator""_str(const char *str, const std::size_t len) { return String(str, len); }
} // namespace Cial

// support fmt::format
template <>
struct fmt::formatter<Cial::String> : ostream_formatter {};
