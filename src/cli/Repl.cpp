#include "Repl.hpp"

#include <cstdio>
#include <iostream>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "Compilation.hpp"
#include "Diagnostics.hpp"
#include "parser/Lexer.hpp"

namespace cial::cli {
    bool isInteractiveInput() {
#ifdef _WIN32
        return _isatty(_fileno(stdin)) != 0;
#else
        return isatty(fileno(stdin)) != 0;
#endif
    }

    bool needsMoreInput(const std::string& text) {
        Common::SourceFile source;
        Common::Result diagnostics;
        source.load(diagnostics, text);
        if(diagnostics.isFailed())
            return false;
        PreProcessor preprocessor;
        syntax::Lexer lexer{ source, preprocessor };
        Vec<syntax::TokenType> brackets;
        syntax::Token* token{};
        do {
            const bool success = lexer.next(token, false);
            if(!token)
                return false;
            using enum syntax::TokenType;
            switch(token->type()) {
                case LParenthesis:
                    brackets.push_back(RParenthesis);
                    break;
                case LBracket:
                    brackets.push_back(RBracket);
                    break;
                case LeftCurlyBrace:
                    brackets.push_back(RightCurlyBrace);
                    break;
                case RParenthesis:
                case RBracket:
                case RightCurlyBrace:
                    if(brackets.empty() || brackets.back() != token->type())
                        return false;
                    brackets.pop_back();
                    break;
                case Error:
                    return lexer.incomplete();
                default:
                    break;
            }
            if(!success)
                return false;
        } while(lexer.hasNext());
        return lexer.incomplete() || !brackets.empty();
    }

    int runRepl(vm::VMState& state, std::istream& input, const bool interactive) {
        if(interactive)
            fmt::println("Ciallang REPL. :help for commands, :quit to exit.");
        std::string buffer;
        std::string line;
        bool failed = false;
        while(true) {
            if(interactive) {
                fmt::print("{}", buffer.empty() ? ">>> " : "... ");
                std::fflush(stdout);
            }
            if(!std::getline(input, line))
                break;
            if(line == ":quit" || line == ":q")
                return interactive ? 0 : failed ? 1 : 0;
            if(line == ":abort") {
                buffer.clear();
                continue;
            }
            if(line == ":help") {
                fmt::println(":help  show commands\n:abort discard pending input\n:quit  exit\n"
                    "Expressions print their value; declarations share the session's globals.\n"
                    "Open brackets, strings and block comments continue on the next line.\n"
                    "End a line with \\ to continue an expression.");
                continue;
            }
            if(buffer.empty() && line.find_first_not_of(" \t\r") == std::string::npos)
                continue;
            const bool continuation = !line.empty() && line.back() == '\\';
            if(continuation)
                line.pop_back();
            buffer += line + '\n';
            if(continuation || needsMoreInput(buffer))
                continue;
            try {
                // Speculative expression parsing uses its own preprocessor, so
                // @set/@if are applied only by the selected script compilation.
                Context probe{ state.rt };
                Common::SourceFile expressionSource;
                Common::Result expressionDiagnostics;
                expressionSource.load(expressionDiagnostics, buffer);
                auto expression = compile(expressionDiagnostics, probe, expressionSource, true);
                if(!expressionDiagnostics.isFailed() && expression.chunk && expression.result) {
                    const auto value = execute(state, std::move(*expression.chunk), expression.result);
                    if(!value.isVoid())
                        fmt::println("{}", value);
                } else {
                    Common::SourceFile source;
                    Common::Result diagnostics;
                    source.load(diagnostics, buffer);
                    auto script = compile(diagnostics, state.context, source);
                    printDiagnostics(diagnostics);
                    if(diagnostics.isFailed())
                        failed = true;
                    else if(script.chunk)
                        execute(state, std::move(*script.chunk));
                }
            } catch(const std::exception& error) {
                // VM 只传播异常值；文件与位置（如有）由共享渲染器附加
                printRuntimeError(state, error.what());
                failed = true;
            }
            buffer.clear();
        }
        if(!buffer.empty()) {
            fmt::println(stderr, "cial: incomplete input at end of REPL session");
            failed = true;
        }
        if(input.bad()) {
            fmt::println(stderr, "cial: cannot read REPL input");
            failed = true;
        }
        return interactive ? 0 : failed ? 1 : 0;
    }
} // namespace cial::cli
