//
// Created by LiDong on 2026/1/19.
//
// tests/test_fuzz_regress.cpp
#include <catch2/catch_test_macros.hpp>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif
#include <filesystem>
#include <fstream>
#include "common/SourceFile.hpp"
#include "parser/Parser.hpp"
#include "parser/PreProcessor.hpp"
#include "test_config.h"

// 把 Debug 断言从弹窗重定向到 stderr，终端/CI 下可见
#ifdef _MSC_VER
static const bool g_redirectCrtReport = [] {
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    return true;
}();
#endif

using namespace cial;
namespace fs = std::filesystem;

static std::string read_file(const fs::path &p) {
    std::ifstream in(p, std::ios::binary);
    return { std::istreambuf_iterator(in), std::istreambuf_iterator<char>() };
}

static void run_lexer(const std::string &input) {
    PreProcessor preProcessor;
    Common::SourceFile sourceFile;
    Common::Result r{};
    sourceFile.load(r, input);
    if(!r.isFailed()) {
        syntax::Lexer lexer{ sourceFile, preProcessor };
        while(lexer.hasNext()) {
            syntax::Token *t{};
            lexer.next(t);
            syntax::Token take;
            lexer.takeOverToken(take);
        }
    }
}

static void run_parser(const std::string &input) {
    PreProcessor preProcessor;
    Common::SourceFile sourceFile;
    Common::Result result;
    sourceFile.load(result, input);
    if(result.isFailed())
        return;
    syntax::Parser parser{ sourceFile, preProcessor };
    parser.parse(result);
}

TEST_CASE("fuzz regression", "[fuzz]") {

    for(const auto &entry : fs::directory_iterator(TEST_FUZZ_REGRESS_PATH)) {
        if(!entry.is_regular_file() || entry.path().extension() != ".tjs")
            continue;
        INFO("file = " << entry.path());

        REQUIRE_NOTHROW(run_lexer(read_file(entry.path())));
        REQUIRE_NOTHROW(run_parser(read_file(entry.path())));
    }
}
