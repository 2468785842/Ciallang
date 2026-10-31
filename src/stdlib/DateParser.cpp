// Copyright (c) 2026/10/26
//

#define _CRT_SECURE_NO_WARNINGS

#include "DateParser.hpp"

#include <cctype>
#include <cstring>
#include <stdexcept>

#include <time.h>

namespace cial::stdlib {

    long tjsTimezoneSeconds() {
#ifdef _WIN32
        long seconds = 0;
        _get_timezone(&seconds);
        return seconds;
#else
        ::tzset();
        return ::timezone;
#endif
    }

    namespace {

        // 词法 token（tTJSDateParser::lex 对齐：数字 / 词表命中 / 单字符回退，
        // TK_END 对应 bison 的 -1）
        enum TokKind { TK_END = -1, TK_NUMBER, TK_MONTH, TK_WDAY, TK_TZ, TK_AM, TK_PM, TK_CHAR };

        struct Token {
            TokKind kind;
            int val{}; // NUMBER / MONTH(0-based) / WDAY / TZ(hhmm)
            int ch{}; // TK_CHAR 的字符码（已小写化）
        };

        // 词表（tjsDateWordMap.cpp 全量转写）。匹配语义 = 最长前缀命中且词后
        // 字符非字母（原 trie 单趟下降无回溯，等价于「最长命中者校验边界，
        // 任一失败整体回退单字符 token」）。
        struct DateWord {
            const char *word;
            TokKind kind;
            int val;
        };

