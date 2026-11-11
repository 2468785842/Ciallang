/*
 * Copyright (c) 2024/5/29 下午5:45
 *
 * /\  _` \   __          /\_ \  /\_ \
 * \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
 *  \ \ \/_/_\/\ \  /'__`\  \ \ \  \ \ \   /'__`\  /' _ `\  /'_ `\
 *   \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
 *    \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
 *     \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
 *                                                            /\____/
 *                                                            \_/__/
 *
 */
#include "DebugSession.hpp"

#include <algorithm>
#include <charconv>
#include <iostream>

#include "common/SourceFile.hpp"
#include "fmt/format.h"
#include "types/Array.hpp"
#include "types/Dictionary.hpp"
#include "types/TypeId.hpp"
#include "vm/Chunk.hpp"

namespace cial::dbg {
    namespace {
        constexpr int ListContext = 5; // list 窗口上下各取的行数
        constexpr int DisasmBefore = 4; // disasm 当前指令前展示条数
        constexpr int DisasmAfter = 8; // disasm 当前指令后展示条数
        constexpr int PrintDepthLimit = 3; // 值打印递归深度上限

        std::string atomText(const Runtime &rt, const Atom atom) {
            if(atom == ATOM_INVALID)
                return {};
            const auto *entry = rt.atomTable.get(atom);
            return entry && entry->str && entry->str->length() ? entry->str->toStdStr() : std::string{};
        }

        const char *objectTypeName(const TypeId id) {
            switch(id) {
                case TypeId::Array:
                    return "array";
                case TypeId::Dictionary:
                    return "dictionary";
                case TypeId::RegExp:
                    return "regexp";
                default:
                    return "object";
            }
        }

        std::string_view trim(const std::string_view text) {
            const auto isSpace = [](const char ch) { return ch == ' ' || ch == '\t' || ch == '\r'; };
            auto begin = text.begin();
            while(begin != text.end() && isSpace(*begin))
                ++begin;
            auto end = text.end();
            while(end != begin && isSpace(*(end - 1)))
                --end;
            // MSVC STL 的 const_iterator 非指针，(iterator, count) 构造不可用，按下标截取
            return text.substr(static_cast<size_t>(begin - text.begin()), static_cast<size_t>(end - begin));
        }

        // 首个空白分隔的 token 与剩余参数
        std::pair<std::string_view, std::string_view> splitCommand(const std::string_view text) {
            const auto trimmed = trim(text);
            if(trimmed.empty())
                return {};
            const auto pos = trimmed.find_first_of(" \t");
            if(pos == std::string_view::npos)
                return { trimmed, {} };
            return { trimmed.substr(0, pos), trim(trimmed.substr(pos + 1)) };
        }

        std::optional<int> parseInt(const std::string_view text) {
            if(text.empty())
                return std::nullopt;
            int value{};
            const auto *begin = text.data();
            const auto *end = text.data() + text.size();
            const auto [ptr, ec] = std::from_chars(begin, end, value);
            if(ec != std::errc{} || ptr != end)
                return std::nullopt;
            return value;
        }
    } // namespace

    DebugSession::DebugSession(vm::VMState &state, vm::Chunk &entryChunk) : _state(state), _entryChunk(entryChunk) {
        // 立即编码全部 Chunk（幂等）：rlc 条目只有在 toBytecode() 后才是 bc 偏移，
        // 断点解析与反汇编都依赖该空间
        for(auto *chunk : collectChunks())
            chunk->toBytecode();
    }

    bool DebugSession::initialPrompt() {
        fmt::println("cial-dbg: loaded {} (use `run` to start, `help` for commands)", _entryChunk.sourceName());
        enterPrompt(*_state.curFrame(), 0); // main 已 allocCallFrame：入口帧就绪，展示首行源码
        return _mode != Mode::Initial;
    }

