//
// Created by LiDong on 2026/1/7.
//
#include <cstring>

#include "TypeConverter.hpp"

#include "Octet.hpp"
#include "Real.hpp"
#include "String.hpp"
#include "common/Defer.hpp"
#include "parser/Lexer.hpp"

namespace cial::TypeConverter {

    String octetToString(const Octet &oct) {
        if(oct.getSize() == 0)
            return ""_str;

        const u32 stringLen = oct.getSize() * 3 - 1;
        const auto buf = new char[stringLen];
        char *pBuf = buf;
        DEFER { delete[] buf; };

        static constexpr char hex[] = "0123456789ABCDEF";
        const u8 *data = oct.getData();
        Integer n = oct.getSize();
        while(n--) {
            pBuf[0] = hex[*data >> 4];
            pBuf[1] = hex[*data & 0x0f];
            if(n != 0)
                pBuf[2] = ' ';
            pBuf += 3;
            data++;
        }

        return String{ buf, stringLen };
    }

    String objectToString(const Object &obj) {
        return String{ fmt::format("(object {:#x})", reinterpret_cast<uintptr_t>(&obj)) };
    }

    String integerToString(const Integer integer) {
        char buf[32];
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), integer);
        if(ec != std::errc())
            return ""_str;
        return String{ buf, static_cast<u32>(ptr - buf) };
    }

    String realToString(const Real real) {
        if(real.isNan())
            return "NaN"_str;

        if(real.isInfinity())
            return real.sign() ? "-Infinity"_str : "Infinity"_str;

        if(real.value() == 0.0)
            return real.sign() ? "-0.0"_str : "+0.0"_str;

        return String{ fmt::format("{:.17g}", real.value()) };
    }

    String realToHexString(const Real &real) {
        if(real.isNan())
            return "NaN"_str;

        if(real.isInfinity())
            return real.sign() ? "-Infinity"_str : "Infinity"_str;

        if(real.value() == 0.0)
            return real.sign() ? "-0.0"_str : "+0.0"_str;

        // 取 raw bits（无 UB）
        const u64 bits = real.bits();

        static constexpr char hex[] = "0123456789ABCDEF";

        String result;
        {
            char buf[96];
            char *p = buf;

            if(real.sign()) {
                *p++ = '-';
            }

            *p++ = '0';
            *p++ = 'x';
            *p++ = '1';
            *p++ = '.';

            // significand
            const u64 frac = bits & (1ULL << SIGNIFICAND_BITS) - 1;
            for(int i = SIGNIFICAND_BITS - 4; i >= 0; i -= 4) {
                *p++ = hex[frac >> i & 0xF];
            }

            *p++ = 'p';

            // exponent
            {
                char expBuf[16];
                auto [ep, ec] = std::to_chars(expBuf, expBuf + 16, real.exponent());
                std::memcpy(p, expBuf, ep - expBuf);
                p += ep - expBuf;
            }

            result = String(buf, static_cast<u32>(p - buf));
        }

        return result;
    }

    // 字符串→数值的公共解析：词法器把 '-'/'+' 产出为 Minus/Plus token 而非数值，
    // 需在词法前剥掉符号（TJS2 语义:"-1"、"+1.5" 均可转数值）。
    // 返回: 1=正 -1=负 0=无符号且首字符非数字(调用方报错)
    static int stripSign(const String &str, String &out) {
        const char *s = str.getData();
        u32 len = str.length();
        u32 i = 0;
        while(i < len && static_cast<unsigned char>(s[i]) <= 0x20)
            ++i;
        int sign = 1;
        if(i < len && (s[i] == '-' || s[i] == '+')) {
            if(s[i] == '-')
                sign = -1;
            ++i;
        }
        if(i >= len || !std::isdigit(static_cast<unsigned char>(s[i])))
            return 0;
        out = String(s + i, len - i);
        return sign;
    }

    Ret<Integer> stringToInteger(const String &str) {
        auto retErr = Ret<Integer>::err(ErrCode::InvalidCast, String{ str.toStdStr() + " can't covert to integer" });

        String body{};
        const int sign = stripSign(str, body);
        if(sign == 0)
            return retErr;

        Common::SourceFile sf{};
        Common::Result r{};
        sf.load(r, body.getData());
        if(r.isFailed())
            return retErr;
        PreProcessor pp{};
        syntax::Lexer lexer{ sf, pp };

        syntax::Token *t;
        lexer.next(t, false);

        if(t->type() != syntax::TokenType::ConstVal)
            return retErr;
        // TJS2 语义:字符串到数值的转换不区分整数/小数写法("10" 与 "1.5" 均可),
        // 小数转整数截断
        Integer value = 0;
        if(t->valueType() == syntax::TokenValueType::Integer)
            value = t->getInteger();
        else if(t->valueType() == syntax::TokenValueType::Real)
            value = static_cast<Integer>(t->getReal().value());
        else
            return retErr;

        return Ret<Integer>::ok(sign * value);
    }

    Ret<Real> stringToReal(const String &str) {
        auto retErr = Ret<Real>::err(ErrCode::InvalidCast, String{ str.toStdStr() + " can't covert to real" });

        String body{};
        const int sign = stripSign(str, body);
        if(sign == 0)
            return retErr;

        Common::SourceFile sf{};
        Common::Result r{};
        sf.load(r, body.getData());
        if(r.isFailed())
            return retErr;
        PreProcessor pp{};
        syntax::Lexer lexer{ sf, pp };

        syntax::Token *t;
        lexer.next(t, false);

        if(t->type() != syntax::TokenType::ConstVal)
            return retErr;
        // 同 stringToInteger:整数写法("10")也接受,升为 Real
        double value = 0.0;
        if(t->valueType() == syntax::TokenValueType::Real)
            value = t->getReal().value();
        else if(t->valueType() == syntax::TokenValueType::Integer)
            value = static_cast<double>(t->getInteger());
        else
            return retErr;

        return Ret<Real>::ok(Real{ sign * value });
    }

} // namespace cial::TypeConverter