        constexpr DateWord kWords[] = {
            // 月名（val = 0-based 月份）
            { "january", TK_MONTH, 0 },
            { "jan", TK_MONTH, 0 },
            { "jan.", TK_MONTH, 0 },
            { "february", TK_MONTH, 1 },
            { "feb", TK_MONTH, 1 },
            { "feb.", TK_MONTH, 1 },
            { "march", TK_MONTH, 2 },
            { "mar", TK_MONTH, 2 },
            { "mar.", TK_MONTH, 2 },
            { "april", TK_MONTH, 3 },
            { "apr", TK_MONTH, 3 },
            { "apr.", TK_MONTH, 3 },
            { "may", TK_MONTH, 4 },
            { "june", TK_MONTH, 5 },
            { "jun", TK_MONTH, 5 },
            { "jun.", TK_MONTH, 5 },
            { "ju", TK_MONTH, 5 }, // 原版词表怪癖："ju" 单独成词
            { "july", TK_MONTH, 6 },
            { "jul", TK_MONTH, 6 },
            { "jul.", TK_MONTH, 6 },
            { "august", TK_MONTH, 7 },
            { "aug", TK_MONTH, 7 },
            { "aug.", TK_MONTH, 7 },
            { "september", TK_MONTH, 8 },
            { "sept", TK_MONTH, 8 },
            { "sept.", TK_MONTH, 8 },
            { "sep", TK_MONTH, 8 },
            { "sep.", TK_MONTH, 8 },
            { "october", TK_MONTH, 9 },
            { "oct", TK_MONTH, 9 },
            { "oct.", TK_MONTH, 9 },
            { "november", TK_MONTH, 10 },
            { "nov", TK_MONTH, 10 },
            { "nov.", TK_MONTH, 10 },
            { "december", TK_MONTH, 11 },
            { "dec", TK_MONTH, 11 },
            { "dec.", TK_MONTH, 11 },
            // 星期（值不参与语义，仅被文法消费）
            { "sunday", TK_WDAY, 0 },
            { "sun", TK_WDAY, 0 },
            { "sun.", TK_WDAY, 0 },
            { "monday", TK_WDAY, 1 },
            { "mon", TK_WDAY, 1 },
            { "mon.", TK_WDAY, 1 },
            { "tuesday", TK_WDAY, 2 },
            { "tues", TK_WDAY, 2 },
            { "tues.", TK_WDAY, 2 },
            { "tue", TK_WDAY, 2 },
            { "tue.", TK_WDAY, 2 },
            { "wednesday", TK_WDAY, 3 },
            { "wed", TK_WDAY, 3 },
            { "wed.", TK_WDAY, 3 },
            { "thursday", TK_WDAY, 4 },
            { "thurs", TK_WDAY, 4 },
            { "thurs.", TK_WDAY, 4 },
            { "thur", TK_WDAY, 4 },
            { "thu", TK_WDAY, 4 },
            { "thu.", TK_WDAY, 4 },
            { "friday", TK_WDAY, 5 },
            { "fri", TK_WDAY, 5 },
            { "fri.", TK_WDAY, 5 },
            { "saturday", TK_WDAY, 6 },
            { "sat", TK_WDAY, 6 },
            { "sat.", TK_WDAY, 6 },
            // 上午/下午
            { "am", TK_AM, 0 },
            { "pm", TK_PM, 0 },
            // 时区缩写（val = [+/-]hhmm）
            { "acst", TK_TZ, 1030 },
            { "act", TK_TZ, 930 },
            { "adt", TK_TZ, -300 },
            { "aest", TK_TZ, 1100 },
            { "aet", TK_TZ, 1000 },
            { "ahst", TK_TZ, -1000 },
            { "ast", TK_TZ, -400 },
            { "awst", TK_TZ, 900 },
            { "awt", TK_TZ, 800 },
            { "a", TK_TZ, -100 },
            { "bst", TK_TZ, 100 },
            { "bt", TK_TZ, 300 },
            { "cadt", TK_TZ, 1030 },
            { "cast", TK_TZ, 930 },
            { "cat", TK_TZ, -1000 },
            { "cct", TK_TZ, 800 },
            { "cdt", TK_TZ, -500 },
            { "cedt", TK_TZ, 200 },
            { "cet", TK_TZ, 100 },
            { "cst", TK_TZ, -600 },
            { "dnt", TK_TZ, 100 },
            { "east", TK_TZ, 1000 },
            { "edt", TK_TZ, -400 },
            { "eetdst", TK_TZ, 300 },
            { "eet", TK_TZ, 200 },
            { "est", TK_TZ, -500 },
            { "fst", TK_TZ, 100 },
            { "fwt", TK_TZ, 200 },
            { "gmt", TK_TZ, 0 },
            { "gst", TK_TZ, 1000 },
            { "hdt", TK_TZ, -900 },
            { "idle", TK_TZ, 1200 },
            { "idlw", TK_TZ, -1200 },
            { "ist", TK_TZ, 200 },
            { "it", TK_TZ, 330 },
            { "jst", TK_TZ, 900 },
            { "jt", TK_TZ, 730 },
            { "kst", TK_TZ, 900 },
            { "ligt", TK_TZ, 1000 },
            { "mdt", TK_TZ, -600 },
            { "mest", TK_TZ, 200 },
            { "metdst", TK_TZ, 200 },
            { "met", TK_TZ, 100 },
            { "mewt", TK_TZ, 100 },
            { "mst", TK_TZ, -700 },
            { "mt", TK_TZ, 830 },
            { "m", TK_TZ, -1200 },
            { "ndt", TK_TZ, -230 },
            { "nft", TK_TZ, -330 },
            { "nor", TK_TZ, 100 },
            { "nst", TK_TZ, -330 },
            { "nt", TK_TZ, -1100 },
            { "nzdt", TK_TZ, 1300 },
            { "nzst", TK_TZ, 1200 },
            { "nzt", TK_TZ, 1200 },
            { "n", TK_TZ, 100 },
            { "pdt", TK_TZ, -700 },
            { "pst", TK_TZ, -800 },
            { "sadt", TK_TZ, 1030 },
            { "sast", TK_TZ, 930 },
            { "set", TK_TZ, 100 },
            { "sst", TK_TZ, 200 },
            { "utc", TK_TZ, 0 },
            { "ut", TK_TZ, 0 },
            { "wadt", TK_TZ, 800 },
            { "wast", TK_TZ, 700 },
            { "wat", TK_TZ, -100 },
            { "wdt", TK_TZ, 900 },
            { "wetdst", TK_TZ, 100 },
            { "wet", TK_TZ, 0 },
            { "wst", TK_TZ, 800 },
            { "ydt", TK_TZ, -800 },
            { "y", TK_TZ, 1200 },
            { "z", TK_TZ, 0 },
        };