    void DebugSession::onInstruction(vm::VMState &state, CallFrame &frame, const u64 pc) {
        switch(_mode) {
            case Mode::Initial:
                return;
            case Mode::Running:
                if(!hitBreakpoint(frame, pc))
                    return;
                break;
            case Mode::Step: {
                const auto line = frame.chunk->lineAtBc(pc);
                // 无行号条目的中间指令与基线同语句，不停
                if(!line || line->start().line + 1 == _baselineLine)
                    return;
                break;
            }
            case Mode::Next:
                if(state.context.stackTop() > _baselineDepth)
                    return;
                {
                    const auto line = frame.chunk->lineAtBc(pc);
                    if(!line || line->start().line + 1 == _baselineLine)
                        return;
                }
                break;
            case Mode::Finish:
                if(state.context.stackTop() >= _baselineDepth)
                    return;
                break;
        }
        enterPrompt(frame, pc);
    }

    void DebugSession::onDebugger(vm::VMState &state, CallFrame &frame, const u64 pc) {
        fmt::println("[debugger statement]");
        // pc 已越过操作码：回退到 debugger 指令起点，横幅指向语句所在行
        const auto start = frame.chunk->instructionBefore(pc).value_or(pc);
        enterPrompt(frame, start);
    }

    void DebugSession::enterPrompt(CallFrame &frame, const u64 pc) {
        _mode = Mode::Initial; // 停止期间复用 Initial：不判定断点
        _baselineDepth = _state.context.stackTop();
        if(const auto line = frame.chunk->lineAtBc(pc))
            _baselineLine = static_cast<int>(line->start().line) + 1;
        else
            _baselineLine = 0;
        _selectedFrame = static_cast<int>(_baselineDepth) - 1;
        if(_selectedFrame < 0)
            _selectedFrame = 0;
        printStopBanner(frame, pc);
        while(true) {
            fmt::print("cial-dbg> ");
            std::string input;
            if(!std::getline(std::cin, input)) {
                fmt::println("");
                fmt::println("cial-dbg: eof, quitting");
                throw vm::DebugAbort{};
            }
            const auto text = trim(input);
            const auto command = text.empty() ? std::string_view{ _lastCommand } : text;
            if(command.empty())
                continue;
            _lastCommand = std::string{ command };
            if(dispatch(std::string{ command }))
                break;
        }
    }

    bool DebugSession::dispatch(const std::string &input) {
        const auto [cmd, args] = splitCommand(input);
        if(cmd == "run" || cmd == "r" || cmd == "continue" || cmd == "c") {
            _mode = Mode::Running;
            return true;
        }
        if(cmd == "step" || cmd == "s") {
            _mode = Mode::Step;
            return true;
        }
        if(cmd == "next" || cmd == "n") {
            _mode = Mode::Next;
            return true;
        }
        if(cmd == "finish") {
            if(_state.context.stackTop() <= 1) {
                fmt::println("already at outermost frame; use continue");
                return false;
            }
            _mode = Mode::Finish;
            return true;
        }
        if(cmd == "quit" || cmd == "q")
            throw vm::DebugAbort{};
        if(cmd == "break" || cmd == "b") {
            cmdBreak(std::string{ args });
            return false;
        }
        if(cmd == "delete" || cmd == "d") {
            cmdDelete(std::string{ args });
            return false;
        }
        if(cmd == "bt" || cmd == "backtrace") {
            cmdBacktrace();
            return false;
        }
        if(cmd == "frame") {
            cmdFrame(std::string{ args });
            return false;
        }
        if(cmd == "locals") {
            cmdLocals();
            return false;
        }
        if(cmd == "print" || cmd == "p") {
            cmdPrint(std::string{ args });
            return false;
        }
        if(cmd == "list" || cmd == "l") {
            cmdList(std::string{ args });
            return false;
        }
        if(cmd == "disasm" || cmd == "dis") {
            cmdDisasm(std::string{ args });
            return false;
        }
        if(cmd == "help" || cmd == "h") {
            cmdHelp();
            return false;
        }
        fmt::println("unknown command `{}` (help lists commands)", cmd);
        return false;
    }

    void DebugSession::printStopBanner(CallFrame &frame, const u64 pc) const {
        auto *chunk = frame.chunk;
        const auto line = chunk->lineAtBc(pc);
        const int displayLine = line ? static_cast<int>(line->start().line) + 1 : 0;
        const auto *meta = frame.funcMeta;
        const auto function = meta ? atomText(_state.rt, meta->name) : "<script>";
        fmt::println("Stopped at {}:{} ({})", chunk->sourceName(), displayLine, function);
        if(displayLine)
            listSourceLine(*chunk, displayLine, "=>");
    }

