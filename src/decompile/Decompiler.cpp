/*
 * Copyright (c) 2026/11/1
 *
 * 字节码反编译器实现（M0 {文本, 行表} 管道；M4 壳组装：context 树 → function/
 * class/property 声明文本装配）。
 * 见 Decompiler.hpp 与 doc/bytecode/decompiler-plan.md。
 *
 * M4 壳组装（方案 §5.5）：ExprBuilder 遇到 Load 声明常量 + 安装链时回调
 * DeclHost，ShellAssembler 按壳元数据（FuncMeta/ClassMeta/PropMeta + localVars
 * 参数表）装配声明块——两条入口共用装配器，函数名解析各异（自产 = FuncMeta
 * 名；bc = context 反查）。嵌套体的行表记入子单元（各自 Chunk 的 TAC 坐标），
 * 文本内联在父单元（子单元 text 为空），text() 拼接即完整源码。
 */

#include "Decompiler.hpp"

#include <algorithm>
#include <iterator>
#include <unordered_set>

#include <fmt/format.h>

#include "ExprBuilder.hpp"
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

        // 逐行遍历语句文本（多行声明块拆行）
        template <typename F>
        void forEachTextLine(const std::string &text, F &&callback) {
            size_t start = 0;
            while(true) {
                const auto nl = text.find('\n', start);
                callback(text.substr(start, nl == std::string::npos ? std::string::npos : nl - start));
                if(nl == std::string::npos)
                    break;
                start = nl + 1;
            }
        }

        std::string indentText(const u32 level) { return std::string(level * 4, ' '); }

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

        // 体的反汇编级视图（定义见下；emitUnitBody 先用）
        void emitBody(UnitBuilder &builder, vm::Chunk &chunk, const Vec<tjsbc::TacOrigin> *origin,
                      const Common::SourceFile *source);

        // M2 表达式恢复发射。严格模式（annotateOnly=false）：恢复语句为主体，
        // 行表区间按游标闭合、末语句延到 Chunk 末尾以覆盖被内联/丢弃的 TAC；
        // 多行语句（M4 声明块）拆行——首行携带 TAC 区间，其余行为 synthetic。
        // 视图模式（annotateOnly=true）：反汇编为主体，恢复结果以 // >> 注释附加。
        // 失败记录逐单元诊断并返回 false（严格模式由调用方回退反汇编视图）
        bool emitRecovered(UnitBuilder &builder, Runtime &rt, vm::Chunk &chunk, const FuncMeta *meta,
                           const bool functionLike, DeclHost *host, DecompileResult &result, DecompiledUnit &unit,
                           const bool annotateOnly) {
            auto recovery = recoverUnitBody(rt, chunk, meta, functionLike, host);
            if(!recovery.unsupported.empty()) {
                result.diagnostics.push_back(fmt::format("{} '{}': {}", unit.kind, unit.name, recovery.unsupported));
                if(annotateOnly)
                    builder.synthetic(fmt::format("//   >> unsupported: {}", recovery.unsupported));
                return false;
            }
            const auto count = chunk.getInstVec().size();
            if(annotateOnly) {
                for(const auto &statement : recovery.statements)
                    forEachTextLine(statement.text, [&](const std::string &lineText) {
                        builder.synthetic(fmt::format("//   >> {}", lineText));
                    });
                return true;
            }
            u32 cursor = 0;
            for(const auto &statement : recovery.statements) {
                LineMapRow row{ .tacStart = cursor,
                                .tacEnd = statement.tacEnd != 0 ? statement.tacEnd : statement.pc + 1 };
                bool firstLine = true;
                forEachTextLine(statement.text, [&](const std::string &lineText) {
                    if(firstLine) {
                        builder.line(lineText, row);
                        firstLine = false;
                    } else {
                        builder.synthetic(lineText);
                    }
                });
                cursor = row.tacEnd;
            }
            if(!recovery.statements.empty()) {
                if(cursor < count && !unit.lines.empty())
                    unit.lines.back().tacEnd = static_cast<u32>(count);
            } else if(count > 0) {
                builder.synthetic(fmt::format("//   (all {} instructions dropped as dead code)", count));
            }
            return true;
        }

        // 单元体发射：视图模式 = 反汇编 + 恢复注释；严格模式 = 恢复语句，失败回退
        // 反汇编。host 为声明装配宿主（M4），无壳上下文的调用传 nullptr
        void emitUnitBody(UnitBuilder &builder, const Options &options, Runtime &rt, vm::Chunk &chunk,
                          const FuncMeta *meta, const bool functionLike, DecompileResult &result, DecompiledUnit &unit,
                          DeclHost *host) {
            if(options.diagnosticView) {
                emitBody(builder, chunk, nullptr, nullptr);
                emitRecovered(builder, rt, chunk, meta, functionLike, host, result, unit, true);
            } else if(!emitRecovered(builder, rt, chunk, meta, functionLike, host, result, unit, false)) {
                emitBody(builder, chunk, nullptr, nullptr);
            }
        }

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

        // ── M4 壳组装器（方案 §5.5）：DeclHost 实现，两条入口共用 ──
        // 声明文本在渲染期装配（ExprBuilder 的 Decl tile 回调）：文本行号基准取自
        // 共享游标（顶层）或装配帧栈（嵌套声明在父声明文本构建中途回调），子单元
        // 行表按最终文本位置登记（各自 Chunk 的 TAC 坐标），文本内联父单元（子单
        // 元 text 为空，text() 拼接即完整源码）。bc 路径 FuncMeta 名/类名由转译器
        // 设置（TjsTranslator Phase B：meta->name = context 名），装配逻辑同构。
        class ShellAssembler : public DeclHost {
        public:
            // program/originalSource 为 bc 路径可选上下文：前者反查体的码字来源表
            // （M1 行表列透传），后者做偏移→行号换算；自产路径传空
            ShellAssembler(Runtime &rt, DecompileResult &result, u32 &cursor, std::unordered_set<const void *> &visited,
                           const tjsbc::TjsTranslatedProgram *program = nullptr,
                           const Common::SourceFile *originalSource = nullptr) :
                _rt(rt), _result(result), _cursor(cursor), _visited(visited), _program(program),
                _originalSource(originalSource) {
                if(program != nullptr)
                    for(size_t i = 0; i < program->contexts.size(); ++i)
                        if(program->contexts[i].funcMeta != nullptr)
                            _originByFunc.emplace(program->contexts[i].funcMeta, i);
            }

            std::string declarationText(const Constant &constant, const DeclForm form, const std::string &literalName,
                                        const u32 indent, std::string &reason) override {
                // 行号基准：嵌套声明取在途帧（父声明文本构建中途），同级声明取
                // 共享游标与上一声明块尾的较大者（父单元行表在渲染后才落游标，
                // 同单元多个声明的渲染期间游标不前进）
                const u32 base =
                    _frames.empty() ? std::max(_cursor, _declTailEnd) : _frames.back().base + _frames.back().count;
                _frames.push_back(Frame{ base, 0 });
                std::string out;
                const auto writer = [&out, this](const std::string &line) {
                    out += line;
                    out += '\n';
                    ++_frames.back().count;
                };
                switch(constant.type()) {
                    case ConstantType::FuncMeta:
                        writeFunction(*constant.value<FuncMeta *>(), form, literalName, indent, writer, reason);
                        break;
                    case ConstantType::ClassMeta:
                        writeClass(*constant.value<ClassMeta *>(), literalName, indent, writer, reason);
                        break;
                    case ConstantType::PropMeta:
                        writeProperty(*constant.value<PropMeta *>(), literalName, indent, writer, reason);
                        break;
                    default:
                        reason = "声明常量类型未知";
                        break;
                }
                const Frame done = _frames.back();
                _frames.pop_back();
                _declTailEnd = done.base + done.count;
                // writer 逐行补 \n：剥掉块尾换行，交还调用方拆行
                if(!out.empty() && out.back() == '\n')
                    out.pop_back();
                return out;
            }

        protected:
            struct Frame {
                u32 base{};
                u32 count{};
            };
            using Writer = std::function<void(const std::string &)>;

            struct FieldInit {
                std::string text; // "x = 10"（无分号）
                u32 tacStart{}, tacEnd{};
            };

            [[nodiscard]] std::string functionName(const FuncMeta &meta) const {
                return displayName(_rt, meta.name, {});
            }

            // 参数表：localVars 中 reg < arity 的具名项按寄存器序；缺名合成 a{i}
            // （bc 路径无局部名表，恒为合成名）；collapse 参数（TJS f(a, b*)）末位补 *
            [[nodiscard]] std::string parameterList(const FuncMeta &meta) const {
                Vec<std::pair<u16, std::string>> named;
                for(const auto &local : meta.localVars)
                    if(local.reg.index() < meta.arity && local.identifier != ATOM_INVALID)
                        named.push_back({ local.reg.index(), atomName(_rt, local.identifier) });
                std::sort(named.begin(), named.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
                std::string out;
                for(u16 i = 0; i < static_cast<u16>(meta.arity); ++i) {
                    std::string name;
                    for(const auto &[index, text] : named)
                        if(index == i) {
                            name = text;
                            break;
                        }
                    if(name.empty())
                        name = fmt::format("a{}", i);
                    out += fmt::format("{}{}", i ? ", " : "", name);
                }
                if(meta.hasCollapse)
                    out += '*';
                return out;
            }

            void writeFunction(const FuncMeta &meta, const DeclForm form, const std::string &literalName,
                               const u32 indent, const Writer &writer, std::string &reason,
                               const std::string &firstPad = {}) {
                if(!_visited.insert(&meta).second) {
                    reason = "重复声明的函数常量";
                    return;
                }
                if(!meta.chunk) {
                    reason = "函数常量缺少体";
                    return;
                }
                const auto params = parameterList(meta);
                const auto pad = indentText(indent);
                // 赋值形态恒为匿名字面量（具名函数表达式不参与恢复：转译与重解析
                // 的名字绑定差异未验证）
                // 缩进为相对量：首行由调用方决定（声明 tile 顶格 bare，类/property
                // 内嵌成员带成员 pad），写入期再叠加体 pad；内层行保留相对 pad
                if(form == DeclForm::Assign)
                    writer(fmt::format("{}function ({}) {{", firstPad, params));
                else
                    writer(fmt::format("{}function {}({}) {{", firstPad, literalName, params));
                writeBody(*meta.chunk, &meta, indent + 1, writer, reason);
                if(reason.empty())
                    writer(fmt::format("{}}}", pad));
            }

            void writeAccessor(const FuncMeta &meta, const char *kind, const u32 indent, const Writer &writer,
                               std::string &reason, const std::string &firstPad = {}) {
                if(!_visited.insert(&meta).second) {
                    reason = "重复声明的函数常量";
                    return;
                }
                if(!meta.chunk) {
                    reason = "property 访问器缺少体";
                    return;
                }
                const auto params = parameterList(meta);
                const auto pad = indentText(indent);
                if(params.empty())
                    writer(fmt::format("{}{} {{", firstPad, kind));
                else
                    writer(fmt::format("{}{}({}) {{", firstPad, kind, params));
                writeBody(*meta.chunk, &meta, indent + 1, writer, reason);
                if(reason.empty())
                    writer(fmt::format("{}}}", pad));
            }

            void writeProperty(const PropMeta &meta, const std::string &name, const u32 indent, const Writer &writer,
                               std::string &reason, const std::string &firstPad = {}) {
                if(!_visited.insert(&meta).second) {
                    reason = "重复声明的 property 常量";
                    return;
                }
                const auto pad = indentText(indent);
                writer(fmt::format("{}property {} {{", firstPad, name));
                const auto memberPad = indentText(indent + 1);
                if(meta.getFunc)
                    writeAccessor(*meta.getFunc, "getter", indent + 1, writer, reason, memberPad);
                if(reason.empty() && meta.setFunc)
                    writeAccessor(*meta.setFunc, "setter", indent + 1, writer, reason, memberPad);
                if(reason.empty())
                    writer(fmt::format("{}}}", pad));
            }

            void writeClass(const ClassMeta &meta, const std::string &name, const u32 indent, const Writer &writer,
                            std::string &reason, const std::string &firstPad = {}) {
                if(!_visited.insert(&meta).second) {
                    reason = "重复声明的类常量";
                    return;
                }
                const auto pad = indentText(indent);
                std::string header = fmt::format("{}class {}", firstPad, name);
                for(const auto base : meta.extends)
                    header += fmt::format("{}{}", header.find(" extends ") == std::string::npos ? " extends " : ", ",
                                          displayName(_rt, base, "?"));
                writer(header + " {");
                // 字段初始化（initDefaultVal 按 Atom 归集，渲染按成员声明序）
                Vec<std::pair<Atom, FieldInit>> inits;
                if(meta.initDefaultVal)
                    collectFieldInits(*meta.initDefaultVal, inits, reason);
                if(!reason.empty())
                    return;
                auto *initUnit = static_cast<DecompiledUnit *>(nullptr);
                if(!inits.empty()) {
                    initUnit = &_result.units.emplace_back();
                    initUnit->name = displayName(_rt, meta.className, "<anonymous>");
                    initUnit->kind = "class-field-init";
                }
                const auto memberPad = indentText(indent + 1);
                // 构造器在前（成员表不含构造器；new 时按名调用）
                if(meta.constructor) {
                    auto ctorName = functionName(*meta.constructor);
                    if(ctorName.empty())
                        ctorName = name;
                    writeFunction(*meta.constructor, DeclForm::Bare, ctorName, indent + 1, writer, reason, memberPad);
                    if(!reason.empty())
                        return;
                }
                for(size_t i = 0; i < meta.memberShapeMetas.size() && i < meta.memberMetas.size(); ++i) {
                    const auto &shape = meta.memberShapeMetas[i];
                    const auto staticPrefix = shape.isStatic ? "static " : "";
                    if(shape.isVar) {
                        const FieldInit *init = nullptr;
                        for(const auto &[atom, candidate] : inits)
                            if(atom == shape.name) {
                                init = &candidate;
                                break;
                            }
                        if(init != nullptr) {
                            writer(fmt::format("{}{}var {};", memberPad, staticPrefix, init->text));
                            initUnit->lines.push_back(LineMapRow{ .textLine = base() + _frames.back().count - 1,
                                                                  .tacStart = init->tacStart,
                                                                  .tacEnd = init->tacEnd });
                        } else {
                            writer(
                                fmt::format("{}{}var {};", memberPad, staticPrefix, displayName(_rt, shape.name, "?")));
                        }
                    } else if(shape.isMethod && meta.memberMetas[i].funcMeta) {
                        // 编译器生成的空 finalize：TJS2 自动提供，不输出
                        if(meta.memberMetas[i].funcMeta->syntheticFinalizer)
                            continue;
                        writeFunction(*meta.memberMetas[i].funcMeta, DeclForm::Bare, displayName(_rt, shape.name, "?"),
                                      indent + 1, writer, reason, memberPad);
                        if(!reason.empty())
                            return;
                    } else if(shape.isProp && meta.memberMetas[i].propMeta) {
                        writeProperty(*meta.memberMetas[i].propMeta, displayName(_rt, shape.name, "?"), indent + 1,
                                      writer, reason, memberPad);
                        if(!reason.empty())
                            return;
                    }
                }
                writer(fmt::format("{}}}", pad));
            }

            // 体装配：恢复语句写入父文本（行文本为相对缩进，此处叠加体 pad），行表记入
            // body 子单元（该 Chunk 的 TAC 坐标）
            void writeBody(vm::Chunk &chunk, const FuncMeta *meta, const u32 indent, const Writer &writer,
                           std::string &reason) {
                auto recovery = recoverUnitBody(_rt, chunk, meta, true, this);
                if(!recovery.unsupported.empty()) {
                    reason = recovery.unsupported;
                    return;
                }
                auto &unit = _result.units.emplace_back();
                unit.name = meta == nullptr ? std::string{ "<body>" } : displayName(_rt, meta->name, "<body>");
                unit.kind = "body";
                // bc 路径体的码字来源表（M1 列透传；自产路径为空）
                const Vec<tjsbc::TacOrigin> *origin = nullptr;
                if(_program != nullptr && meta != nullptr) {
                    const auto it = _originByFunc.find(meta);
                    if(it != _originByFunc.end() && it->second < _program->contexts.size()) {
                        origin = &_program->contexts[it->second].origin;
                    }
                }
                if(_frames.empty()) {
                    reason = "声明装配帧栈为空（内部错误）";
                    return;
                }
                // 体首行号 = 帧基准 + 帧内已写行（声明头行已先行写入）
                const u32 base = _frames.back().base + _frames.back().count;
                const auto pad = indentText(indent);
                const auto count = chunk.getInstVec().size();
                u32 covered = 0;
                for(size_t si = 0; si < recovery.statements.size(); ++si) {
                    const auto &statement = recovery.statements[si];
                    const u32 tacStart = statement.pc;
                    // 末语句区间延到 Chunk 末尾（覆盖被内联/丢弃的尾段），先于
                    // origin 分段——扩展区间内的来源条目同样登记
                    u32 tacEnd = statement.tacEnd != 0 ? statement.tacEnd : statement.pc + 1;
                    if(si + 1 == recovery.statements.size() && tacEnd < count)
                        tacEnd = count;
                    // 语句区间按 origin 条目分段（bc 路径）：一条语句跨多条来源条目
                    // 时各段独立成行表行、同 textLine——只取起点条目会丢失段内
                    // 行号变化（TJS2 outputdebug 仅在控制流点登记）。每段来源取
                    // ≤ 段起点的最后一项（继承语义与反汇编视图一致）。
                    // 自产路径 origin 为空，恒单段
                    Vec<u32> cuts{ tacStart };
                    if(origin != nullptr)
                        for(const auto &entry : *origin)
                            if(entry.tacIdx > tacStart && entry.tacIdx < tacEnd)
                                cuts.push_back(entry.tacIdx);
                    Vec<LineMapRow> headRows;
                    for(size_t s = 0; s < cuts.size(); ++s) {
                        const u32 segStart = cuts[s];
                        const u32 segEnd = s + 1 < cuts.size() ? cuts[s + 1] : tacEnd;
                        LineMapRow row{ .textLine = 0, .tacStart = segStart, .tacEnd = segEnd };
                        if(origin != nullptr && !origin->empty()) {
                            const auto oit = std::upper_bound(
                                origin->begin(), origin->end(), segStart,
                                [](const size_t value, const tjsbc::TacOrigin &entry) { return value < entry.tacIdx; });
                            if(oit != origin->begin()) {
                                const auto &entry = *std::prev(oit);
                                row.tjsWordStart = entry.tjsWord;
                                row.sourceOffset = entry.sourceOffset;
                                if(entry.sourceOffset && _originalSource != nullptr) {
                                    const auto location = _originalSource->locationByIndex(
                                        utf16UnitsToByteIndex(_originalSource->buffer(), *entry.sourceOffset));
                                    row.sourceLine = location.start().line;
                                }
                            }
                        }
                        headRows.push_back(row);
                    }
                    bool firstLine = true;
                    forEachTextLine(statement.text, [&](const std::string &lineText) {
                        if(firstLine) {
                            for(auto row : headRows) {
                                row.textLine = base + _frames.back().count;
                                unit.lines.push_back(row);
                            }
                        } else
                            unit.lines.push_back(
                                LineMapRow{ .textLine = base + _frames.back().count, .synthetic = true });
                        writer(pad + lineText);
                        firstLine = false;
                    });
                    covered = tacEnd;
                }
                if(recovery.statements.empty() && count > 0) {
                    writer(fmt::format("{}// (all {} instructions dropped as dead code)", pad, count));
                }
            }

            void collectFieldInits(FuncMeta &meta, Vec<std::pair<Atom, FieldInit>> &out, std::string &reason) {
                if(!meta.chunk) {
                    reason = "类缺少字段初始化体";
                    return;
                }
                auto recovery = recoverUnitBody(_rt, *meta.chunk, &meta, true, this);
                if(!recovery.unsupported.empty()) {
                    reason = recovery.unsupported;
                    return;
                }
                auto &code = meta.chunk->getInstVec();
                for(const auto &statement : recovery.statements) {
                    if(statement.text.find('\n') != std::string::npos) {
                        reason = "字段初始化含多行语句（函数字面量初始化器属后续批次）";
                        return;
                    }
                    // 仅取成员初始化 tile（DThis/DGlobal atom ← expr → "name = value"）
                    Atom atom = ATOM_INVALID;
                    if(statement.pc < code.size()) {
                        const auto op = code[statement.pc]->opcode();
                        if(op == inter::TacOpCode::DThis || op == inter::TacOpCode::DGlobal)
                            atom = code[statement.pc]->getOp1().value<Atom>();
                    }
                    if(atom == ATOM_INVALID)
                        continue;
                    auto text = statement.text;
                    if(!text.empty() && text.back() == ';')
                        text.pop_back();
                    out.push_back({ atom,
                                    FieldInit{ std::move(text), statement.pc,
                                               statement.tacEnd != 0 ? statement.tacEnd : statement.pc + 1 } });
                }
            }

            [[nodiscard]] u32 base() const { return _frames.back().base; }

            Runtime &_rt;
            DecompileResult &_result;
            u32 &_cursor;
            std::unordered_set<const void *> &_visited;
            const tjsbc::TjsTranslatedProgram *_program{};
            std::unordered_map<const FuncMeta *, size_t> _originByFunc;
            const Common::SourceFile *_originalSource;
            Vec<Frame> _frames;
            u32 _declTailEnd{}; // 最近一个完成声明块的尾行（同级声明行号基准）
        };

        // ── 自产路径：顶层体装配 + 常量图兜底收集 ──
        class ChunkTreeCollector final : public DeclHost {
        public:
            ChunkTreeCollector(Runtime &rt, const Options &options, DecompileResult &result) :
                _rt(rt), _options(options), _result(result), _assembler(rt, result, _cursor, _visited) {}

            void header() {
                auto &unit = _result.units.emplace_back();
                unit.name = "<header>";
                unit.kind = "header";
                UnitBuilder builder{ unit, _cursor };
                builder.synthetic(_options.diagnosticView
                                      ? "// cial decompile (diagnostic view): disassembly, not recompilable"
                                      : "// cial decompile: recovered source (TAC-level recovery, M4)");
                // 视图产物恒不承诺可重编译：以诊断记录，使状态公式统一为"无诊断即 Success"
                if(_options.diagnosticView)
                    _result.diagnostics.push_back(
                        "diagnostic view: disassembly annotated with recovery, not recompilable");
            }

            void topLevel(vm::Chunk &chunk) {
                auto &unit = _result.units.emplace_back();
                unit.name = "<top-level>";
                unit.kind = "top-level";
                UnitBuilder builder{ unit, _cursor };
                builder.synthetic("// ==== top-level ====");
                emitUnitBody(builder, _options, _rt, chunk, nullptr, false, _result, unit, this);
                walkConstants(chunk);
            }

            // DeclHost：体装配期经装配器回调（顶层游标为行号基准）
            std::string declarationText(const Constant &constant, const DeclForm form, const std::string &literalName,
                                        const u32 indent, std::string &reason) override {
                return _assembler.declarationText(constant, form, literalName, indent, reason);
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

            // 兜底：未经理器体装配（Decl tile）的常量独立成诊断单元。有效安装链
            // 已在体内装配并登记 visited，正常不达此处
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
                emitUnitBody(builder, _options, _rt, *meta.chunk, &meta, true, _result, unit, &_assembler);
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
            ShellAssembler _assembler;
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
        // 逐单元恢复：全部成功 → Success；任一失败 → Unsupported（诊断已逐单元记录，
        // 视图模式恒不含可重编译承诺，同样按诊断判定）
        result.status = result.diagnostics.empty() ? Status::Success : Status::Unsupported;
        return result;
    }

    DecompileResult decompileChunk(Runtime &rt, const vm::Chunk &top, const Options &options) {
        // 模块对 Chunk 只读（常量遍历与指令转储）；非 const 入口存在的唯一原因是
        // Chunk 的容器访问器缺 const 重载
        return decompileChunk(rt, const_cast<vm::Chunk &>(top), options);
    }

    namespace {
        // bc 路径声明引用扫描：各 context 的 Load 声明常量集合（INTER_OBJECT 回填
        // 与 context meta 同指针，TjsTranslator Phase B'），供壳装配去重
        void collectReferencedMetas(const tjsbc::TjsTranslatedProgram &program,
                                    std::unordered_set<const void *> &referenced) {
            for(const auto &ctx : program.contexts) {
                if(!ctx.chunk)
                    continue;
                auto &instructions = ctx.chunk->getInstVec();
                for(size_t pc = 0; pc < instructions.size(); ++pc) {
                    if(instructions[pc]->opcode() != inter::TacOpCode::Load)
                        continue;
                    const auto &constant =
                        ctx.chunk->getConstants()[instructions[pc]->getOp2().value<ConstIdx>().index()];
                    if(constant.type() == ConstantType::FuncMeta)
                        referenced.insert(constant.value<FuncMeta *>());
                    else if(constant.type() == ConstantType::ClassMeta)
                        referenced.insert(constant.value<ClassMeta *>());
                    else if(constant.type() == ConstantType::PropMeta)
                        referenced.insert(constant.value<PropMeta *>());
                }
            }
        }

        // property/class 常量的成员闭包在 decompileProgram 内联展开
        // （getter/setter/构造器/成员函数/字段初始化），判断 context 是否已被壳装配覆盖
    } // namespace

    DecompileResult decompileProgram(Runtime &rt, const tjsbc::TjsTranslatedProgram &program, const Options &options) {
        DecompileResult result;
        u32 cursor = 0;
        std::unordered_set<const void *> visited;
        std::unordered_set<const void *> referenced;
        collectReferencedMetas(program, referenced);
        // 成员闭包：property → getter/setter、class → 构造器/初始化/成员
        auto closure = referenced;
        bool changed = true;
        while(changed) {
            changed = false;
            for(const auto &ctx : program.contexts) {
                if(ctx.isProperty && ctx.propMeta && closure.contains(ctx.propMeta)) {
                    changed |= closure.insert(ctx.propMeta->getFunc).second;
                    changed |= closure.insert(ctx.propMeta->setFunc).second;
                }
                if(ctx.isClass && ctx.classMeta && closure.contains(ctx.classMeta)) {
                    changed |= closure.insert(ctx.classMeta->constructor).second;
                    changed |= closure.insert(ctx.classMeta->initDefaultVal).second;
                    for(const auto &member : ctx.classMeta->memberMetas) {
                        changed |= closure.insert(member.funcMeta).second;
                        changed |= closure.insert(member.propMeta).second;
                    }
                }
            }
        }
        ShellAssembler assembler{ rt, result, cursor, visited, &program, options.originalSource };
        {
            auto &unit = result.units.emplace_back();
            unit.name = "<header>";
            unit.kind = "header";
            UnitBuilder builder{ unit, cursor };
            builder.synthetic(
                options.diagnosticView
                    ? "// cial decompile (diagnostic view): disassembly of TJS2 bytecode, not recompilable"
                    : "// cial decompile: recovered source of TJS2 bytecode (TAC-level recovery, M4)");
            if(options.diagnosticView)
                result.diagnostics.push_back("diagnostic view: disassembly annotated with recovery, not recompilable");
        }
        // 声明装配（M4）：context 树的函数/类/property 均已在宿主 context 体的
        // Load+安装链处内联装配，不再独立成单元；未被引用的 context 兜底独立发射
        for(size_t ci = 0; ci < program.contexts.size(); ++ci) {
            const auto &ctx = program.contexts[ci];
            const bool isTop = ci == static_cast<size_t>(program.topLevel);
            const bool declared = (!ctx.isClass && !ctx.isProperty && ctx.funcMeta != nullptr &&
                                   (closure.contains(ctx.funcMeta) || visited.contains(ctx.funcMeta))) ||
                (ctx.isClass && ctx.classMeta != nullptr &&
                 (closure.contains(ctx.classMeta) || visited.contains(ctx.classMeta))) ||
                (ctx.isProperty && ctx.propMeta != nullptr &&
                 (closure.contains(ctx.propMeta) || visited.contains(ctx.propMeta)));
            if(!isTop && declared)
                continue;
            auto &unit = result.units.emplace_back();
            unit.kind = contextKind(ctx.type);
            unit.name = displayName(rt, ctx.nameAtom, isTop ? "<top-level>" : "<anonymous>");
            UnitBuilder builder{ unit, cursor };
            builder.synthetic(fmt::format("// ==== context {} {} {} (parent {}, arity {}) ====", ci, unit.kind,
                                          unit.name, ctx.parent, ctx.arity));
            if(ctx.isClass && ctx.classMeta && !isTop) {
                // 未被引用的类兜底：装配为裸声明（不产可执行安装），诊断不记失败
                std::string reason;
                auto text = assembler.declarationText(Constant{ ctx.classMeta }, DeclForm::Bare, unit.name, 0, reason);
                if(reason.empty())
                    forEachTextLine(text, [&](const std::string &lineText) { builder.synthetic(lineText); });
                else
                    builder.synthetic(fmt::format("//   class shell unavailable: {}", reason));
                continue;
            }
            if(ctx.funcMeta)
                emitLocals(builder, rt, *ctx.funcMeta);
            if(ctx.chunk) {
                if(options.diagnosticView) {
                    emitBody(builder, *ctx.chunk, &ctx.origin, options.originalSource);
                    emitRecovered(builder, rt, *ctx.chunk, ctx.funcMeta, ctx.isFunctionLike, &assembler, result, unit,
                                  true);
                } else if(!emitRecovered(builder, rt, *ctx.chunk, ctx.funcMeta, ctx.isFunctionLike, &assembler, result,
                                         unit, false)) {
                    emitBody(builder, *ctx.chunk, &ctx.origin, options.originalSource);
                }
            }
        }
        result.status = result.diagnostics.empty() ? Status::Success : Status::Unsupported;
        return result;
    }
} // namespace cial::decompile
