#include "Formatter.hpp"

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <system_error>
#include "cial/Version.hpp"
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {
    namespace fs = std::filesystem;
    struct Input {
        fs::path path;
        std::string name, original, formatted;
        bool standardInput = false;
    };

    std::string read(const fs::path &path) {
        std::ifstream stream(path, std::ios::binary);
        if(!stream)
            throw std::runtime_error("cannot open file");
        std::string contents{ std::istreambuf_iterator<char>(stream), {} };
        if(stream.bad())
            throw std::runtime_error("cannot read file");
        return contents;
    }

    // 同目录排他创建临时文件，完整写入并保留权限后原子替换，失败清理临时文件。
    // 拒绝符号链接；所有输入格式化成功后才开始写回，不因词法错误截断源文件。
    void replace(const Input &input) {
        if(fs::symlink_status(input.path).type() != fs::file_type::regular)
            throw std::runtime_error("--write requires a regular file, not a symbolic link");
        if(read(input.path) != input.original)
            throw std::runtime_error("file changed since it was read");
        std::random_device random;
        fs::path temporary;
        int descriptor = -1;
        for(int attempt = 0; attempt < 32; ++attempt) {
            temporary = input.path;
            temporary += ".cial-fmt-" + std::to_string(random()) + ".tmp";
#ifdef _WIN32
            const auto status = _wsopen_s(&descriptor, temporary.c_str(), _O_CREAT | _O_EXCL | _O_WRONLY | _O_BINARY,
                                          _SH_DENYRW, _S_IREAD | _S_IWRITE);
            if(status != 0)
                errno = status;
#else
            descriptor = ::open(temporary.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0600);
#endif
            if(descriptor >= 0)
                break;
            if(errno != EEXIST)
                throw std::runtime_error(std::error_code(errno, std::generic_category()).message());
        }
        if(descriptor < 0)
            throw std::runtime_error("cannot create temporary file");
        auto closeFile = [&] {
#ifdef _WIN32
            return _close(descriptor);
#else
            return ::close(descriptor);
#endif
        };
        try {
            size_t position = 0;
            while(position < input.formatted.size()) {
                const auto count = std::min<size_t>(input.formatted.size() - position, 1024 * 1024);
#ifdef _WIN32
                const auto written =
                    _write(descriptor, input.formatted.data() + position, static_cast<unsigned>(count));
#else
                const auto written = ::write(descriptor, input.formatted.data() + position, count);
#endif
                if(written < 0 && errno == EINTR)
                    continue;
                if(written <= 0)
                    throw std::runtime_error("cannot write temporary file");
                position += static_cast<size_t>(written);
            }
            const auto closed = closeFile();
            descriptor = -1;
            if(closed != 0)
                throw std::runtime_error("cannot close temporary file");
            fs::permissions(temporary, fs::status(input.path).permissions());
#ifdef _WIN32
            if(!MoveFileExW(temporary.c_str(), input.path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                throw std::runtime_error("cannot replace source file (Windows error " + std::to_string(GetLastError()) +
                                         ")");
#else
            fs::rename(temporary, input.path);
#endif
        } catch(...) {
            if(descriptor >= 0)
                closeFile();
            std::error_code ignored;
            fs::permissions(temporary, fs::perms::owner_write, fs::perm_options::add, ignored);
            fs::remove(temporary, ignored);
            throw;
        }
    }

    int run(const cial::Vec<std::string> &arguments) {
        bool write = false, check = false, positional = false;
        cial::format::Options options;
        cial::Vec<std::string> names;
        for(size_t i = 0; i < arguments.size(); ++i) {
            const auto &argument = arguments[i];
            if(!positional && argument == "--") {
                positional = true;
            } else if(!positional && (argument == "--help" || argument == "-h")) {
                std::cout << "Usage: cial-fmt [--write | --check] [--indent-width N] [--] [FILE ... | -]\n"
                             "Format TJS/Ciallang source. Without files, read standard input.\n"
                             "Default: write one formatted source to stdout.\n"
                             "  -i, --write        replace files in place\n"
                             "  --check            list files that need formatting; do not write\n"
                             "  --indent-width N   spaces per indentation level (1..16; default 4)\n"
                             "  --version          print version\n"
                             "Exit codes: 0 success, 1 formatting needed, 2 input/usage/format error.\n";
                return 0;
            } else if(!positional && argument == "--version") {
                std::cout << CIAL_VERSION_STRING << '\n';
                return 0;
            } else if(!positional && (argument == "--write" || argument == "-i")) {
                write = true;
            } else if(!positional && argument == "--check") {
                check = true;
            } else if(!positional && (argument == "--indent-width" || argument.starts_with("--indent-width="))) {
                std::string_view value;
                if(argument == "--indent-width") {
                    if(++i == arguments.size())
                        throw std::runtime_error("--indent-width needs a value");
                    value = arguments[i];
                } else
                    value = std::string_view(argument).substr(15);
                const auto parsed = std::from_chars(value.data(), value.data() + value.size(), options.indentWidth);
                if(parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || options.indentWidth < 1 ||
                   options.indentWidth > 16)
                    throw std::runtime_error("indent width must be between 1 and 16");
            } else if(!positional && argument != "-" && argument.starts_with('-')) {
                throw std::runtime_error("unknown option: " + argument);
            } else
                names.push_back(argument);
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
                    input.original = read(input.path);
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
                        replace(input);
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
        cial::Vec<std::string> arguments;
        for(int i = 1; i < argc; ++i) {
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
