//
// Created by LiDong on 2026/1/19.
//
// tests/test_fuzz_regress.cpp
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include "common/SourceFile.hpp"
#include "parser/Parser.hpp"
#include "runtime/Context.hpp"
#include "runtime/Runtime.hpp"
#include "test_config.h"

using namespace cial;
namespace fs = std::filesystem;

static std::string read_file(const fs::path &p) {
    std::ifstream in(p, std::ios::binary);
    return { std::istreambuf_iterator(in), std::istreambuf_iterator<char>() };
}

void run_lexer(const std::string &input) {
    Runtime rt{};
    Context context{ rt };
    Common::SourceFile sourceFile;
    Common::Result r{};
    sourceFile.load(r, input);
    if(!r.isFailed()) {
        Syntax::Lexer lexer{ sourceFile, context.pp() };
        while(lexer.hasNext()) {
            Syntax::Token *t;
            lexer.next(r, t);
            if(r.isFailed())
                break;
            Syntax::Token take;
            lexer.takeOverToken(take);
        }
    }
}

TEST_CASE("fuzz regression", "[fuzz]") {

    for(const auto &entry : fs::directory_iterator(TEST_FUZZ_REGRESS_PATH)) {
        INFO("file = " << entry.path());

        REQUIRE_NOTHROW(run_lexer(read_file(entry.path())));
    }
}