    void DebugSession::cmdBreak(const std::string &args) {
        const auto line = parseInt(trim(args));
        if(!line || *line < 1) {
            fmt::println("usage: break <line>");
            return;
        }
        Breakpoint bp{ .number = _nextBreakpointNumber++, .requestLine = *line };
        resolveBreakpoint(bp);
        if(bp.line == 0) {
            fmt::println("breakpoint {}: no code at or after line {}", bp.number, bp.requestLine);
            return;
        }
        fmt::println("Breakpoint {} at {}:{} ({} location{})", bp.number, _entryChunk.sourceName(), bp.line,
                     bp.sites.size(), bp.sites.size() == 1 ? "" : "s");
        _breakpoints.push_back(std::move(bp));
    }

    void DebugSession::cmdDelete(const std::string &args) {
        const auto trimmed = trim(args);
        if(trimmed.empty()) {
            if(_breakpoints.empty()) {
                fmt::println("no breakpoints");
                return;
            }
            _breakpoints.clear();
            fmt::println("deleted all breakpoints");
            return;
        }
        const auto number = parseInt(trimmed);
        if(!number) {
            fmt::println("usage: delete [<breakpoint number>]");
            return;
        }
        const auto it = std::ranges::find_if(_breakpoints, [&](const Breakpoint &bp) { return bp.number == *number; });
        if(it == _breakpoints.end()) {
            fmt::println("no breakpoint {}", *number);
            return;
        }
        _breakpoints.erase(it);
        fmt::println("deleted breakpoint {}", *number);
    }

    std::vector<vm::Chunk *> DebugSession::collectChunks() const {
        std::vector<vm::Chunk *> chunks{ &_entryChunk };
        std::set<const vm::Chunk *> visited{ &_entryChunk };
        // 沿常量表收集函数 Chunk（FuncMeta::chunk），仿 cli::dumpGraphs 的遍历；
        // 类成员函数同样挂在常量 FuncMeta 上，ClassMeta 无独立代码 Chunk
        for(size_t i = 0; i < chunks.size(); ++i) {
            for(const auto &constant : chunks[i]->getConstants()) {
                if(constant.type() != ConstantType::FuncMeta)
                    continue;
                auto *meta = constant.value<FuncMeta *>();
                if(!meta || !meta->chunk || visited.contains(meta->chunk))
                    continue;
                visited.insert(meta->chunk);
                chunks.push_back(meta->chunk);
            }
        }
        return chunks;
    }

    void DebugSession::resolveBreakpoint(Breakpoint &bp) const {
        const auto chunks = collectChunks();
        const int target = bp.requestLine - 1; // Rlc 行列 0 基
        int snapped = -1;
        for(auto *chunk : chunks) {
            for(const auto &[offset, loc] : chunk->rlc().rawEntries()) {
                const auto entryLine = static_cast<int>(loc.start().line);
                if(entryLine < target)
                    continue;
                if(snapped < 0 || entryLine < snapped)
                    snapped = entryLine;
            }
        }
        // 吸附窗口限 +3 行：函数体内无 AST 位置的语句（如 return，Parser 既有
        // 缺口）不产生行表条目，不设界会跨函数吸到远处的顶层行
        if(snapped < 0 || snapped > target + 3) {
            bp.line = 0;
            return;
        }
        bp.line = snapped + 1;
        for(auto *chunk : chunks) {
            for(const auto &[offset, loc] : chunk->rlc().rawEntries()) {
                if(static_cast<int>(loc.start().line) == snapped)
                    bp.sites.emplace_back(chunk, offset);
            }
        }
    }

    bool DebugSession::hitBreakpoint(CallFrame &frame, const u64 pc) const {
        return std::ranges::any_of(_breakpoints, [&](const Breakpoint &bp) {
            return std::ranges::any_of(
                bp.sites, [&](const auto &site) { return site.first == frame.chunk && site.second == pc; });
        });
    }

    CallFrame *DebugSession::selectedFrame() const {
        const auto depth = _state.context.stackTop();
        if(depth == 0)
            return nullptr;
        const auto index = std::clamp(_selectedFrame, 0, static_cast<int>(depth) - 1);
        return _state.context.callStack() + index;
    }

