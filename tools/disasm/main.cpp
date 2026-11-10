// cial-disasm：TJS2 字节码 / Ciallang 源码的反汇编 CLI（TAC 层，不执行、不恢复）。
// 字节码读取在 src/tjsbc/，指令与来源表渲染在 src/decompile/；本文件只做
// 输入探测、参数解析与输出编排。

#include "cial/Version.hpp"
#include "cli/Compilation.hpp"
#include "cli/Diagnostics.hpp"
#include "common/SourceFile.hpp"
#include "decompile/Decompiler.hpp"
#include "tjsbc/TjsBytecodeReader.hpp"
#include "tjsbc/TjsTranslator.hpp"

#include <argparse/argparse.hpp>

#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace {
    namespace fs = std::filesystem;

    // 输入探测与 `cial --decompile` 同一规则：前 8 字节命中 cial::tjsbc::FILE_TAG
    // 视为 TJS2 字节码，否则按 Ciallang 源码编译
    bool looksLikeTjsBytecode(const fs::path &path) {
        std::ifstream input{ path, std::ios::binary };
        if(!input)
            throw std::runtime_error("cannot open input: " + path.string());
        char magic[8]{};
        input.read(magic, sizeof(magic));
        return input.gcount() == 8 && std::memcmp(magic, cial::tjsbc::FILE_TAG, 8) == 0;
    }

    void writeOutput(const std::string &text, const std::string &path, const char *what) {
        if(path.empty()) {
            std::cout << text;
            if(!std::cout)
                throw std::runtime_error("cannot write standard output");
            return;
        }
        std::ofstream output{ path, std::ios::binary };
        if(!output)
            throw std::runtime_error(std::string{ "cannot write " } + what + ": " + path);
        output << text;
        if(!output)
            throw std::runtime_error(std::string{ "cannot write " } + what + ": " + path);
    }

    int run(const cial::Vec<std::string> &arguments) {
        argparse::ArgumentParser parser{ "cial-disasm", CIAL_VERSION_STRING };
        parser.add_description(
            "Disassemble TJS2 bytecode or compiled Ciallang source to TAC text (no execution, no recovery).");
        parser.add_epilog("Exit codes: 0 success, 1 compile failure, 2 input/usage error.");
        parser.add_argument("--output", "-o")
            .default_value(std::string{})
            .help("write the disassembly to this file (default: stdout)");
        parser.add_argument("--map")
            .default_value(std::string{})
            .help("write the line table JSON (text line to TAC ranges, TJS code words, source lines)");
        parser.add_argument("--source")
            .default_value(std::string{})
            .help("original TJS2 source for offset-to-line mapping on bytecode input");
        parser.add_argument("input").required().help("bytecode file (magic-probed) or Ciallang source file");
        parser.parse_args(arguments);

        const auto input = parser.get<std::string>("input");
        const auto output = parser.get<std::string>("--output");
        const auto map = parser.get<std::string>("--map");
        if(!map.empty() && !output.empty() &&
           fs::weakly_canonical(fs::path{ output }) == fs::weakly_canonical(fs::path{ map }))
            throw std::runtime_error("--map output must differ from --output");

        cial::Runtime runtime;
        cial::Context context{ runtime };
        cial::vm::VMState state{ context };

        cial::decompile::Options options{ .diagnosticView = true, .disassemblyOnly = true };
        cial::Common::SourceFile originalSource;
        if(parser.is_used("--source")) {
            cial::Common::Result result;
            originalSource = cial::Common::SourceFile{ fs::path{ parser.get<std::string>("--source") } };
            originalSource.load(result);
            if(result.isFailed())
                throw std::runtime_error("cannot load --source: " + parser.get<std::string>("--source"));
            options.originalSource = &originalSource;
        }

        cial::decompile::DecompileResult result;
        if(looksLikeTjsBytecode(input)) {
            const auto file = cial::tjsbc::readTjsBytecodeFile(input);
            cial::tjsbc::TjsTranslator translator{ state.rt, file };
            result = cial::decompile::decompileProgram(state.rt, translator.translate(), options);
        } else {
            cial::Common::Result diagnostics;
            cial::Common::SourceFile source{ fs::path{ input } };
            source.load(diagnostics);
            if(diagnostics.isFailed()) {
                cial::cli::printDiagnostics(diagnostics);
                return 1;
            }
            const cial::cli::Compilation compilation = cial::cli::compile(diagnostics, context, source);
            cial::cli::printDiagnostics(diagnostics);
            if(diagnostics.isFailed() || !compilation.chunk)
                return 1;
            result = cial::decompile::decompileChunk(state.rt, *compilation.chunk, options);
        }

        writeOutput(result.text(), output, "--output");
        if(!map.empty())
            writeOutput(result.mapJson(), map, "--map");
        return 0;
    }
} // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
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
        std::cerr << "cial-disasm: " << error.what() << '\n';
        return 2;
    }
}
