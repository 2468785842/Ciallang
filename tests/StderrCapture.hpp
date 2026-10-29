#pragma once

#include <cstdio>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace cial::test {

    // fmt::println(stderr, ...) 绕过 C++ streambuf，必须捕获文件描述符。
    // 只在预期诊断的调用期间使用；离开作用域（含断言失败/异常）即恢复。
    class StderrCapture {
        FILE *_file = nullptr;
        int _saved = -1;

        static int descriptor(FILE *file) {
#ifdef _WIN32
            return _fileno(file);
#else
            return fileno(file);
#endif
        }

        static int duplicate(int fd) {
#ifdef _WIN32
            return _dup(fd);
#else
            return dup(fd);
#endif
        }

        static int redirect(int from, int to) {
#ifdef _WIN32
            return _dup2(from, to);
#else
            return dup2(from, to);
#endif
        }

        static void closeDescriptor(int fd) {
#ifdef _WIN32
            _close(fd);
#else
            close(fd);
#endif
        }

        void restore() noexcept {
            if(_saved >= 0) {
                std::fflush(stderr);
                redirect(_saved, descriptor(stderr));
                closeDescriptor(_saved);
                _saved = -1;
            }
        }

    public:
        StderrCapture() {
            std::fflush(stderr);
            _file = std::tmpfile();
            if(!_file)
                throw std::runtime_error("Cannot create stderr capture file");
            _saved = duplicate(descriptor(stderr));
            if(_saved < 0 || redirect(descriptor(_file), descriptor(stderr)) < 0) {
                restore();
                std::fclose(_file);
                throw std::runtime_error("Cannot redirect stderr");
            }
        }

        StderrCapture(const StderrCapture &) = delete;
        StderrCapture &operator=(const StderrCapture &) = delete;

        ~StderrCapture() {
            restore();
            std::fclose(_file);
        }

        // 先恢复，再做 Catch 断言，确保断言失败时诊断仍然可见。
        std::string finish() {
            restore();
            std::rewind(_file);
            std::string output;
            char buffer[1024];
            while(const auto count = std::fread(buffer, 1, sizeof(buffer), _file))
                output.append(buffer, count);
            if(std::ferror(_file))
                throw std::runtime_error("Cannot read captured stderr");
            return output;
        }
    };
} // namespace cial::test
