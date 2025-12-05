//
// Created by LiDong on 2025/10/6.
//
#pragma once

#include <fmt/ostream.h>

#include "gc/GC.hpp"

namespace Ciallang {
    class String : public GCObject {
        static constexpr int G_ShortStrLen = 21;

    public:
        explicit String(const char *str, std::uint32_t len);

        const char *getData() const { return _longStr ? _longStr : _shortStr; }

        std::string toStdStr() const { return std::string(getData(), _len); }

        bool operator==(const String &str) { return toStdStr() == str.toStdStr(); }

        friend std::ostream &operator<<(std::ostream &os, const String &d) { return os << d.toStdStr(); }

    private:
        char *_longStr;
        char _shortStr[G_ShortStrLen + 1];
        int _len;
    };
} // namespace Ciallang

// support fmt::format
template <>
struct fmt::formatter<Ciallang::String> : ostream_formatter {};