        class DateParser {
        public:
            explicit DateParser(const std::string &s) :
                input(reinterpret_cast<const unsigned char *>(s.c_str())), cursor(input) {}

            time_t parse() {
                Token t = lex();
                // wday_omittable：星期 [',']
                if(t.kind == TK_WDAY) {
                    t = lex();
                    if(t.kind == TK_CHAR && t.ch == ',')
                        t = lex();
                }
                parseDateForm(t);
                parseTz();
                return build();
            }

        private:
            const unsigned char *input;
            const unsigned char *cursor;
            bool hasPending = false; // 单枚回退 token（对齐 bison 的单 token 前瞻）
            Token pending{ TK_END };
            bool yearSet = false, monthSet = false, mdaySet = false;
            bool hourSet = false, minSet = false, secSet = false;
            bool ampmSet = false, ampm = false; // ampm: pm 为 true
            bool timeZoneSet = false, timeZoneOffsetSet = false;
            int timeZone = 0, timeZoneOffset = 0; // hhmm，build 时换算为秒
            int year = 1970, month = 0, mday = 1, hour = 0, min = 0, sec = 0;

            [[noreturn]] static void fail() { throw std::runtime_error("cannot parse date"); }

            static bool isSpaceByte(const unsigned char c) { return c < 0x80 && std::isspace(c) != 0; }

            static bool isDigitByte(const unsigned char c) { return c >= '0' && c <= '9'; }

            // 词边界：ASCII 字母或任意 >=0x80 字节视为字母（原 iswalpha 对宽字符
            // 一律返回字母；UTF-8 多字节序列逐字节等价阻断词命中）
            static bool isAlphaByte(const unsigned char c) { return c >= 0x80 || std::isalpha(c) != 0; }

            static unsigned char lowerByte(const unsigned char c) {
                return c >= 'A' && c <= 'Z' ? static_cast<unsigned char>(c - 'A' + 'a') : c;
            }

            const DateWord *matchWord(const unsigned char *p) const {
                const DateWord *best = nullptr;
                size_t bestLen = 0;
                for(const DateWord &w : kWords) {
                    const size_t len = std::strlen(w.word);
                    if(len <= bestLen)
                        continue;
                    size_t i = 0;
                    while(i < len && lowerByte(p[i]) == static_cast<unsigned char>(w.word[i]))
                        i++;
                    if(i == len && !isAlphaByte(p[len])) {
                        best = &w;
                        bestLen = len;
                    }
                }
                return best;
            }

            Token lex() {
                if(hasPending) {
                    hasPending = false;
                    return pending;
                }
                while(*cursor != 0 && isSpaceByte(*cursor))
                    cursor++;
                if(*cursor == 0)
                    return { TK_END };
                if(isDigitByte(*cursor)) {
                    int val = 0;
                    while(isDigitByte(*cursor))
                        val = val * 10 + (*cursor++ - '0');
                    return { TK_NUMBER, val };
                }
                if(const DateWord *w = matchWord(cursor)) {
                    cursor += std::strlen(w->word);
                    return { w->kind, w->val };
                }
                int ch = *cursor++;
                if(ch >= 'A' && ch <= 'Z')
                    ch += 'a' - 'A';
                return { TK_CHAR, 0, ch };
            }

            void unlex(const Token t) {
                hasPending = true;
                pending = t;
            }

            int expectNumber() {
                const Token t = lex();
                if(t.kind != TK_NUMBER)
                    fail();
                return t.val;
            }

            void expectChar(const char c) {
                const Token t = lex();
                if(t.kind != TK_CHAR || t.ch != c)
                    fail();
            }

