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
#include <memory>
#include <string>
#include <cstdarg>
#include <functional>

#define CLL_LOG_TRACE(fmt, ...) ::Ciallang::LogFmt(::Ciallang::Level::kTrace, fmt, ##__VA_ARGS__)
#define CLL_LOG_DEBUG(fmt, ...) ::Ciallang::LogFmt(::Ciallang::Level::kDebug, fmt, ##__VA_ARGS__)
#define CLL_LOG_INFO(fmt, ...)  ::Ciallang::LogFmt(::Ciallang::Level::kInfo,  fmt, ##__VA_ARGS__)
#define CLL_LOG_WARN(fmt, ...)  ::Ciallang::LogFmt(::Ciallang::Level::kWarn,  fmt, ##__VA_ARGS__)
#define CLL_LOG_ERROR(fmt, ...) ::Ciallang::LogFmt(::Ciallang::Level::kError, fmt, ##__VA_ARGS__)
#define CLL_LOG_FATAL(fmt, ...) ::Ciallang::LogFmt(::Ciallang::Level::kFatal, fmt, ##__VA_ARGS__)

#define CLL_ASSERT(cond, fmt, ...) \
do { \
    if (!(cond)) { \
        ::Ciallang::AssertFail(#cond, __FILE__, __LINE__, fmt, ##__VA_ARGS__); \
    } \
} while (false)

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

        virtual void Init(const std::string& name, Level min_level) = 0;

        // 设置最小输出级别
        virtual void SetLevel(Level level) = 0;
        [[nodiscard]] virtual Level GetLevel() const = 0;

        virtual void Log(Level level, const char* fmt, va_list ap) = 0;

        virtual void LogStr(Level level, const std::string& msg) = 0;
    };

    using LoggerFactory = std::function<std::unique_ptr<Logger>(const std::string& name)>;

    void RegisterLoggerBackend(const std::string& backend_name, LoggerFactory factory);

    std::unique_ptr<Logger> CreateLogger(const std::string& backend_name, const std::string& logger_name);

    void SetGlobalLogger(std::unique_ptr<Logger> logger);
    Logger* GetGlobalLogger(); // 不拥有

    void LogFmt(Level level, const char* fmt, ...);
    void AssertFail(const char* expr, const char* file, int line, const char* fmt, ...);
}
