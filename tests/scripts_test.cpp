// Copyright (c) 2026/10/30
//
// Scripts 宿主类行为锁定测试（krkr2 tTJSNC_Scripts / TVPExecuteStorage 对齐）：
// execStorage 语句脚本在全局上下文执行（兄弟脚本 var/函数落全局）、
// evalStorage 表达式求值返回、相对路径相对基目录（CLI 场景为当前执行
// 脚本所在目录）、缺失文件抛可捕获异常（消息含路径）。
#include <chrono>
#include <filesystem>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

#include "runtime/Context.hpp"
#include "runtime/Runtime.hpp"
#include "stdlib/ExceptionLib.hpp"
#include "stdlib/ScriptsLib.hpp"
#include "stdlib/StandardLibrary.hpp"
#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace cial;

namespace {
    // 独立临时目录：每个 SECTION 写入自己的语料脚本
    class TempDir {
    public:
        TempDir() {
            const auto base = std::filesystem::temp_directory_path();
            dir_ = base / ("cial_scripts_test_" + std::to_string(std::chrono::steady_clock::now()
                                                                      .time_since_epoch()
                                                                      .count()));
            std::filesystem::create_directories(dir_);
        }

        ~TempDir() noexcept {
            std::error_code ec;
            std::filesystem::remove_all(dir_, ec);
        }

        TempDir(const TempDir &) = delete;
        TempDir &operator=(const TempDir &) = delete;

        [[nodiscard]] const std::filesystem::path &path() const noexcept { return dir_; }

        void write(const char *name, const std::string &content) const {
            std::ofstream f{dir_ / name, std::ios::binary | std::ios::trunc};
            f << content;
        }

    private:
        std::filesystem::path dir_;
    };

    struct Session {
        Runtime rt;
        Context context{rt};
        vm::VMState vmState{context};
        VM vm{&vmState};

        explicit Session(const std::filesystem::path &baseDir) {
            stdlib::registerStandardLibrary(context);
            // registerStandardLibrary 已注册过一次；按测试基目录与 VMState 重绑定
            stdlib::registerScriptsLib(context, &vmState, baseDir);
            stdlib::initExceptionClass(vm);
        }
    };
} // namespace

TEST_CASE("Scripts.execStorage 在全局上下文执行语句脚本", "[stdlib][scripts]") {
    const TempDir dir;
    dir.write("sibling_exec.tjs",
              "var g_from_storage = 41;\n"
              "function g_func() { return g_from_storage + 1; }\n");

    Session session{dir.path()};

    // 嵌套执行：execStorage 在运行中的脚本回调内再入 VM
    session.vm.eval("Scripts.execStorage(\"sibling_exec.tjs\");"_str);

    // 顶层 var/函数声明落到全局（TJS2 context 实参省略语义）
    REQUIRE(*session.vm.evalExpr<Integer>("g_from_storage"_str) == 41);
    REQUIRE(*session.vm.evalExpr<Integer>("g_func()"_str) == 42);
}

TEST_CASE("Scripts.evalStorage 返回表达式值", "[stdlib][scripts]") {
    const TempDir dir;
    dir.write("expr.tjs", "6 * 7");

    Session session{dir.path()};

    session.vm.eval("var ev = Scripts.evalStorage(\"expr.tjs\");"_str);
    REQUIRE(*session.vm.evalExpr<Integer>("ev"_str) == 42);
}

TEST_CASE("Scripts 相对路径相对基目录解析", "[stdlib][scripts]") {
    const TempDir dir;
    std::filesystem::create_directories(dir.path() / "sub");
    // 进程 cwd 不在临时目录：能加载成功即证明按基目录（当前执行脚本所在目录）解析
    dir.write("sub/relative.tjs", "var rel = 9;\n");

    Session session{dir.path()};
    session.vm.eval("Scripts.execStorage(\"sub/relative.tjs\");"_str);
    REQUIRE(*session.vm.evalExpr<Integer>("rel"_str) == 9);

    // 绝对路径直用（正斜杠形式：脚本字符串字面量里 `\` 是转义前缀）
    const auto abs = (dir.path() / "sub" / "relative.tjs").generic_string();
    session.vm.eval(String{"Scripts.execStorage(\"" + abs + "\"); var rel2 = rel + 1;"});
    REQUIRE(*session.vm.evalExpr<Integer>("rel2"_str) == 10);
}

TEST_CASE("Scripts 缺失文件抛可捕获异常且消息含路径", "[stdlib][scripts]") {
    const TempDir dir;
    Session session{dir.path()};

    session.vm.eval(R"(
var hit = false;
var msg = "";
try {
    Scripts.execStorage("no_such_storage.tjs");
} catch(e) {
    hit = true;
    msg = e.message;
}
)"_str);
    REQUIRE(*session.vm.evalExpr<bool>("hit"_str));
    const auto msg = (*session.vm.evalExpr<String *>("msg"_str))->toStdStr();
    INFO("caught message: " << msg);
    REQUIRE(msg.find("no_such_storage") != std::string::npos);
}

TEST_CASE("Scripts.execStorage 内脚本抛出的异常向调用方传播", "[stdlib][scripts]") {
    const TempDir dir;
    dir.write("boom.tjs", "throw new Exception(\"boom-from-storage\");\n");

    Session session{dir.path()};
    session.vm.eval(R"(
var hit = false;
var msg = "";
try {
    Scripts.execStorage("boom.tjs");
} catch(e) {
    hit = true;
    msg = e.message;
}
)"_str);
    REQUIRE(*session.vm.evalExpr<bool>("hit"_str));
    REQUIRE((*session.vm.evalExpr<String *>("msg"_str))->toStdStr() == "boom-from-storage");
}

TEST_CASE("Scripts.evalStorage 缺失文件同样可捕获", "[stdlib][scripts]") {
    const TempDir dir;
    Session session{dir.path()};

    session.vm.eval(R"(
var hit = false;
try {
    Scripts.evalStorage("no_such_expr.tjs");
} catch(e) {
    hit = true;
}
)"_str);
    REQUIRE(*session.vm.evalExpr<bool>("hit"_str));
}
