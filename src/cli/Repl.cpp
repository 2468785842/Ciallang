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

    namespace {
        struct InputScan {
            bool needsMore{}; // 输入未完：未闭合括号、字符串或注释，等待续行
            bool terminated{ true }; // 缺省不补分号：残缺或错误输入交给编译路径报告
            bool sawToken{};
            syntax::TokenType lastToken{ syntax::TokenType::EndOfFile };
        };

        // 与编译一致的词法扫描：判定是否需要续行，并记录最后一个有效 token，
        // 供缺省分号补齐判断使用
        InputScan scanInput(const std::string &text) {
            InputScan scan;
            Common::SourceFile source;
            Common::Result diagnostics;
            source.load(diagnostics, text);
            if(diagnostics.isFailed())
                return scan;
            PreProcessor preprocessor;
            syntax::Lexer lexer{ source, preprocessor };
            Vec<syntax::TokenType> brackets;
            syntax::Token *token{};
            do {
                const bool success = lexer.next(token, false);
                if(!token)
                    return scan;
                using enum syntax::TokenType;
                const auto type = token->type();
                // 注释与错误 token 均不参与终结判断：前者编译时跳过，后者交给编译路径报告
                if(type != EndOfFile && type != Error && type != LineComment && type != BlockComment) {
                    scan.lastToken = type;
                    scan.sawToken = true;
                }
                switch(type) {
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
                        if(brackets.empty() || brackets.back() != type)
                            return scan;
                        brackets.pop_back();
                        break;
                    case Error:
                        scan.needsMore = lexer.incomplete();
                        return scan;
                    default:
                        break;
                }
                if(!success)
                    return scan;
            } while(lexer.hasNext());
            scan.needsMore = lexer.incomplete() || !brackets.empty();
            // 顶层以 } 结束的声明（function/class/property/块）无需分号；
            // 纯注释输入没有有效 token，同样保持原样
            scan.terminated = !scan.sawToken || scan.lastToken == syntax::TokenType::SemiColon ||
                scan.lastToken == syntax::TokenType::RightCurlyBrace;
            return scan;
        }
    } // namespace

    int runRepl(vm::VMState &state, std::istream &input, const bool interactive) {
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
                             "A trailing ';' is optional and is inserted automatically.\n"
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
            const auto scan = scanInput(buffer);
            if(continuation || scan.needsMore)
                continue;
            // 统一表达式与声明的回车行为：声明经脚本路径要求分号而表达式不需要，
            // 缺行尾分号时按 TJS2 语句终结符补齐
            if(!scan.terminated)
                buffer += ';';
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
            } catch(const std::exception &error) {
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