            // ---- 字段 setter（tTJSDateParser 语义）----

            void setYear(const int v) {
                int y = v;
                if(y < 100)
                    y = y <= 50 ? 2000 + y : 1900 + y;
                yearSet = true;
                year = y;
            }

            void setMonth(const int v) {
                monthSet = true;
                month = v;
            }

            void setMDay(const int v) {
                mdaySet = true;
                mday = v;
            }

            void setHours(const int v) {
                hourSet = true;
                hour = v;
            }

            void setMin(const int v) {
                minSet = true;
                min = v;
            }

            void setSec(const int v) {
                secSet = true;
                sec = v;
            }

            void setAMPM(const bool isPm) {
                ampmSet = true;
                ampm = isPm;
            }

            void setTimeZone(const int v) {
                timeZoneSet = true;
                timeZone = v;
            }

            void setTimeZoneOffset(const int v) {
                timeZoneOffsetSet = true;
                timeZoneOffset = v;
            }

            // ---- 文法 ----

            // time_hms 小时已知的剩余部分：':' 分 [':' 秒 ['.' 小数秒]]；
            // 结束前多读的一枚 token 归还（可能是尾随 am/pm / 年份 / 时区起点）
            void parseTimeFromHour(const int h) {
                expectChar(':');
                const int m = expectNumber();
                int s = 0;
                Token t = lex();
                if(t.kind == TK_CHAR && t.ch == ':') {
                    s = expectNumber();
                    t = lex();
                    if(t.kind == TK_CHAR && t.ch == '.') {
                        expectNumber(); // 小数秒：消费但忽略（原版同）
                        t = lex();
                    }
                }
                unlex(t);
                setHours(h);
                setMin(m);
                setSec(s);
            }

            // time := am_or_pm time_hms | time_hms am_or_pm
            void parseTime() {
                const Token t = lex();
                if(t.kind == TK_AM || t.kind == TK_PM) {
                    setAMPM(t.kind == TK_PM);
                    parseTimeFromHour(expectNumber());
                } else if(t.kind == TK_NUMBER) {
                    parseTimeFromHour(t.val);
                    consumeTrailingAmPm();
                } else {
                    fail();
                }
            }

            void consumeTrailingAmPm() {
                const Token t = lex();
                if(t.kind == TK_AM || t.kind == TK_PM)
                    setAMPM(t.kind == TK_PM);
                else
                    unlex(t);
            }

            // F7/F8/F9/F10 数值时刻分支：小时已消耗，时刻后可尾随 am/pm，随后年份
            void parseTimeThenYear(const int h) {
                parseTimeFromHour(h);
                consumeTrailingAmPm();
                setYear(expectNumber());
            }

