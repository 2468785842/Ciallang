// cial-fmt：TJS/Ciallang 源码格式化 CLI。词法、布局与安全写回实现在
// src/format/（CiallangFormat 库）；本文件只做 argparse 解析与输入输出编排。

#include "format/Files.hpp"
#include "format/Formatter.hpp"

#include <argparse/argparse.hpp>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include "cial/Version.hpp"
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace {
    namespace fs = std::filesystem;
    struct Input {
        fs::path path;
        std::string name, original, formatted;
        bool standardInput = false;
    };

    int run(const cial::Vec<std::string> &arguments) {
        argparse::ArgumentParser parser{ "cial-fmt", CIAL_VERSION_STRING };
        parser.add_description("Format TJS/Ciallang source. Without files, read standard input.\n"
                               "Default: write one formatted source to stdout.");
        parser.add_epilog("Exit codes: 0 success, 1 formatting needed, 2 input/usage/format error.");
        parser.add_argument("--write", "-i").flag().help("replace files in place");
        parser.add_argument("--check").flag().help("list files that need formatting; do not write");
        parser.add_argument("--indent-width").scan<'i', int>().help("spaces per indentation level (1..16; default 4)");
        parser.add_argument("files").nargs(argparse::nargs_pattern::any).help("files ('-' reads stdin)");
        parser.add_argument("--").remaining().help("treat all remaining arguments as file names");
        parser.parse_args(arguments);

        const bool write = parser.get<bool>("--write");
        const bool check = parser.get<bool>("--check");
        cial::format::Options options;
        if(parser.is_used("--indent-width")) {
            options.indentWidth = static_cast<unsigned>(parser.get<int>("--indent-width"));
            if(options.indentWidth < 1 || options.indentWidth > 16)
                throw std::runtime_error("indent width must be between 1 and 16");
        }
        cial::Vec<std::string> names = parser.get<std::vector<std::string>>("files");
        if(parser.is_used("--")) {
            const auto remaining = parser.get<std::vector<std::string>>("--");
            names.insert(names.end(), remaining.begin(), remaining.end());
        }
        if(write && check)
            throw std::runtime_error("--write and --check cannot be combined");
        if(names.empty())
            names.emplace_back("-");
        if(names.size() > 1 && !write && !check)
            throw std::runtime_error("multiple files require --write or --check");
        if(std::count(names.begin(), names.end(), "-") > 1 ||
           (write && std::find(names.begin(), names.end(), "-") != names.end()))
            throw std::runtime_error("--write requires files; standard input can only be used once");

        cial::Vec<Input> inputs;
        bool failed = false;
        for(const auto &name : names) {
            try {
                Input input;
                input.name = name == "-" ? "<stdin>" : name;
                input.standardInput = name == "-";
                input.path = fs::path(std::u8string(name.begin(), name.end()));
                if(input.standardInput) {
                    input.original.assign(std::istreambuf_iterator<char>(std::cin), {});
                    if(std::cin.bad())
                        throw std::runtime_error("cannot read standard input");
                } else {
                    if(!fs::is_regular_file(input.path))
                        throw std::runtime_error("not a regular file");
                    if(write && fs::is_symlink(fs::symlink_status(input.path)))
                        throw std::runtime_error("--write does not follow symbolic links");
                    input.original = cial::format::readFile(input.path);
                }
                cial::Common::Result diagnostics;
                if(!cial::format::formatSource(input.original, input.formatted, diagnostics, options, input.name)) {
                    for(const auto &message : diagnostics.messages()) {
                        std::cerr << input.name;
                        if(message.hasLocation())
                            std::cerr << ':' << message.location().start().line + 1 << ':'
                                      << message.location().start().column + 1;
                        std::cerr << ": " << message.message() << '\n';
                    }
                    failed = true;
                    continue;
                }
                const auto duplicate = std::find_if(inputs.begin(), inputs.end(), [&](const Input &other) {
                    return !input.standardInput && !other.standardInput &&
                        fs::weakly_canonical(other.path) == fs::weakly_canonical(input.path);
                });
                if(duplicate == inputs.end())
                    inputs.push_back(std::move(input));
            } catch(const std::exception &error) {
                std::cerr << name << ": " << error.what() << '\n';
                failed = true;
            }
        }
        if(failed)
            return 2;
        bool changed = false;
        for(const auto &input : inputs) {
            if(input.original != input.formatted) {
                changed = true;
                if(check)
                    std::cout << input.name << '\n';
                else if(write) {
                    try {
                        cial::format::replaceFile(input.path, input.original, input.formatted);
                    } catch(const std::exception &error) {
                        std::cerr << input.name << ": " << error.what() << '\n';
                        return 2;
                    }
                }
            }
            if(!check && !write)
                std::cout << input.formatted;
        }
        std::cout.flush();
        if(!std::cout)
            throw std::runtime_error("cannot write standard output");
        return check && changed ? 1 : 0;
    }
} // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#else
int main(int argc, char **argv) {
#endif
    try {
        // parse_args(vector) 契约要求首元素为程序名（内部跳过 argv[0]）
        cial::Vec<std::string> arguments;
        for(int i = 0; i < argc; ++i) {
#ifdef _WIN32
            const auto utf8 = std::filesystem::path(argv[i]).u8string();
            arguments.emplace_back(utf8.begin(), utf8.end());
#else
            arguments.emplace_back(argv[i]);
#endif
        }
        return run(arguments);
    } catch(const std::exception &error) {
        std::cerr << "cial-fmt: " << error.what() << '\n';
        return 2;
    }
}
