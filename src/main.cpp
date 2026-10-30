#include <cstdio>
#include <exception>
#include <fmt/format.h>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#include "cli/Application.hpp"

int main(const int argc, char **argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
    setvbuf(stdout, nullptr, _IONBF, 0);
    cial::cli::Options options;
    try {
        options = cial::cli::parseOptions(argc, argv);
    } catch(const std::exception &error) {
        fmt::println(stderr, "cial: {}", error.what());
        return 2;
    }
    try {
        return cial::cli::runApplication(options);
    } catch(const std::exception &error) {
        fmt::println(stderr, "cial: {}", error.what());
        return 1;
    }
}
