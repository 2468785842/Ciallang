//
// Created by LiDong on 2025/10/6.
//
#pragma once

#include <fmt/ostream.h>

#include "common/Hash.hpp"

namespace cial {
    class Value;
    class String final {
        static constexpr int SHORT_STR_LEN = 21;

    public:
        constexpr explicit String() : _len(0) {}
        constexpr explicit String(const char c) : _shortStr{ c, '\0' }, _len(1) {}

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
                this->~String();
                new(this) String(str);
            }
            return *this;
        }

        void append(const String &str);

        // 一次性拼接(单次分配,两段均拷贝并正确终止);热路径用这个而非 copy+append。
        // SSO 阈值必须与构造/析构/move 的长串判据(len > SHORT_STR_LEN)一致:
        // total == SHORT_STR_LEN 仍是短串(_shortStr 容量 22=21字符+NUL)。
        // 旧判据 total < SHORT_STR_LEN 把恰好 21 字符的结果送进长串分支,
        // _longStr 分配后 move/析构按短串处理——丢尾字符并泄漏缓冲
        [[nodiscard]] static String concat(const String &a, const String &b) {
            const u32 total = a._len + b._len;
            if(total <= SHORT_STR_LEN) {
                String r{};
                std::memcpy(r._shortStr, a.getData(), a._len);
                std::memcpy(r._shortStr + a._len, b.getData(), b._len);
                r._shortStr[total] = '\0';
                r._len = total;
                return r;
            }
            String r{ a };
            r.reserveExact(total);
            std::memcpy(r.getBuffer() + a._len, b.getData(), b._len);
            r.getBuffer()[total] = '\0';
            r._len = total;
            return r;
        }

        // 保证长字符串缓冲至少可容纳 cap 字节(不含终止符);短串超限则转长串
        void reserveExact(const u32 cap) {
            if(cap <= SHORT_STR_LEN)
                return; // 短串足够(与 concat 的 SSO 阈值一致)
            if(_longStr && cap < _lenCap)
                return; // 容量已够
            auto *buf = new char[cap + 1];
            std::memcpy(buf, getData(), _len);
            buf[_len] = 0;
            delete[] _longStr;
            _longStr = buf;
            _lenCap = cap;
        }

        [[nodiscard]] u32 length() const { return _len; }

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

        char operator[](const u32 index) const { return getData()[index]; }

        friend std::ostream &operator<<(std::ostream &os, const String &d) { return os.write(d.getData(), d.length()); }

        [[nodiscard]] bool isEmpty() const { return _len == 0; }

        //---------------------------------------------------------------------------
        [[nodiscard]] String escapeBackSlash() const;

    private:
        char *_longStr = nullptr;
        char _shortStr[SHORT_STR_LEN + 1]{};
        u32 _len;
        u32 _lenCap{}; // 长字符串已分配容量(不含终止符);短串恒 0

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
        const cial::u32 h = cial::fnv1a(v.getData(), v.length());
        const cial::u64 key = static_cast<cial::u64>(h) << 32 | static_cast<cial::u64>(v.length());
        return key;
    }
};
