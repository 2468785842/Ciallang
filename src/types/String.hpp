//
// Created by LiDong on 2025/10/6.
//
#pragma once

#include <fmt/ostream.h>

#include "gc/GC.hpp"

namespace Cial {
    class String final : public RefCountHeader {
        static constexpr int SHORT_STR_LEN = 21;

    public:
        explicit String(const char *str) : String(str, std::strlen(str)) {}

        template <size_t N>
        explicit String(const char (&arr)[N]) : String(arr, N) {}

        explicit String(const char *str, std::uint32_t len);

        ~String() noexcept override;

        String(String &&str) noexcept;

        String &operator=(String &&str) noexcept;

        String(const String &str) = delete;

        String &operator=(const String &str) = delete;

        [[nodiscard]] std::uint32_t length() const { return _len; }

        [[nodiscard]] const char *getData() const { return _longStr ? _longStr : _shortStr; }

        [[nodiscard]] std::string toStdStr() const { return std::string{ getData(), static_cast<size_t>(_len) }; }

        bool operator==(const char *cStr) const {
            if(std::strlen(cStr) != _len)
                return false;
            return std::memcmp(getData(), cStr, _len) == 0;
        }

        bool operator==(const String &str) const {
            if(_len != str._len)
                return false;
            return std::memcmp(getData(), str.getData(), _len) == 0;
        }

        friend std::ostream &operator<<(std::ostream &os, const String &d) { return os.write(d.getData(), d.length()); }

        [[nodiscard]] bool isEmpty() const { return _len == 0; }

    private:
        char *_longStr = nullptr;
        char _shortStr[SHORT_STR_LEN + 1]{};
        std::uint32_t _len;
    };

    inline String operator""_str(const char *str, const std::size_t len) { return String(str, len); }
} // namespace Cial

// support fmt::format
template <>
struct fmt::formatter<Cial::String> : ostream_formatter {};