            // wday_omittable 之后的日期主体（tjsdate.y 的 11 个产生式；注释为
            // 各产生式的动作）
            void parseDateForm(const Token first) {
                Token t = first;
                if(t.kind == TK_NUMBER) {
                    const int d1 = t.val;
                    t = lex();
                    if(t.kind == TK_MONTH) {
                        setMonth(t.val);
                        t = lex();
                        if(t.kind == TK_AM || t.kind == TK_PM) {
                            // Sun, 3 May am 11:22:33 2004：day=N1，time，year
                            setMDay(d1);
                            setAMPM(t.kind == TK_PM);
                            parseTimeFromHour(expectNumber());
                            setYear(expectNumber());
                        } else if(t.kind == TK_NUMBER) {
                            const int d2 = t.val;
                            t = lex();
                            if(t.kind == TK_CHAR && t.ch == ':') {
                                // Sun, 3 May 11:22:33 2004：day=N1，hour=N2
                                setMDay(d1);
                                unlex(t);
                                parseTimeThenYear(d2);
                            } else {
                                // Sun, 3 May 2004 11:22:33：day=N1，year=N2
                                setMDay(d1);
                                setYear(d2);
                                unlex(t);
                                parseTime();
                            }
                        } else {
                            fail();
                        }
                    } else if(t.kind == TK_CHAR && (t.ch == '-' || t.ch == '/')) {
                        const bool hyphen = t.ch == '-';
                        t = lex();
                        if(hyphen && t.kind == TK_MONTH) {
                            setMonth(t.val);
                            t = lex();
                            if(t.kind == TK_AM || t.kind == TK_PM) {
                                // Sun, 3-May am 11:22:33 2004
                                setMDay(d1);
                                setAMPM(t.kind == TK_PM);
                                parseTimeFromHour(expectNumber());
                                setYear(expectNumber());
                            } else if(t.kind == TK_NUMBER) {
                                const int d2 = t.val;
                                t = lex();
                                if(t.kind == TK_CHAR && t.ch == ':') {
                                    // Sun, 3-May 11:22:33 2004
                                    setMDay(d1);
                                    unlex(t);
                                    parseTimeThenYear(d2);
                                } else {
                                    // Sun, 3-May 2004 11:22:33
                                    setMDay(d1);
                                    setYear(d2);
                                    unlex(t);
                                    parseTime();
                                }
                            } else if(t.kind == TK_CHAR && t.ch == '-') {
                                // Sun, 3-May-2004 11:22:33
                                setMDay(d1);
                                setYear(expectNumber());
                                parseTime();
                            } else {
                                fail();
                            }
                        } else if(t.kind == TK_NUMBER) {
                            // 2004/03/03（每个分隔符独立取 '-' 或 '/'）：
                            // year=N1，month=N2-1，day=N3
                            const int d2 = t.val;
                            t = lex();
                            if(!(t.kind == TK_CHAR && (t.ch == '-' || t.ch == '/')))
                                fail();
                            setYear(d1);
                            setMonth(d2 - 1);
                            setMDay(expectNumber());
                            parseTime();
                        } else {
                            fail();
                        }
                    } else {
                        fail();
                    }
                } else if(t.kind == TK_MONTH) {
                    setMonth(t.val);
                    t = lex();
                    if(t.kind == TK_NUMBER) {
                        const int d1 = t.val;
                        t = lex();
                        if(t.kind == TK_AM || t.kind == TK_PM) {
                            // Sun, May 3 am 11:22:33 2004
                            setMDay(d1);
                            setAMPM(t.kind == TK_PM);
                            parseTimeFromHour(expectNumber());
                            setYear(expectNumber());
                        } else if(t.kind == TK_NUMBER) {
                            const int d2 = t.val;
                            t = lex();
                            if(t.kind == TK_CHAR && t.ch == ':') {
                                // Sun, May 3 11:22:33 2004
                                setMDay(d1);
                                unlex(t);
                                parseTimeThenYear(d2);
                            } else {
                                // Sun, May 3 2004 11:22:33
                                setMDay(d1);
                                setYear(d2);
                                unlex(t);
                                parseTime();
                            }
                        } else {
                            fail();
                        }
                    } else if(t.kind == TK_CHAR && t.ch == '-') {
                        const int d1 = expectNumber();
                        t = lex();
                        if(t.kind == TK_AM || t.kind == TK_PM) {
                            // Sun, May-3 am 11:22:33 2004
                            setMDay(d1);
                            setAMPM(t.kind == TK_PM);
                            parseTimeFromHour(expectNumber());
                            setYear(expectNumber());
                        } else if(t.kind == TK_NUMBER) {
                            const int d2 = t.val;
                            t = lex();
                            if(t.kind == TK_CHAR && t.ch == ':') {
                                // Sun, May-3 11:22:33 2004
                                setMDay(d1);
                                unlex(t);
                                parseTimeThenYear(d2);
                            } else {
                                // Sun, May-3 2004 11:22:33
                                setMDay(d1);
                                setYear(d2);
                                unlex(t);
                                parseTime();
                            }
                        } else if(t.kind == TK_CHAR && t.ch == '-') {
                            // Sun, May-3-2004 11:22:33
                            setMDay(d1);
                            setYear(expectNumber());
                            parseTime();
                        } else {
                            fail();
                        }
                    } else {
                        fail();
                    }
                } else {
                    fail();
                }
            }

