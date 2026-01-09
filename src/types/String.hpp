//
// Created by LiDong on 2025/10/6.
//
#pragma once

#include <fmt/ostream.h>

#include "common/Hash.hpp"
#include "gc/GC.hpp"

namespace cial {
    class Value;
    class String final : public RefCountHeader {
        static constexpr int SHORT_STR_LEN = 21;

    public:
        explicit String() : _len(0) {}
        explicit String(const char c) : _shortStr{ c, '\0' }, _len(1) {}
        explicit String(const char *str) : String(str, std::strlen(str)) {}

        template <size_t N>
        explicit String(const char (&arr)[N]) : String(arr, N - 1) {}

        explicit String(const char *str, std::uint32_t len);
        explicit String(const std::string &s) : String(s.data(), static_cast<std::uint32_t>(s.size())) {}

        ~String() noexcept override;

        String(String &&str) noexcept;

        String &operator=(String &&str) noexcept;

        String(const String &str) : String(str.getData(), str.length()) {}

        String &operator=(const String &str) = delete;

        void append(const String &str);

        [[nodiscard]] std::uint32_t length() const { return _len; }

        [[nodiscard]] char *getBuffer() { return _longStr ? _longStr : _shortStr; }
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

        //---------------------------------------------------------------------------
        [[nodiscard]] String escapeBackSlash() const;

    private:
        char *_longStr = nullptr;
        char _shortStr[SHORT_STR_LEN + 1]{};
        std::uint32_t _len;

    public:
        using iterator = char *;
        using const_iterator = const char *;

        iterator begin() noexcept { return getBuffer(); }
        iterator end() noexcept { return getBuffer() + _len; }

        [[nodiscard]] const_iterator begin() const noexcept { return getData(); }
        [[nodiscard]] const_iterator end() const noexcept { return getData() + _len; }

        [[nodiscard]] const_iterator cbegin() const noexcept { return begin(); }
        [[nodiscard]] const_iterator cend() const noexcept { return end(); }
    };

    inline String operator""_str(const char *str, const std::size_t len) { return String(str, len); }

    String formatString(const String &fmt, size_t paramCount, Value *params);

} // namespace cial

// support fmt::format
template <>
struct fmt::formatter<cial::String> : ostream_formatter {};

template <>
struct std::hash<cial::String> {
    size_t operator()(const cial::String &v) const noexcept {
        const uint32_t h = cial::fnv1a(v.getData(), v.length());
        const uint64_t key = static_cast<uint64_t>(h) << 32 | static_cast<uint64_t>(v.length());
        return key;
    }
};
