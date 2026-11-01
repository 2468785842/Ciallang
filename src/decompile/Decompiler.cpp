/*
 * Copyright (c) 2026/11/1
 *
 * 字节码反编译器实现（M0：反汇编级诊断视图 + {文本, 行表} 管道）。
 * 见 Decompiler.hpp 与 doc/bytecode/decompiler-plan.md。
 */

#include "Decompiler.hpp"

#include <algorithm>
#include <iterator>
#include <unordered_set>

#include <fmt/format.h>

#include "gen/Instruction.hpp"
#include "runtime/Runtime.hpp"

namespace cial::decompile {
    namespace {
        std::string atomName(Runtime &rt, const Atom atom) {
            if(atom == ATOM_INVALID)
                return {};
            if(const auto *entry = rt.atomTable.get(atom); entry && entry->str)
                return entry->str->toStdStr();
            return {};
        }

        std::string displayName(Runtime &rt, const Atom atom, const char *fallback) {
            const auto name = atomName(rt, atom);
            return name.empty() ? fallback : name;
        }

        // TJS2 源偏移按 UTF-16 码元计；SourceFile 缓冲为 UTF-8——按码点步进换算成
        // 字节下标（补充平面码点占两个码元）。越界截断到缓冲末尾
        size_t utf16UnitsToByteIndex(const std::vector<u8> &text, const i32 units) {
            size_t i = 0;
            i32 seen = 0;
            while(i < text.size() && seen < units) {
                const auto lead = static_cast<unsigned char>(text[i]);
                size_t len = 1;
                if(lead >= 0xF0)
                    len = 4;
                else if(lead >= 0xE0)
                    len = 3;
                else if(lead >= 0xC0)
                    len = 2;
                seen += len == 4 ? 2 : 1;
                i += len;
            }
            return std::min(i, text.size());
        }

        // 逐行构造单元文本并登记行表；textLine 相对整个输出（游标跨单元累加）
        class UnitBuilder {
        public:
            UnitBuilder(DecompiledUnit &unit, u32 &cursor) : _unit(unit), _cursor(cursor) {}

            void line(const std::string &text, LineMapRow row) {
                row.textLine = _cursor++;
                _unit.text += text;
                _unit.text += '\n';
                _unit.lines.push_back(row);
            }

            void synthetic(const std::string &text) { line(text, LineMapRow{ .synthetic = true }); }

        private:
            DecompiledUnit &_unit;
            u32 &_cursor;
        };

        // 体的反汇编级视图：每条 TAC 一行，行表区间 [i, i+1)。bc 路径按来源表附带
        // 码字/源偏移；提供原源码时把偏移换算为行号
        void emitBody(UnitBuilder &builder, vm::Chunk &chunk, const Vec<tjsbc::TacOrigin> *origin,
                      const Common::SourceFile *source) {
            auto &instructions = chunk.getInstVec();
            for(size_t i = 0; i < instructions.size(); ++i) {
                LineMapRow row{ .tacStart = static_cast<u32>(i), .tacEnd = static_cast<u32>(i) + 1 };
                std::string suffix;
                if(origin && !origin->empty()) {
                    // 来源表按 tacIdx 升序：取 ≤ i 的最后一项（一条 TJS2 指令展开的多条
                    // TAC 共享同一来源）
                    const auto it = std::upper_bound(
                        origin->begin(), origin->end(), i,
                        [](const size_t value, const tjsbc::TacOrigin &entry) { return value < entry.tacIdx; });
                    if(it != origin->begin()) {
                        const auto &entry = *std::prev(it);
                        row.tjsWordStart = entry.tjsWord;
                        row.sourceOffset = entry.sourceOffset;
                        suffix = fmt::format("  ; w{}", entry.tjsWord);
                        if(entry.sourceOffset) {
                            suffix += fmt::format(" src@{}", *entry.sourceOffset);
                            if(source) {
                                const auto location = source->locationByIndex(
                                    utf16UnitsToByteIndex(source->buffer(), *entry.sourceOffset));
                                row.sourceLine = location.start().line;
                                suffix += fmt::format(" L{}", location.start().line);
                            }
                        }
                    }
                }
                builder.line(fmt::format("//   @{:<4} {}{}", i, instructions[i]->dump(nullptr), suffix), row);
            }
        }

        void emitLocals(UnitBuilder &builder, Runtime &rt, const FuncMeta &meta) {
            if(meta.localVars.empty())
                return;
            std::string line = "//   locals:";
            for(const auto &local : meta.localVars)
                line += fmt::format(" %{}={}", local.reg.index(), displayName(rt, local.identifier, "?"));
            builder.synthetic(line);
        }

        void markUnsupported(DecompileResult &result, const DecompiledUnit &unit) {
            result.diagnostics.push_back(
                fmt::format("{} '{}': body recovery not implemented (M0 disassembly view only)", unit.kind, unit.name));
        }

        // ── 自产路径：沿常量图收集嵌套单元 ──
        class ChunkTreeCollector {
        public:
            ChunkTreeCollector(Runtime &rt, const Options &options, DecompileResult &result) :
                _rt(rt), _options(options), _result(result) {}

