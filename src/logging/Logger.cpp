// Copyright (c) 2025/9/29 10:57
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

#include "Logger.hpp"
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>

namespace cial {

    namespace {
        std::map<std::string, LoggerFactory> &getRegistry() {
            static std::map<std::string, LoggerFactory> r;
            return r;
        }
        std::mutex &getRegistryMutex() {
            static std::mutex m;
            return m;
        }

        std::unique_ptr<Logger> g_global_logger;
        std::mutex g_global_mutex;
    } // namespace

    void registerLoggerBackend(const std::string &backend_name, LoggerFactory factory) {
        std::lock_guard lk(getRegistryMutex());
        getRegistry()[backend_name] = std::move(factory);
    }

    std::unique_ptr<Logger> createLogger(const std::string &backend_name, const std::string &logger_name) {
        std::lock_guard lk(getRegistryMutex());
        const auto it = getRegistry().find(backend_name);
        if(it == getRegistry().end()) {
            return nullptr;
        }
        return it->second(logger_name);
    }

    void setGlobalLogger(std::unique_ptr<Logger> logger) {
        std::lock_guard lk(g_global_mutex);
        g_global_logger = std::move(logger);
    }

    Logger *getGlobalLogger() {
        std::lock_guard lk(g_global_mutex);
        return g_global_logger.get();
    }

    void vformat_to_string(std::string &out, const char *fmt, va_list ap) {
        va_list ap_copy;
        va_copy(ap_copy, ap);
        const int needed = std::vsnprintf(nullptr, 0, fmt, ap_copy);
        va_end(ap_copy);
        if(needed < 0) {
            out = "logger: format error";
            return;
        }
        out.resize(needed);
        std::vsnprintf(&out[0], needed + 1, fmt, ap);
    }

    void logFmt(const Level level, const char *fmt, ...) {
        Logger *lg = getGlobalLogger();
        if(!lg) {
            // fallback to stderr
            va_list ap;
            va_start(ap, fmt);
            std::string s;
            vformat_to_string(s, fmt, ap);
            va_end(ap);
            auto lvl = "I";
            switch(level) {
                case Level::kTrace:
                    lvl = "T";
                    break;
                case Level::kDebug:
                    lvl = "D";
                    break;
                case Level::kInfo:
                    lvl = "I";
                    break;
                case Level::kWarn:
                    lvl = "W";
                    break;
                case Level::kError:
                    lvl = "E";
                    break;
                case Level::kFatal:
                    lvl = "F";
                    break;
            }
            std::fprintf(stderr, "[%s] %s\n", lvl, s.c_str());
            if(level == Level::kFatal) {
                std::fflush(stderr);
                std::abort();
            }
            return;
        }

        va_list ap;
        va_start(ap, fmt);
        lg->log(level, fmt, ap);
        va_end(ap);
    }

    void assertFail(const char *expr, const char *file, const int line, const char *fmt, ...) {
        std::string msg;
        va_list ap;
        va_start(ap, fmt);
        vformat_to_string(msg, fmt, ap);
        va_end(ap);

        CLL_LOG_FATAL("Assertion failed: (%s), file %s, line %d: %s", expr, file, line, msg.c_str());
    }
} // namespace cial