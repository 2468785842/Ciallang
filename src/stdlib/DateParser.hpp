// Copyright (c) 2026/10/26
//
// TJS2 日期字符串解析（tjsDateParser.cpp + tjsdate.y 完整对齐）。
#pragma once

#include <ctime>
#include <string>

namespace cial::stdlib {

    // 解析 TJS2 全部 11 种日期形态（英文月名/星期/AMPM/时区名/±hhmm 偏移/
    // 括号忽略段/两位年/秒与小数秒可省）。返回 time_t 秒：显式时区时为 UTC 秒，
    // 省略时区时为本地钟面秒（与原版 DateTime 同约定，localtime 往返一致）。
    // 文法错误抛 std::runtime_error("cannot parse date")；
    // mktime 失败抛 std::runtime_error("invalid value for timestamp")。
    time_t parseDateString(const std::string &s);

    // 本地时区相对 UTC 的偏移（秒，西正东负；TJS_timezone 对应物）
    long tjsTimezoneSeconds();

} // namespace cial::stdlib
