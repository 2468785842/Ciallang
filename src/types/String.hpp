//
// Created by LiDong on 2025/10/6.
//
#pragma once

#include <cstddef>
#include <cstring>
#include <fmt/ostream.h>

#include "common/Hash.hpp"

namespace cial {
    class Value;
    class String final {
    public:
        static constexpr u32 SHORT_STR_LEN = 14;

        constexpr explicit String() : _shortStr{} {}
        constexpr explicit String(const char c) : _shortStr{ c, '\0' } { _shortStr[15] = 1; }

        template <size_t N>
        constexpr explicit String(const char (&arr)[N]) : String(arr, N - 1) {}

        explicit String(const char *str) : String(str, std::strlen(str)) {}
        explicit String(const char *str, u32 len);
        explicit String(const std::string &s) : String(s.data(), static_cast<u32>(s.size())) {}

        ~String() noexcept;

        String(String &&str) noexcept;

        String &operator=(String &&str) noexcept;

        String(const String &str) : String(str.getData(), str.length()) {}

        String &operator=(const String &str) {
            if(this != &str) {
                const auto externalTag = flags() & 0xe0;
                this->~String();
                new(this) String(str);
                flags() |= externalTag;
            }
            return *this;
        }

        void append(const String &str);

        // 16B 表示：短串占前 15B（含 NUL），末字节低 4 位存长度、
        // bit4 标识长串。高 3 位保留给内联 Value 的类型标签。
        [[nodiscard]] static String concat(const String &a, const String &b) {
            String r{};
            const u32 total = a.length() + b.length();
            r.reserveExact(total);
            std::memcpy(r.getBuffer(), a.getData(), a.length());
            std::memcpy(r.getBuffer() + a.length(), b.getData(), b.length());
            r.getBuffer()[total] = '\0';
            if(r.isLong())
                r._long.len = total;
            else
                r.flags() = static_cast<u8>(total);
            return r;
        }

        void reserveExact(const u32 cap) {
            if(cap <= SHORT_STR_LEN || cap <= length())
                return;
            const u32 len = length();
            const auto externalTag = flags() & 0xe0;
            auto *buf = new char[cap + 1];
            std::memcpy(buf, getData(), len);
            buf[len] = 0;
            if(isLong())
                delete[] _long.data;
            _long.data = buf;
            _long.len = len;
            flags() = externalTag | 0x10;
        }

        [[nodiscard]] u32 length() const { return isLong() ? _long.len : flags() & 0x0f; }
        [[nodiscard]] char *getBuffer() { return isLong() ? _long.data : _shortStr; }
        [[nodiscard]] const char *getData() const { return isLong() ? _long.data : _shortStr; }

        [[nodiscard]] std::string toStdStr() const { return std::string{ getData(), static_cast<size_t>(length()) }; }

        bool operator==(const char *cStr) const {
            if(std::strlen(cStr) != length())
                return false;
            return std::memcmp(getData(), cStr, length()) == 0;
        }

        bool operator==(const String &str) const {
            if(length() != str.length())
                return false;
            return std::memcmp(getData(), str.getData(), length()) == 0;
        }

        char operator[](const u32 index) const { return getData()[index]; }

        friend std::ostream &operator<<(std::ostream &os, const String &d) { return os.write(d.getData(), d.length()); }

        [[nodiscard]] bool isEmpty() const { return length() == 0; }

        //---------------------------------------------------------------------------
        [[nodiscard]] String escapeBackSlash() const;

    private:
        struct LongData {
            char *data;
            u32 len;
            u8 reserved[15 - sizeof(char *) - sizeof(u32)];
            u8 control;
        };
        static_assert(sizeof(LongData) == 16 && offsetof(LongData, control) == 15);
        union {
            char _shortStr[16];
            LongData _long;
        };
        [[nodiscard]] u8 &flags() noexcept { return reinterpret_cast<u8 *>(this)[15]; }
        [[nodiscard]] u8 flags() const noexcept { return reinterpret_cast<const u8 *>(this)[15]; }
        [[nodiscard]] bool isLong() const noexcept { return (flags() & 0x10) != 0; }

    public:
        using iterator = char *;
        using const_iterator = const char *;

        iterator begin() noexcept { return getBuffer(); }
        iterator end() noexcept { return getBuffer() + length(); }

        [[nodiscard]] const_iterator begin() const noexcept { return getData(); }
        [[nodiscard]] const_iterator end() const noexcept { return getData() + length(); }

        [[nodiscard]] const_iterator cbegin() const noexcept { return begin(); }
        [[nodiscard]] const_iterator cend() const noexcept { return end(); }
    };

    static_assert(sizeof(String) == 16, "String tag must remain at byte 15");

    inline String operator""_str(const char *str, const std::size_t len) { return String(str, len); }

    String formatString(const String &fmt, size_t paramCount, Value *params);

} // namespace cial

// support fmt::format
template <>
struct fmt::formatter<cial::String> : ostream_formatter {};

template <>
struct std::hash<cial::String> {
    size_t operator()(const cial::String &v) const noexcept {
        const cial::u32 h = cial::fnv1a(v.getData(), v.length());
        const cial::u64 key = static_cast<cial::u64>(h) << 32 | static_cast<cial::u64>(v.length());
        return key;
    }
};
