#pragma once

#include <map>
#include <string>
#include <utility>

#include "Chunk.hpp"
#include "runtime/Runtime.hpp"

namespace cial::vm {
    // 聚合实际派发的指令与相邻执行边；内存随代码位置/边数增长，而非运行步数。
    // 只保存数值和文本，不延长脚本对象寿命，GC 后仍可导出。
    class ExecutionTrace {
    public:
        using Location = std::pair<u64, u64>; // chunk identity, bytecode offset
        struct Node {
            std::string function;
            std::string opcode;
            Opt<u32> line;
            u64 hits{};
        };
        using Edge = std::pair<Location, Location>;

        class RunGuard {
        public:
            explicit RunGuard(ExecutionTrace *trace) : _trace(trace) {
                if(_trace && _trace->_depth++ == 0)
                    _trace->_last.reset();
            }
            ~RunGuard() {
                if(_trace)
                    --_trace->_depth;
            }
            RunGuard(const RunGuard &) = delete;
            RunGuard &operator=(const RunGuard &) = delete;

        private:
            ExecutionTrace *_trace;
        };

        void record(Chunk &chunk, const u64 pc, Runtime &rt, const FuncMeta *function) {
            const Location location{ chunk.executionTraceId(), pc };
            auto [it, inserted] = _nodes.try_emplace(location);
            auto &node = it->second;
            if(inserted) {
                node.function = "top-level";
                if(function) {
                    node.function = "anonymous";
                    if(const auto *entry = rt.atomTable.get(function->name); entry && entry->str)
                        node.function = entry->str->toStdStr();
                }
                node.opcode = opcodeName(static_cast<VmOpCode>(chunk.code()[pc]));
                if(const auto source = chunk.lineAtBc(pc))
                    node.line = source->start().line + 1;
            }
            ++node.hits;
            ++_instructions;
            if(_last)
                ++_edges[{ *_last, location }];
            _last = location;
        }

        [[nodiscard]] const auto &nodes() const noexcept { return _nodes; }
        [[nodiscard]] const auto &edges() const noexcept { return _edges; }
        [[nodiscard]] u64 instructions() const noexcept { return _instructions; }

        [[nodiscard]] std::string dumpDot() const {
            std::string out =
                "digraph execution {\n  label=\"Executed instructions: " + std::to_string(_instructions) + "\";\n";
            for(const auto &[location, node] : _nodes) {
                out += "  " + nodeId(location) + " [label=\"" + escape(node.function) + "\\nchunk " +
                    std::to_string(location.first) + " pc " + std::to_string(location.second) + " " + node.opcode;
                if(node.line)
                    out += "\\nline " + std::to_string(*node.line);
                out += "\\nhits " + std::to_string(node.hits) + "\"];\n";
            }
            for(const auto &[edge, count] : _edges) {
                out += "  " + nodeId(edge.first) + " -> " + nodeId(edge.second) + " [label=\"" + std::to_string(count) +
                    "\"";
                if(edge.first.first == edge.second.first && edge.second.second <= edge.first.second)
                    out += ", color=red";
                out += "];\n";
            }
            return out + "}\n";
        }

    private:
        static std::string nodeId(const Location &location) {
            return "c" + std::to_string(location.first) + "p" + std::to_string(location.second);
        }
        static std::string escape(const std::string &text) {
            std::string out;
            for(const char c : text) {
                if(c == '\\' || c == '"')
                    out += '\\';
                if(c == '\n')
                    out += "\\n";
                else if(c == '\r')
                    out += "\\r";
                else
                    out += c;
            }
            return out;
        }
        static const char *opcodeName(const VmOpCode opcode) {
            switch(opcode) {
#define TRACE_OPCODE(op)                                                                                               \
    case VmOpCode::op:                                                                                                 \
        return #op;
                CIAL_BYTECODE_OPCODE_ENUMS(TRACE_OPCODE)
#undef TRACE_OPCODE
            }
            return "unknown";
        }
        std::map<Location, Node> _nodes;
        std::map<Edge, u64> _edges;
        Opt<Location> _last;
        u64 _instructions{};
        size_t _depth{};
    };
} // namespace cial::vm
