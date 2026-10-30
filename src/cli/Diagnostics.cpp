#include "Diagnostics.hpp"

#include <fmt/format.h>

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
    } // namespace

    void printDiagnostics(const Common::Result &diagnostics) {
        for(const auto &message : diagnostics.messages())
            fmt::println(stderr, "cial: {}: {}", message.isError() ? "error" : "warning", message.message());
    }

    void printJsonDiagnostics(const Common::Result &diagnostics) {
        fmt::print("{{\"ok\":{},\"columnEncoding\":\"utf-8\",\"diagnostics\":[", !diagnostics.isFailed());
        bool first = true;
        for(const auto &message : diagnostics.messages()) {
            if(!first)
                fmt::print(",");
            first = false;
            const auto &location = message.location();
            fmt::print("{{\"message\":{},\"severity\":{},\"line\":{},\"column\":{},\"endLine\":{},\"endColumn\":{}}}",
                       jsonString(message.message()), message.isError() ? 1 : 2, location.start().line,
                       location.start().column, location.end().line, location.end().column);
        }
        fmt::println("]}}");
    }
} // namespace cial::cli
