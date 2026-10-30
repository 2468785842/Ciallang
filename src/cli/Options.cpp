#include "Options.hpp"

#include <argparse/argparse.hpp>
#include <stdexcept>
#include <vector>

namespace cial::cli {
    Options parseOptions(const int argc, char **argv) {
        argparse::ArgumentParser parser{ "cial", "1.0.0" };
        parser.add_description("TJS2-compatible interpreter. With no input, start the REPL.");
        for(const auto &[name, help] : { std::pair{ "--tjsbc", "execute TJS2 bytecode" },
                                         { "--check-json", "compile only; emit JSON diagnostics" },
                                         { "--dump-cfg", "compile only; emit control-flow graphs" },
                                         { "--dump-ssa", "compile only; emit SSA analysis" },
                                         { "--repl", "start an interactive session" },
                                         { "--jit", "enable the optional x64 integer JIT" },
                                         { "--trace-jit", "enable the optional trace JIT" },
                                         { "--tail-recursion", "enable numeric self-tail recursion elimination" },
                                         { "--trace-execution", "record execution; takes input and output.dot" },
                                         { "--time", "report elapsed time on stderr" },
                                         { "--dump-tac", "dump compiled TAC on stderr before execution" } })
            parser.add_argument(name).flag().help(help);
        parser.add_argument("input")
            .nargs(argparse::nargs_pattern::any)
            .help("script; trace mode also takes output.dot");
        parser.add_argument("--").remaining().help("treat all remaining arguments as file names");
        parser.parse_args(argc, argv);

        Options options;
        int modes = 0;
        for(const auto &[flag, mode] : { std::pair{ "--tjsbc", Mode::Bytecode },
                                         { "--check-json", Mode::CheckJson },
                                         { "--dump-cfg", Mode::DumpCfg },
                                         { "--dump-ssa", Mode::DumpSsa },
                                         { "--repl", Mode::Repl } }) {
            if(parser.get<bool>(flag)) {
                options.mode = mode;
                ++modes;
            }
        }
        if(modes > 1)
            throw std::runtime_error("choose only one of --tjsbc, --check-json, --dump-cfg, --dump-ssa, --repl");
        if(parser.get<bool>("--jit") && parser.get<bool>("--trace-jit"))
            throw std::runtime_error("--jit and --trace-jit are mutually exclusive");
        options.jit = parser.get<bool>("--jit") ? vm::JitMode::Eager
            : parser.get<bool>("--trace-jit")   ? vm::JitMode::Trace
                                                : vm::JitMode::Off;
        options.tailRecursion = parser.get<bool>("--tail-recursion");
        options.timing = parser.get<bool>("--time");
        options.dumpTac = parser.get<bool>("--dump-tac");
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
            throw std::runtime_error(trace ? "usage: cial --trace-execution <script-file> <output.dot>"
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
        return options;
    }
} // namespace cial::cli