            // tz_omittable := [时区名] [[+|-]hhmm] ['(' 任意 ')']
            void parseTz() {
                Token t = lex();
                if(t.kind == TK_TZ) {
                    setTimeZone(t.val);
                    t = lex();
                }
                if(t.kind == TK_CHAR && t.ch == '+') {
                    setTimeZoneOffset(expectNumber());
                    t = lex();
                } else if(t.kind == TK_CHAR && t.ch == '-') {
                    setTimeZoneOffset(-expectNumber());
                    t = lex();
                }
                if(t.kind == TK_CHAR && t.ch == '(') {
                    // 括号段整体忽略：跳到 ')'（SkipToRightParenthesis）
                    while(*cursor != 0 && *cursor != ')')
                        cursor++;
                    expectChar(')');
                    t = lex();
                }
                if(t.kind != TK_END)
                    fail();
            }

            // 语义汇总（tTJSDateParser 构造器尾部对齐）
            time_t build() {
                // 目前除秒可省略外不允许任何缺省（原版同）
                if(!yearSet || !monthSet || !mdaySet || !hourSet || !minSet)
                    fail();
                if(!secSet)
                    sec = 0;

                // 时区 [+/-]hhmm → 秒
                const auto hhmmToSeconds = [](int v) {
                    const bool neg = v < 0;
                    if(neg)
                        v = -v;
                    v = (v / 100) * 3600 + (v % 100) * 60;
                    return neg ? -v : v;
                };
                if(timeZoneSet)
                    timeZone = hhmmToSeconds(timeZone);
                if(timeZoneOffsetSet)
                    timeZoneOffset = hhmmToSeconds(timeZoneOffset);

                // 时区省略时取系统时区（TimeZone = -TJS_timezone；与下方
                // tmv -= TJS_timezone 恰好相消，净效果 = 本地钟面秒）。
                // 已知怪癖（按参考实现逐位同步保留）：Windows CRT 的 _timezone
                // 为惰性初始化（进程内首次 mktime/localtime 触发 tzset 前是默认
                // 值 +28800）。原版 tjsDateParser 在 mktime 之前读 TJS_timezone
                // 算此默认值、之后再用（已更新为真实时区的）TJS_timezone 调整
                // ——进程内首个字符串构造会读到新旧两个值，结果偏移 +2×|tz|，
                // 之后恢复本行注释所述的净零语义。实测两引擎逐位一致
                // （含 Date.getTimezoneOffset 首调返回 stale 值 480 的现象）。
                if(!timeZoneSet && !timeZoneOffsetSet) {
                    timeZoneSet = true;
                    timeZone = -static_cast<int>(tjsTimezoneSeconds());
                }

                // PM 加 12（原版朴素语义：12:00 pm → 24:00 进位到次日 0 点）
                if(ampmSet && ampm)
                    hour += 12;

                tm stm{};
                stm.tm_year = year - 1900;
                stm.tm_mon = month;
                stm.tm_mday = mday;
                stm.tm_hour = hour;
                stm.tm_min = min;
                stm.tm_sec = sec;
                const time_t mk = mktime(&stm);
                if(mk == static_cast<time_t>(-1))
                    throw std::runtime_error("invalid value for timestamp");

                // 本地 → UTC，再减去显式时区偏移（西正东负）
                time_t tmv = mk - tjsTimezoneSeconds();
                int tz = 0;
                if(timeZoneSet)
                    tz += timeZone;
                if(timeZoneOffsetSet)
                    tz += timeZoneOffset;
                tmv -= tz;
                return tmv;
            }
        };

    } // namespace

    time_t parseDateString(const std::string &s) {
        DateParser parser{ s };
        return parser.parse();
    }

} // namespace cial::stdlib