            void header() {
                auto &unit = _result.units.emplace_back();
                unit.name = "<header>";
                unit.kind = "header";
                UnitBuilder builder{ unit, _cursor };
                builder.synthetic("// cial decompile (M0): disassembly view, not recompilable");
            }

            void topLevel(vm::Chunk &chunk) {
                auto &unit = _result.units.emplace_back();
                unit.name = "<top-level>";
                unit.kind = "top-level";
                UnitBuilder builder{ unit, _cursor };
                builder.synthetic("// ==== top-level ====");
                emitBody(builder, chunk, nullptr, nullptr);
                markUnsupported(_result, unit);
                walkConstants(chunk);
            }

        private:
            void walkConstants(vm::Chunk &chunk) {
                for(auto &constant : chunk.getConstants()) {
                    switch(constant.type()) {
                        case ConstantType::FuncMeta:
                            function(*constant.value<FuncMeta *>(), "function");
                            break;
                        case ConstantType::ClassMeta:
                            klass(*constant.value<ClassMeta *>());
                            break;
                        case ConstantType::PropMeta:
                            property(*constant.value<PropMeta *>());
                            break;
                        default:
                            break;
                    }
                }
            }

            void function(FuncMeta &meta, const char *kind) {
                if(!_visited.insert(&meta).second || !meta.chunk)
                    return;
                auto &unit = _result.units.emplace_back();
                unit.name = displayName(_rt, meta.name, "<anonymous>");
                unit.kind = kind;
                UnitBuilder builder{ unit, _cursor };
                builder.synthetic(fmt::format("// ==== {} {} (arity {}{}) ====", kind, unit.name, meta.arity,
                                              meta.hasCollapse ? ", collapse" : ""));
                emitLocals(builder, _rt, meta);
                emitBody(builder, *meta.chunk, nullptr, nullptr);
                markUnsupported(_result, unit);
                walkConstants(*meta.chunk);
            }

            void property(PropMeta &meta) {
                if(!_visited.insert(&meta).second)
                    return;
                if(meta.getFunc)
                    function(*meta.getFunc, "property-getter");
                if(meta.setFunc)
                    function(*meta.setFunc, "property-setter");
            }

            void klass(ClassMeta &meta) {
                if(!_visited.insert(&meta).second)
                    return;
                auto &unit = _result.units.emplace_back();
                unit.name = displayName(_rt, meta.className, "<anonymous>");
                unit.kind = "class";
                UnitBuilder builder{ unit, _cursor };
                std::string extends;
                for(const auto base : meta.extends)
                    extends += fmt::format("{}{}", extends.empty() ? " extends " : ", ", displayName(_rt, base, "?"));
                builder.synthetic(fmt::format("// ==== class {}{} ====", unit.name, extends));
                for(size_t i = 0; i < meta.memberShapeMetas.size(); ++i) {
                    const auto &shape = meta.memberShapeMetas[i];
                    builder.synthetic(fmt::format("//   {}{} {}", shape.isStatic ? "static " : "",
                                                  shape.isMethod     ? "method"
                                                      : shape.isProp ? "property"
                                                                     : "var",
                                                  displayName(_rt, shape.name, "?")));
                }
                // 壳只描述结构；类体无 TAC（方案 §1），成员函数/属性/构造/字段初始化各成单元
                if(meta.constructor)
                    function(*meta.constructor, "constructor");
                if(meta.initDefaultVal)
                    function(*meta.initDefaultVal, "class-field-init");
                for(size_t i = 0; i < meta.memberShapeMetas.size() && i < meta.memberMetas.size(); ++i) {
                    const auto &shape = meta.memberShapeMetas[i];
                    if(shape.isMethod && meta.memberMetas[i].funcMeta)
                        function(*meta.memberMetas[i].funcMeta, "method");
                    else if(shape.isProp && meta.memberMetas[i].propMeta)
                        property(*meta.memberMetas[i].propMeta);
                }
            }

            Runtime &_rt;
            const Options &_options;
            DecompileResult &_result;
            std::unordered_set<const void *> _visited{};
            u32 _cursor{};
        };

        const char *contextKind(const tjsbc::TjsContextType type) {
            switch(type) {
                case tjsbc::TjsContextType::TopLevel:
                    return "top-level";
                case tjsbc::TjsContextType::Function:
                    return "function";
                case tjsbc::TjsContextType::ExprFunction:
                    return "expr-function";
                case tjsbc::TjsContextType::Property:
                    return "property";
                case tjsbc::TjsContextType::PropertySetter:
                    return "property-setter";
                case tjsbc::TjsContextType::PropertyGetter:
                    return "property-getter";
                case tjsbc::TjsContextType::Class:
                    return "class";
                case tjsbc::TjsContextType::SuperClassGetter:
                    return "superclass-getter";
            }
            return "unknown";
        }
    } // namespace

    std::string DecompileResult::text() const {
        std::string out;
        for(const auto &unit : units)
            out += unit.text;
        return out;
    }

