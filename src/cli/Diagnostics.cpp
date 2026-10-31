#include "Diagnostics.hpp"

#include <fstream>
#include <iterator>

#include <fmt/format.h>

#include "common/SourceFile.hpp"

namespace cial::cli {
    namespace {
        std::string jsonString(const std::string &text) {
            std::string out = "\"";
            for(const unsigned char c : text) {
                switch(c) {
                    case '"':
                        out += "\\\"";
                        break;
                    case '\\':
                        out += "\\\\";
                        break;
                    case '\n':
                        out += "\\n";
                        break;
                    case '\r':
                        out += "\\r";
                        break;
                    case '\t':
                        out += "\\t";
                        break;
                    default:
                        if(c < 32)
                            out += fmt::format("\\u{:04x}", c);
                        else
                            out += static_cast<char>(c);
                }
            }
            return out + '"';
        }

        const char *severityName(const Common::ResultMessage &message) {
            return message.isError() ? "error" : message.type() == Common::ResultMessage::warning ? "warning" : "note";
        }
    } // namespace

    void printDiagnostics(const Common::Result &diagnostics) {
        for(const auto &message : diagnostics.messages()) {
            std::string label{ fmt::format("cial: {}", severityName(message)) };
            if(!message.code().empty())
                label += fmt::format("[{}]", message.code());

            if(message.hasLocation()) {
                const auto &location = message.location();
                fmt::println(stderr, "{}: {}:{}:{}: {}", label, message.source(), location.start().line + 1,
                             location.start().column + 1, message.message());
            } else {
                fmt::println(stderr, "{}: {}", label, message.message());
            }

            // 源码摘录（含指示符）；颜色由 Colorizer 全局开关控制
            if(!message.details().empty())
                fmt::println(stderr, "{}", message.details());
        }
    }

    void printRuntimeError(const vm::VMState &state, const std::string &message) {
        const auto &fault = state.fault();
        if(fault.valid && !fault.source.empty()) {
            fmt::println(stderr, "cial: error: {}:{}:{}: {}", fault.source, fault.line + 1, fault.column + 1, message);
            // 摘录按文件身份在打印时重读（Python linecache 式）：Chunk 不保留正文，
            // stdin/REPL/<eval> 等逻辑名或文件已变动时读不到即跳过，不伪造内容
            if(std::ifstream in{ fault.source, std::ios::in | std::ios::binary }; in.is_open()) {
                const std::string content{ std::istreambuf_iterator<char>{ in }, std::istreambuf_iterator<char>{} };
                Common::SourceFile file;
                Common::Result loadResult;
                if(file.load(loadResult, content)) {
                    if(const auto excerpt = file.renderExcerptAt(fault.line, fault.column); !excerpt.empty())
                        fmt::println(stderr, "{}", excerpt);
                }
            }
        } else if(fault.valid) // 有行列但无源码身份：不伪造文件名
            fmt::println(stderr, "cial: error: (line {}): {}", fault.line + 1, message);
        else
            fmt::println(stderr, "cial: error: {}", message);
    }

    void printJsonDiagnostics(const Common::Result &diagnostics, const int schemaVersion,
                              const std::string &sourceName) {
        if(schemaVersion < 1 || schemaVersion > JsonSchemaVersion) {
            fmt::println(stderr, "cial: unknown json schema version: {}", schemaVersion);
            return;
        }

        if(schemaVersion == 1) {
            // 兼容输出：旧字段形状（endColumn 为包含式），供未迁移消费者过渡
            fmt::print("{{\"ok\":{},\"columnEncoding\":\"utf-8\",\"diagnostics\":[", !diagnostics.isFailed());
            bool first = true;
            for(const auto &message : diagnostics.messages()) {
                if(!first)
                    fmt::print(",");
                first = false;
                const auto &location = message.location();
                const auto endColumn = location.end().column > 0 ? location.end().column - 1 : 0;
                fmt::print(
                    "{{\"message\":{},\"severity\":{},\"line\":{},\"column\":{},\"endLine\":{},\"endColumn\":{}}}",
                    jsonString(message.message()), message.isError() ? 1 : 2, location.start().line,
                    location.start().column, location.end().line, endColumn);
            }
            fmt::println("]}}");
            return;
        }

        fmt::print("{{\"schema\":{},\"ok\":{},\"columnEncoding\":\"utf-8\"", JsonSchemaVersion,
                   !diagnostics.isFailed());
        // 顶层源码身份：优先显式名（无诊断也保留），否则取首个携带 source 的诊断
        if(!sourceName.empty()) {
            fmt::print(",\"source\":{}", jsonString(sourceName));
        } else {
            for(const auto &message : diagnostics.messages()) {
                if(!message.source().empty()) {
                    fmt::print(",\"source\":{}", jsonString(message.source()));
                    break;
                }
            }
        }
        fmt::print(",\"diagnostics\":[");
        bool first = true;
        for(const auto &message : diagnostics.messages()) {
            if(!first)
                fmt::print(",");
            first = false;
            const auto &location = message.location();
            fmt::print("{{\"code\":{},\"severity\":{},\"message\":{}", jsonString(message.code()),
                       message.isError() ? 1 : 2, jsonString(message.message()));
            if(message.hasLocation()) {
                fmt::print(",\"line\":{},\"column\":{},\"endLine\":{},\"endColumn\":{}", location.start().line,
                           location.start().column, location.end().line, location.end().column);
            }
            if(message.hasSpan()) {
                fmt::print(",\"range\":{{\"begin\":{},\"end\":{}}}", message.span().begin, message.span().end);
            }
            if(!message.source().empty())
                fmt::print(",\"source\":{}", jsonString(message.source()));
            fmt::print("}}");
        }
        fmt::println("]}}");
    }
} // namespace cial::cli
