/*
 * Copyright (c) 2024/5/31 上午8:45
 *
 * /\  _` \   __          /\_ \  /\_ \
 * \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
 *  \ \ \/_/_\/\ \  /'__`\  \ \ \  \ \ \   /'__`\  /' _ `\  /'_ `\
 *   \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
 *    \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
 *     \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
 *                                                            /\____/
 *                                                            \_/__/
 *
 */
#pragma once

namespace Ciallang::Init {
    inline void MyPrefixFormatter(std::ostream& s, const google::LogMessage& m, void* /*data*/) {
        using namespace std;
        s << fmt::format("{} {:02d}{:02d}{:02d} {:02d}:{:02d}:{:02d}",
                    GetLogSeverityName(m.severity()),
                    1900 + m.time().year(), 1 + m.time().month(), m.time().day(),
                    m.time().hour(), m.time().min(), m.time().sec())
                << " " << m.thread_id()
                << fmt::format(" {}:{}]:", m.basename(), m.line());
    }

    // 初始化glog的函数
    inline void InitializeGlog(char** argv) {
        using namespace std::chrono_literals;

        auto logDir = std::filesystem::path(*argv)
                      .parent_path() / "log";

        if(!exists(logDir) && !create_directories(logDir)) {
            std::cerr << "create log directory falied: " << logDir << std::endl;
        }

        FLAGS_logtostderr = true;
        FLAGS_logtostdout = true;
        FLAGS_alsologtostderr = true;
        FLAGS_colorlogtostderr = true;
        FLAGS_colorlogtostdout = true;

        google::InitGoogleLogging(*argv);
        google::EnableLogCleaner(24h * 3);

        #define LOG_FILE(prefix) ((logDir / #prefix).string().c_str())

        // SetLogDestination(google::GLOG_INFO, LOG_FILE(INFO_));
        SetLogDestination(google::GLOG_WARNING, LOG_FILE(WARN_));
        SetLogDestination(google::GLOG_ERROR, LOG_FILE(ERROR_));
        SetLogDestination(google::GLOG_FATAL, LOG_FILE(FATAL_));

        google::SetLogFilenameExtension(".log");

        InstallPrefixFormatter(&MyPrefixFormatter);

        LOG(INFO) << "log directory: " << logDir;
    }
}
