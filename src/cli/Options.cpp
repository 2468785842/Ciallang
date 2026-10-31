#include "Options.hpp"

#include <argparse/argparse.hpp>
#include <stdexcept>
#include <vector>
#include "cial/Version.hpp"

namespace cial::cli {
    Options parseOptions(const int argc, char** argv) {
        argparse::ArgumentParser parser{ "cial", CIAL_VERSION_STRING };
        parser.add_description("TJS2-compatible interpreter. With no input, start the REPL.");
        for(const auto& [name, help] :
            { std::pair{ "--tjsbc", "execute TJS2 bytecode" },
              { "--check-json", "compile only; emit JSON diagnostics" },
              { "--dump-cfg", "compile only; emit control-flow graphs" },
              { "--dump-ssa", "compile only; emit SSA analysis" },
              { "--repl", "start an interactive session" },
              { "--jit", "enable the optional x64 integer JIT" },
              { "--trace-jit", "enable the optional trace JIT" },
              { "--tail-recursion", "enable numeric self-tail recursion elimination" },
              { "--trace-execution", "record execution; takes input and output.dot" },
              { "--time", "report elapsed time on stderr" },
              { "--no-color", "disable colored diagnostics even on a terminal" },
              { "--dump-tac", "dump compiled TAC on stderr before execution" }
            })
            parser.add_argument(name).flag().help(help);
        parser.add_argument("--json-schema")
              .help("JSON diagnostics schema version (1 legacy, 2 half-open ranges; default 2)")
              .default_value(std::string{ "2" });
        parser.add_argument("--source-name")
              .help("logical source name reported in diagnostics (default: input path or <stdin>)");
        parser.add_argument("input")
              .nargs(argparse::nargs_pattern::any)
              .help("script ('-' reads stdin); trace mode also takes output.dot");
        parser.add_argument("--").remaining().help("treat all remaining arguments as file names");
        parser.parse_args(argc, argv);
        Options options;
        int modes = 0;
        for(const auto& [flag, mode] : { std::pair{ "--tjsbc", Mode::Bytecode },
                                         { "--check-json", Mode::CheckJson },
                                         { "--dump-cfg", Mode::DumpCfg },
                                         { "--dump-ssa", Mode::DumpSsa },
                                         { "--repl", Mode::Repl }
            }) {
            if(parser.get<bool>(flag)) {
                options.mode = mode;
                ++modes;
            }
        }
        if(modes > 1)
            throw std::runtime_error("choose only one of --tjsbc, --check-json, --dump-cfg, --dump-ssa, --repl");
        if(parser.get<bool>("--jit") && parser.get<bool>("--trace-jit"))
            throw std::runtime_error("--jit and --trace-jit are mutually exclusive");
        options.jit = parser.get<bool>("--jit")
                      ? vm::JitMode::Eager
                      : parser.get<bool>("--trace-jit")
                      ? vm::JitMode::Trace
                      : vm::JitMode::Off;
        options.tailRecursion = parser.get<bool>("--tail-recursion");
        options.timing = parser.get<bool>("--time");
        options.dumpTac = parser.get<bool>("--dump-tac");
        options.noColor = parser.get<bool>("--no-color");
        if(parser.is_used("--json-schema")) {
            const auto schema = parser.get<std::string>("--json-schema");
            try {
                options.jsonSchema = std::stoi(schema);
            } catch(const std::exception&) {
                throw std::runtime_error("--json-schema expects an integer (1 or 2)");
            }
            if(options.jsonSchema < 1 || options.jsonSchema > 2)
                throw std::runtime_error("--json-schema supports 1 (legacy) or 2 (current)");
        }
        if(parser.is_used("--source-name"))
            options.sourceName = parser.get<std::string>("--source-name");
        const bool trace = parser.get<bool>("--trace-execution");
        auto inputs = parser.get<std::vector<std::string>>("input");
        if(parser.is_used("--")) {
            const auto remaining = parser.get<std::vector<std::string>>("--");
            inputs.insert(inputs.end(), remaining.begin(), remaining.end());
        }
        if(inputs.empty() && modes == 0)
            options.mode = Mode::Repl;
        const size_t expected = options.mode == Mode::Repl ? 0 : trace ? 2 : 1;
        if(inputs.size() != expected)
            throw std::runtime_error(trace
                                     ? "usage: cial --trace-execution <script-file> <output.dot>"
                                     : options.mode == Mode::Repl
                                     ? "--repl does not accept an input file"
                                     : "expected exactly one input file; use --help for usage");
        if(!inputs.empty())
            options.input = inputs[0];
        if(trace && inputs.size() == 2)
            options.traceOutput = inputs[1];
        const bool compileOnly =
                options.mode == Mode::CheckJson || options.mode == Mode::DumpCfg || options.mode == Mode::DumpSsa;
        if(compileOnly &&
           (trace || options.jit != vm::JitMode::Off || options.tailRecursion || options.timing || options.dumpTac))
            throw std::runtime_error("compile-only modes do not accept execution options");
        if(options.mode == Mode::Repl && trace)
            throw std::runtime_error("--trace-execution requires an input file");
        if(!options.sourceName.empty() && options.input != "-")
            throw std::runtime_error("--source-name only applies to stdin input ('-')");
        return options;
    }
} // namespace cial::cli
