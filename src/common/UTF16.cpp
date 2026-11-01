#include "UTF16.hpp"

#include <algorithm>
#include <stdexcept>

#include "UTF8.hpp"

namespace cial::Common {
    namespace {
        CodePointType decode(std::string_view text) {
            // utf8Decode 禁止代理码元；脚本值还需允许 WTF-8 的三字节代理编码。
            auto point = utf8Decode(text.data(), text.size());
            if(text.size() >= 3) {
                const auto a = static_cast<unsigned char>(text[0]);
                const auto b = static_cast<unsigned char>(text[1]);
                const auto c = static_cast<unsigned char>(text[2]);
                if(a == 0xed && b >= 0xa0 && b <= 0xbf && (c & 0xc0) == 0x80)
                    return { 3, static_cast<i32>(((a & 15) << 12) | ((b & 63) << 6) | (c & 63)) };
            }
            if(point.width <= 0)
                point.width = 1;
            return point;
        }

        void append(std::string &out, const char16_t unit) {
            if(unit >= 0xd800 && unit <= 0xdfff) {
                out.push_back(static_cast<char>(0xe0 | (unit >> 12)));
                out.push_back(static_cast<char>(0x80 | ((unit >> 6) & 63)));
                out.push_back(static_cast<char>(0x80 | (unit & 63)));
            } else {
                const auto encoded = utf8Encode(unit);
                out.append(reinterpret_cast<const char *>(encoded.data), encoded.width);
            }
        }
    } // namespace

    std::u16string toUtf16(std::string_view text) {
        std::u16string out;
        out.reserve(text.size());
        while(!text.empty()) {
            const auto point = decode(text);
            auto value = static_cast<unsigned>(point.value);
            if(value > 0xffff) {
                value -= 0x10000;
                out.push_back(static_cast<char16_t>(0xd800 + (value >> 10)));
                out.push_back(static_cast<char16_t>(0xdc00 + (value & 1023)));
            } else {
                out.push_back(static_cast<char16_t>(value));
            }
            text.remove_prefix(static_cast<size_t>(point.width));
        }
        return out;
    }

    std::string fromUtf16(const std::u16string_view text) {
        std::string out;
        out.reserve(text.size());
        for(size_t i = 0; i < text.size(); ++i) {
            const auto unit = text[i];
            if(unit >= 0xd800 && unit <= 0xdbff && i + 1 < text.size() && text[i + 1] >= 0xdc00 &&
               text[i + 1] <= 0xdfff) {
                const auto value = 0x10000 + ((unit - 0xd800) << 10) + (text[++i] - 0xdc00);
                const auto encoded = utf8Encode(value);
                out.append(reinterpret_cast<const char *>(encoded.data), encoded.width);
            } else {
                append(out, unit);
            }
        }
        return out;
    }

    std::string fromUtf16LE(const std::string_view bytes) {
        if(bytes.size() % 2)
            throw std::runtime_error("truncated UTF-16 data");
        std::u16string units;
        units.reserve(bytes.size() / 2);
        for(size_t i = 0; i < bytes.size(); i += 2)
            units.push_back(static_cast<char16_t>(static_cast<unsigned char>(bytes[i]) |
                                                  (static_cast<unsigned char>(bytes[i + 1]) << 8)));
        return fromUtf16(units);
    }

    std::string toUtf16LE(const std::string_view text) {
        std::string out;
        for(const auto unit : toUtf16(text)) {
            out.push_back(static_cast<char>(unit & 0xff));
            out.push_back(static_cast<char>(unit >> 8));
        }
        return out;
    }

    size_t utf16Length(std::string_view text) {
        size_t count = 0;
        while(!text.empty()) {
            const auto point = decode(text);
            count += point.value > 0xffff ? 2 : 1;
            text.remove_prefix(static_cast<size_t>(point.width));
        }
        return count;
    }

    std::string utf16Slice(const std::string_view text, const size_t start, const size_t count) {
        const auto units = toUtf16(text);
        if(start >= units.size())
            return {};
        return fromUtf16(std::u16string_view(units).substr(start, count));
    }
} // namespace cial::Common