    namespace {
        const char *statusName(const Status status) {
            switch(status) {
                case Status::Success:
                    return "success";
                case Status::Unsupported:
                    return "unsupported";
                case Status::InvalidInput:
                    return "invalid-input";
                case Status::InternalError:
                    return "internal-error";
            }
            return "unknown";
        }

        std::string jsonString(const std::string &text) {
            std::string out = "\"";
            for(const char c : text) {
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
                    default:
                        if(static_cast<unsigned char>(c) < 0x20)
                            out += fmt::format("\\u{:04x}", static_cast<unsigned>(c));
                        else
                            out += c;
                }
            }
            return out + "\"";
        }
    } // namespace

    std::string DecompileResult::mapJson() const {
        std::string out = fmt::format("{{\n  \"status\": \"{}\",\n  \"units\": [", statusName(status));
        for(size_t u = 0; u < units.size(); ++u) {
            const auto &unit = units[u];
            out += fmt::format("{}\n    {{\"name\": {}, \"kind\": {}, \"lines\": [", u ? "," : "",
                               jsonString(unit.name), jsonString(unit.kind));
            for(size_t i = 0; i < unit.lines.size(); ++i) {
                const auto &row = unit.lines[i];
                out += fmt::format("{}\n      {{\"textLine\": {}, \"tac\": [{}, {}], \"synthetic\": {}", i ? "," : "",
                                   row.textLine, row.tacStart, row.tacEnd, row.synthetic ? "true" : "false");
                if(row.tjsWordStart)
                    out += fmt::format(", \"tjsWord\": {}", *row.tjsWordStart);
                if(row.sourceOffset)
                    out += fmt::format(", \"srcOffset\": {}", *row.sourceOffset);
                if(row.sourceLine)
                    out += fmt::format(", \"srcLine\": {}", *row.sourceLine);
                out += "}";
            }
            out += unit.lines.empty() ? "]}" : "\n    ]}";
        }
        out += "\n  ],\n  \"diagnostics\": [";
        for(size_t i = 0; i < diagnostics.size(); ++i)
            out += fmt::format("{}\n    {}", i ? "," : "", jsonString(diagnostics[i]));
        out += diagnostics.empty() ? "]\n}\n" : "\n  ]\n}\n";
        return out;
    }

    DecompileResult decompileChunk(Runtime &rt, vm::Chunk &top, const Options &options) {
        DecompileResult result;
        ChunkTreeCollector collector{ rt, options, result };
        collector.header();
        collector.topLevel(top);
        // M0：无表达式/语句恢复，严格模式一律 Unsupported（诊断视图同样如实标注）
        result.status = Status::Unsupported;
        return result;
    }

    DecompileResult decompileChunk(Runtime &rt, const vm::Chunk &top, const Options &options) {
        // 模块对 Chunk 只读（常量遍历与指令转储）；非 const 入口存在的唯一原因是
        // Chunk 的容器访问器缺 const 重载
        return decompileChunk(rt, const_cast<vm::Chunk &>(top), options);
    }

    DecompileResult decompileProgram(Runtime &rt, const tjsbc::TjsTranslatedProgram &program, const Options &options) {
        DecompileResult result;
        u32 cursor = 0;
        {
            auto &unit = result.units.emplace_back();
            unit.name = "<header>";
            unit.kind = "header";
            UnitBuilder builder{ unit, cursor };
            builder.synthetic("// cial decompile (M0): disassembly view of TJS2 bytecode, not recompilable");
        }
        for(size_t ci = 0; ci < program.contexts.size(); ++ci) {
            const auto &ctx = program.contexts[ci];
            auto &unit = result.units.emplace_back();
            unit.kind = contextKind(ctx.type);
            unit.name = displayName(rt, ctx.nameAtom,
                                    ci == static_cast<size_t>(program.topLevel) ? "<top-level>" : "<anonymous>");
            UnitBuilder builder{ unit, cursor };
            builder.synthetic(fmt::format("// ==== context {} {} {} (parent {}, arity {}) ====", ci, unit.kind,
                                          unit.name, ctx.parent, ctx.arity));
            if(ctx.isClass && ctx.classMeta) {
                // ctClass 加载期解释、无 TAC 体：只出壳（extends/成员形态）
                std::string extends;
                for(const auto base : ctx.classMeta->extends)
                    extends += fmt::format("{}{}", extends.empty() ? "" : ", ", displayName(rt, base, "?"));
                if(!extends.empty())
                    builder.synthetic(fmt::format("//   extends {}", extends));
                for(const auto &shape : ctx.classMeta->memberShapeMetas)
                    builder.synthetic(fmt::format("//   {}{} {}", shape.isStatic ? "static " : "",
                                                  shape.isMethod     ? "method"
                                                      : shape.isProp ? "property"
                                                                     : "var",
                                                  displayName(rt, shape.name, "?")));
            }
            if(ctx.funcMeta)
                emitLocals(builder, rt, *ctx.funcMeta);
            if(ctx.chunk)
                emitBody(builder, *ctx.chunk, &ctx.origin, options.originalSource);
            markUnsupported(result, unit);
        }
        result.status = Status::Unsupported;
        return result;
    }
} // namespace cial::decompile
