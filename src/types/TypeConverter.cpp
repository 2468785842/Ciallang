//
// Created by LiDong on 2026/1/7.
//
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

        const std::uint32_t stringLen = oct.getSize() * 3 - 1;
        const auto buf = new char[stringLen];
        char *pBuf = buf;
        DEFER { delete[] buf; };

        static constexpr char hex[] = "0123456789ABCDEF";
        const std::uint8_t *data = oct.getData();
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
        return String{ buf, static_cast<std::uint32_t>(ptr - buf) };
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
        const std::uint64_t bits = real.bits();

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
            const std::uint64_t frac = bits & (1ULL << SIGNIFICAND_BITS) - 1;
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

            result = String(buf, static_cast<std::uint32_t>(p - buf));
        }

        return result;
    }

    Ret<Integer> stringToInteger(const String &str) {
        auto retErr = Ret<Integer>::err(ErrCode::InvalidCast, String{ str.toStdStr() + " can't covert to integer" });
        Common::SourceFile sf{};
        Common::Result r{};
        sf.load(r, str.getData());
        if(r.isFailed())
            return retErr;
        PreProcessor pp{};
        Syntax::Lexer lexer{ sf, pp };

        Syntax::Token *t;
        lexer.next(t, false);

        if(t->type() != Syntax::TokenType::ConstVal || t->valueType() != Syntax::TokenValueType::Integer)
            return retErr;

        return Ret<Integer>::ok(t->getInteger());
    }

    Ret<Real> stringToReal(const String &str) {
        auto retErr = Ret<Real>::err(ErrCode::InvalidCast, String{ str.toStdStr() + " can't covert to real" });
        Common::SourceFile sf{};
        Common::Result r{};
        sf.load(r, str.getData());
        if(r.isFailed())
            return retErr;
        PreProcessor pp{};
        Syntax::Lexer lexer{ sf, pp };

        Syntax::Token *t;
        lexer.next(t, false);

        if(t->type() != Syntax::TokenType::ConstVal || t->valueType() != Syntax::TokenValueType::Real)
            return retErr;

        return Ret<Real>::ok(t->getReal());
    }

} // namespace cial::TypeConverter