    void DebugSession::cmdBacktrace() const {
        const auto depth = _state.context.stackTop();
        if(depth == 0) {
            fmt::println("(no frames)");
            return;
        }
        for(auto i = static_cast<int>(depth) - 1; i >= 0; --i) {
            const auto &frame = _state.context.callStack()[static_cast<size_t>(i)];
            const auto *meta = frame.funcMeta;
            const auto function = meta ? atomText(_state.rt, meta->name) : "<script>";
            const auto line = frame.chunk->lineAtBc(frame.pc);
            const auto displayLine = line ? static_cast<int>(line->start().line) + 1 : 0;
            fmt::println("#{}{} {} at {}:{}", static_cast<int>(depth) - 1 - i, i == _selectedFrame ? " *" : "  ",
                         function, frame.chunk->sourceName().empty() ? "?" : frame.chunk->sourceName(), displayLine);
        }
    }

    void DebugSession::cmdFrame(const std::string &args) {
        // bt 编号惯例：#0 = 最新帧（栈顶），n 越大越靠外层
        const auto number = parseInt(trim(args));
        const auto depth = static_cast<int>(_state.context.stackTop());
        if(!number || *number < 0 || *number >= depth) {
            fmt::println("usage: frame <0-{}>", depth - 1);
            return;
        }
        _selectedFrame = depth - 1 - *number;
        const auto &frame = _state.context.callStack()[static_cast<size_t>(_selectedFrame)];
        const auto *meta = frame.funcMeta;
        const auto line = frame.chunk->lineAtBc(frame.pc);
        fmt::println("#{} {} at {}:{}", *number, meta ? atomText(_state.rt, meta->name) : "<script>",
                     frame.chunk->sourceName(), line ? static_cast<int>(line->start().line) + 1 : 0);
    }

    void DebugSession::cmdLocals() const {
        const auto *frame = selectedFrame();
        if(!frame || !frame->funcMeta) {
            fmt::println("(no locals; top-level variables are globals)");
            return;
        }
        bool any = false;
        for(const auto &local : frame->funcMeta->localVars) {
            const auto name = atomText(_state.rt, local.identifier);
            if(name.empty())
                continue;
            fmt::print("  {} = ", name);
            printValue(frame->getReg(local.reg.index()));
            any = true;
        }
        if(!any)
            fmt::println("(no locals)");
    }

    void DebugSession::cmdPrint(const std::string &args) const {
        const auto expr = trim(args);
        if(expr.empty()) {
            fmt::println("usage: print <name|%register>");
            return;
        }
        const auto *frame = selectedFrame();
        if(expr.starts_with('%')) {
            const auto reg = parseInt(expr.substr(1));
            if(!reg || !frame) {
                fmt::println("invalid register `{}`", expr);
                return;
            }
            fmt::print("{} = ", expr);
            printValue(frame->getReg(static_cast<u16>(*reg)));
            return;
        }
        const std::string name{ expr };
        if(frame && frame->funcMeta) {
            for(const auto &local : frame->funcMeta->localVars) {
                if(atomText(_state.rt, local.identifier) == name) {
                    fmt::print("{} = ", name);
                    printValue(frame->getReg(local.reg.index()));
                    return;
                }
            }
        }
        if(_state.globalHas(name)) {
            fmt::print("{} = ", name);
            printValue(_state.global(name));
            return;
        }
        fmt::println("no local or global `{}`", name);
    }

    void DebugSession::cmdList(const std::string &args) const {
        const auto *frame = selectedFrame();
        if(!frame) {
            fmt::println("(no frames)");
            return;
        }
        auto *chunk = frame->chunk;
        const auto snapshot = chunk->sourceSnapshot();
        if(!snapshot) {
            fmt::println("no source available for {}", chunk->sourceName());
            return;
        }
        const auto current = chunk->lineAtBc(frame->pc);
        auto center = current ? static_cast<int>(current->start().line) + 1 : 1;
        if(const auto line = parseInt(trim(args)))
            center = *line;
        const auto total = static_cast<int>(snapshot->numberOfLines());
        const int begin = std::clamp(center - ListContext, 1, total);
        const int end = std::clamp(center + ListContext, 1, total);
        for(int line = begin; line <= end; ++line)
            listSourceLine(*chunk, line, line == center ? "=>" : "  ");
    }

