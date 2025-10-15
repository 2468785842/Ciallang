// Copyright (c) 2025/9/29 10:56
//
// /\  _` \   __          /\_ \  /\_ \
// \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
//  \ \ \/_/_\/\ \  /'__`\  \ \ \  \ \ \   /'__`\  /' _ `\  /'_ `\
//   \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
//    \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
//     \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
//                                                            /\____
//                                                            \_/__/
//

//
// Created by LiDon on 2025/9/29.
//
#pragma once

#include "pch.h"

#include <functional>
#include <memory>
#include <string>

#if _DEBUG
#define CLL_LOG_TRACE(fmt, ...) ::Ciallang::logFmt(::Ciallang::Level::kTrace, fmt, ##__VA_ARGS__)
#define CLL_LOG_DEBUG(fmt, ...) ::Ciallang::logFmt(::Ciallang::Level::kDebug, fmt, ##__VA_ARGS__)
#else
#define CLL_LOG_TRACE(fmt, ...)
#define CLL_LOG_DEBUG(fmt, ...)
#endif

#define CLL_LOG_INFO(fmt, ...) ::Ciallang::logFmt(::Ciallang::Level::kInfo, fmt, ##__VA_ARGS__)
#define CLL_LOG_WARN(fmt, ...) ::Ciallang::logFmt(::Ciallang::Level::kWarn, fmt, ##__VA_ARGS__)
#define CLL_LOG_ERROR(fmt, ...) ::Ciallang::logFmt(::Ciallang::Level::kError, fmt, ##__VA_ARGS__)
#define CLL_LOG_FATAL(fmt, ...) ::Ciallang::logFmt(::Ciallang::Level::kFatal, fmt, ##__VA_ARGS__)

#if _DEBUG
#define CLL_ASSERT(cond, fmt, ...)
#else
#define CLL_ASSERT(cond, fmt, ...)                                                                                     \
    do {                                                                                                               \
        if(!(cond)) {                                                                                                  \
            ::Ciallang::assertFail(#cond, __FILE__, __LINE__, fmt, ##__VA_ARGS__);                                     \
            std::abort();                                                                                              \
        }                                                                                                              \
    } while(false)
#endif

namespace Ciallang {

    enum class Level {
        kTrace = 0,
        kDebug,
        kInfo,
        kWarn,
        kError,
        kFatal,
    };

    class Logger {
    public:
        virtual ~Logger() = default;

        virtual void init(const std::string &name, Level min_level) = 0;

        // 设置最小输出级别
        virtual void setLevel(Level level) = 0;
        [[nodiscard]] virtual Level getLevel() const = 0;

        virtual void log(Level level, const char *fmt, va_list ap) = 0;

        virtual void logStr(Level level, const std::string &msg) = 0;
    };

    using LoggerFactory = std::function<std::unique_ptr<Logger>(const std::string &name)>;

    void registerLoggerBackend(const std::string &backend_name, LoggerFactory factory);

    std::unique_ptr<Logger> createLogger(const std::string &backend_name, const std::string &logger_name);

    void setGlobalLogger(std::unique_ptr<Logger> logger);
    Logger *getGlobalLogger();

    void logFmt(Level level, const char *fmt, ...);
    void assertFail(const char *expr, const char *file, int line, const char *fmt, ...);
} // namespace Ciallang