    void DebugSession::cmdDisasm(const std::string &args) const {
        const auto *frame = selectedFrame();
        if(!frame) {
            fmt::println("(no frames)");
            return;
        }
        auto *chunk = frame->chunk;
        chunk->toBytecode();
        const auto index = chunk->tacIndexAtBc(frame->pc);
        if(!index) {
            fmt::println("chunk not encoded yet");
            return;
        }
        const auto requested = parseInt(trim(args)).value_or(DisasmAfter);
        const auto total = static_cast<int>(chunk->instructionCount());
        const int begin = std::clamp(static_cast<int>(*index) - DisasmBefore, 0, total);
        const int end = std::clamp(static_cast<int>(*index) + requested + 1, begin, total);
        // 寄存器值注释只对当前停止帧有意义（VMState::reg 读当前帧）
        const bool annotate = frame == _state.curFrame();
        for(int i = begin; i < end; ++i) {
            const auto bcOffset = chunk->bcOffsetAtTac(static_cast<size_t>(i));
            const auto line = chunk->lineAtBc(bcOffset);
            fmt::print("{} {:>5} [{}] ", i == *index ? "=>" : "  ", bcOffset,
                       line ? fmt::format("{}", static_cast<int>(line->start().line) + 1) : std::string{ "-" });
            fmt::println("{}", chunk->inst(static_cast<size_t>(i))->dump(annotate ? &_state : nullptr));
        }
    }

    void DebugSession::cmdHelp() const {
        fmt::println("commands: run|continue  step  next  finish  break <line>  delete [<n>]");
        fmt::println("          bt  frame <n>  locals  print <name|%reg>  list [line]  disasm [n]");
        fmt::println("          quit  help");
    }

    void DebugSession::listSourceLine(vm::Chunk &chunk, const int line, const char *marker) const {
        const auto snapshot = chunk.sourceSnapshot();
        if(!snapshot)
            return;
        const auto *lineText = snapshot->lineByNumber(static_cast<size_t>(line - 1));
        if(!lineText)
            return;
        const auto width = std::max(size_t{ 1 }, std::to_string(snapshot->numberOfLines()).size());
        fmt::println("{} {:>{}} | {}", marker, line, width, snapshot->substring(lineText->begin, lineText->end));
    }

    void DebugSession::printValue(const Value &value) const {
        std::string out;
        appendValue(out, value, 0, {});
        fmt::println("{}", out);
    }

    void DebugSession::appendValue(std::string &out, const Value &value, const int depth,
                                   const std::set<const void *> &seen) const {
        if(!value.isObject()) {
            fmt::format_to(std::back_inserter(out), "{}", value);
            return;
        }
        const auto object = value.asObject();
        if(object.isFailed() || !object.value()) {
            out += "null";
            return;
        }
        auto *obj = object.value();
        if(depth >= PrintDepthLimit) {
            out += "...";
            return;
        }
        if(seen.contains(obj)) {
            out += "<cycle>";
            return;
        }
        if(const auto *array = dynamic_cast<const Array *>(obj)) {
            fmt::format_to(std::back_inserter(out), "array({}) [", array->elements.size());
            std::set<const void *> nested{ seen };
            nested.insert(obj);
            for(size_t i = 0; i < array->elements.size(); ++i) {
                if(i)
                    out += ", ";
                appendValue(out, array->elements[i], depth + 1, nested);
            }
            out += "]";
            return;
        }
        if(const auto *dict = dynamic_cast<const Dictionary *>(obj)) {
            fmt::format_to(std::back_inserter(out), "dictionary({}) {{", dict->members.size());
            std::set<const void *> nested{ seen };
            nested.insert(obj);
            bool first = true;
            for(const auto &[atom, member] : dict->members) {
                if(!first)
                    out += ", ";
                first = false;
                fmt::format_to(std::back_inserter(out), "{}: ", atomText(_state.rt, atom));
                appendValue(out, member, depth + 1, nested);
            }
            out += "}";
            return;
        }
        const auto name = atomText(_state.rt, obj->getName());
        fmt::format_to(std::back_inserter(out), "<{}{}>", objectTypeName(obj->typeId()),
                       name.empty() ? "" : " " + name);
    }
} // namespace cial::dbg
