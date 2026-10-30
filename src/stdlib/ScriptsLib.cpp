// Copyright (c) 2026/10/30
//
// Scripts 宿主类实现（krkr2 tTJSNC_Scripts / TVPExecuteStorage 对应物）。
// 嵌套执行协议：execStorage/evalStorage 在运行中的脚本回调里再入 VM，
// 嵌套 run 的 tmp 宿主帧必须挂全区哨兵 throwHandler 截住 unwind——否则被
// 执行脚本的异常会恢复调用方帧的 pc、在本次 run 内执行其 catch 块，native
// 调用方控制流错乱（AGENTS.md 坑位 4；VMState.cpp dispatchEvalStr /
// VM::callValue 同款协议，异常经 ScriptExceptionCarrier 交还主循环转入
// 脚本异常协议向上传播）。缺失文件抛 std::runtime_error，由主循环统一
// 包装为可捕获 Exception（消息含路径）。
#include "ScriptsLib.hpp"

#include <fstream>

#include "common/UTF8.hpp"
#include "runtime/Context.hpp"
#include "types/Class.hpp"
#include "vm/VM.hpp"

namespace cial::stdlib {
    namespace {
        // ---------- 文件读取（BOM 识别同 SerializeLib：UTF-8/UTF-16LE/BE → UTF-8）----------

        std::string readFileBytes(const std::filesystem::path &path) {
            std::ifstream f(path, std::ios::binary);
            if(!f)
                throw std::runtime_error("cannot open storage: " + path.string());
            std::string content{(std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()};
            return content;
        }

        std::string utf16LeToUtf8(const u8 *data, const size_t bytes) {
            std::string out;
            out.reserve(bytes);
            for(size_t i = 0; i + 1 < bytes;) {
                char32_t u = data[i] | (static_cast<char32_t>(data[i + 1]) << 8);
                i += 2;
                if(u >= 0xD800 && u <= 0xDBFF && i + 1 < bytes) {
                    const char32_t lo = data[i] | (static_cast<char32_t>(data[i + 1]) << 8);
                    if(lo >= 0xDC00 && lo <= 0xDFFF) {
                        u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00);
                        i += 2;
                    }
                }
                const auto enc = Common::utf8Encode(static_cast<i32>(u));
                if(enc.width > 0)
                    out.append(reinterpret_cast<const char *>(enc.data), static_cast<size_t>(enc.width));
            }
            return out;
        }

        std::string readTextDecoded(const std::filesystem::path &path) {
            const std::string raw = readFileBytes(path);
            if(raw.size() >= 3 && static_cast<u8>(raw[0]) == 0xEF && static_cast<u8>(raw[1]) == 0xBB
               && static_cast<u8>(raw[2]) == 0xBF)
                return raw.substr(3);
            if(raw.size() >= 2 && static_cast<u8>(raw[0]) == 0xFF && static_cast<u8>(raw[1]) == 0xFE)
                return utf16LeToUtf8(reinterpret_cast<const u8 *>(raw.data()) + 2, raw.size() - 2);
            if(raw.size() >= 2 && static_cast<u8>(raw[0]) == 0xFE && static_cast<u8>(raw[1]) == 0xFF) {
                // UTF-16BE → 翻转字节序后复用 LE 解码
                std::string sw(raw.substr(2));
                for(size_t i = 0; i + 1 < sw.size(); i += 2)
                    std::swap(sw[i], sw[i + 1]);
                return utf16LeToUtf8(reinterpret_cast<const u8 *>(sw.data()), sw.size());
            }
            return raw;
        }

        // ---------- 公共实参处理 ----------

        // thisVal 必须是注册期绑定的 ScriptsNS（propGetMember 经 getProp 取出的
        // 绑定方法；显式 incontextof 换接收者不支持——与 Math 静态方法同口径）
        ScriptsNS *scriptsNamespace(const Value &thisVal, const char *who) {
            if(!thisVal.isObject())
                throw std::runtime_error(std::string{who} + " requires the Scripts namespace receiver");
            auto *obj = thisVal.asObject().unwrap();
            if(!obj || obj->typeId() != TypeId::Scripts)
                throw std::runtime_error(std::string{who} + " requires the Scripts namespace receiver");
            return static_cast<ScriptsNS *>(obj); // final 类，typeId 判型后 static_cast 安全
        }

        std::filesystem::path requireStoragePath(const ScriptsNS *ns, const size_t argCount, Value *args,
                                                 const char *who) {
            if(argCount < 1)
                throw std::runtime_error(std::string{who} + " expects a storage name");
            if(!args[0].isString())
                throw std::runtime_error(std::string{who} + " expects a string storage name");
            return ns->resolveStorage(args[0].asString().unwrap()->toStdStr());
        }

        std::string compileErrorMessage(const Common::Result &r) {
            for(const auto &m : r.messages()) {
                if(m.isError())
                    return fmt::format("{} (line {})", m.message(), m.location().start().line);
            }
            return "unknown compile error";
        }

        // ---------- 嵌套执行（哨兵双帧协议）----------

        // 压 tmp 哨兵帧 + 目标 chunk 帧、嵌套 run、栈平衡。目标块正常返回时
        // 返回值取自 tmp 帧 retSlot；目标块抛出时哨兵截住 unwind，异常值经
        // ScriptExceptionCarrier 抛出（主循环 catch 转脚本异常协议）。
        //
        // 异常值槽须非 0 寄存器：ThrowHandler::exValueRegNone(0xFFFF) 才是
        // "无槽位"哨兵（2026-10-30 修正——旧约定以 0 表示无槽位，而 0 是合法
        // 寄存器）；callValue/EvalStr 哨兵已同步改为 reg1 承接异常值。
        Value runNestedWithSentinel(vm::VMState &state, vm::Chunk &&compiled) {
            auto evalChunk = std::move(compiled).toHeap(state.rt);

            constexpr u16 retSlot = 0;
            constexpr u16 exSlot = 1; // 异常值槽（须非 0，见上）
            vm::Chunk tmpChunk{};
            tmpChunk.setRegCount(2); // reg0 = 返回值槽，reg1 = 哨兵异常值槽
            tmpChunk.emit<inter::NOP>(); // 哨兵区间 [0,1) 覆盖的 1 字节指令
            tmpChunk.addThrowHandler({ 0, 1, exSlot }); // 全区哨兵：截住 unwind
            tmpChunk.toBytecode(); // 哨兵表必须显式落 bc 层（unwind 只查 bc 表，
                                   // callValue 同款陷阱——惰性编译由 FETCH 触发、
                                   // tmp 帧永不派发，不显式编译形同虚设）

            const size_t tmpSp = state.getRegPoolTop(); // tmp 帧 reg1 的池内位置 = tmpSp + exSlot
            state.allocCallFrame(&tmpChunk);
            state.makeClosure();
            state.allocCallFrame(evalChunk.get(), retSlot);
            state.makeClosure();
            const size_t stackTop = state.context.stackTop();
            state.run();

            // 哨兵路径：eval/tmp 帧均已被 run 弹出（栈深 -2）
            if(state.context.stackTop() == stackTop - 2)
                throw vm::ScriptExceptionCarrier{ state.poolAt(tmpSp + exSlot) };

            const Value ret = state.reg(retSlot);
            // 目标块以 Ret 收尾时 Ret 已自弹（栈深 -1），否则两帧都在（由下方双弹平衡）
            if(stackTop - 1 != state.context.stackTop())
                state.freeCallFrame();
            state.freeCallFrame();
            return ret;
        }

        // 语句脚本形态（execStorage / krkr2 ExecScript）：整段编译执行，
        // 顶层 var/函数经 DGlobal/GGlobal 落全局上下文，结果为 void
        Value execScriptStorage(vm::VMState &state, const std::filesystem::path &path) {
            Common::Result r{};
            Common::SourceFile sourceFile{path.string()};
            sourceFile.load(r);
            if(r.isFailed())
                throw std::runtime_error("cannot open storage: " + path.string());

            syntax::Parser parser{ sourceFile, state.context.pp() };
            syntax::AstNode *node = parser.parse(r);
            if(r.isFailed() || !node)
                throw std::runtime_error(fmt::format("cannot compile storage {}: {}",
                                                     path.string(),
                                                     compileErrorMessage(r)));

            inter::IRGenerator codeGen{ r, state.rt, sourceFile };
            OptReg ignoreReg{};
            Opt<vm::Chunk> compiled = codeGen.parseAst(node, ignoreReg);
            if(r.isFailed() || !compiled)
                throw std::runtime_error(fmt::format("cannot compile storage {}: {}",
                                                     path.string(),
                                                     r.isFailed() ? compileErrorMessage(r) : "compile failed"));
            if(compiled->getInstVec().empty())
                return Value{}; // 空脚本（仅注释/空白）= 无操作（krkr2 ExecScript 语义）

            return runNestedWithSentinel(state, std::move(*compiled));
        }

        // 表达式形态（evalStorage / krkr2 EvalExpression）：求值并返回其值
        Value evalScriptStorage(vm::VMState &state, const std::filesystem::path &path) {
            const auto content = readTextDecoded(path); // 打不开则抛（含路径）

            Common::Result r{};
            Common::SourceFile sourceFile{};
            sourceFile.load(r, content);
            syntax::Parser parser{ sourceFile, state.context.pp() };
            syntax::ExprNode *node = parser.parseExpression(r, true);
            if(r.isFailed() || !node)
                throw std::runtime_error(fmt::format("cannot evaluate storage {}: {}",
                                                     path.string(),
                                                     compileErrorMessage(r)));

            inter::IRGenerator codeGen{ r, state.rt, sourceFile };
            // 哨兵寄存器（0xFFFF 不会被正常分配）：表达式结果寄存器
            OptReg retReg{ inter::Register{ 0xFFFF } };
            Opt<vm::Chunk> compiled = codeGen.parseAst(node, retReg);
            if(r.isFailed() || !compiled || !retReg || compiled->getRegCount() == 0
               || compiled->getInstVec().empty())
                throw std::runtime_error(fmt::format("cannot evaluate storage {}: {}",
                                                     path.string(),
                                                     r.isFailed() ? compileErrorMessage(r)
                                                                  : "expression produced no code"));
            // 表达式块以 Ret 收尾：返回值经 Ret 写回 tmp 帧槽位（dispatchEvalStr 同款）
            compiled->emit<inter::Ret>(*retReg);

            return runNestedWithSentinel(state, std::move(*compiled));
        }
    } // namespace
} // namespace cial::stdlib

namespace cial {
    // ---------- ScriptsNS ----------

    std::filesystem::path ScriptsNS::resolveStorage(const std::string &name) const {
        const std::filesystem::path p{name};
        if(p.is_absolute() || _baseDir.empty())
            return p;
        return _baseDir / p;
    }

    Value ScriptsNS::getProp(const Atom a) {
        if(a == execAtom && execFn)
            return Value{ execFn };
        if(a == evalAtom && evalFn)
            return Value{ evalFn };
        throw std::runtime_error("Scripts member not found");
    }

    bool ScriptsNS::hasProp(const Atom a) const { return (a == execAtom && execFn) || (a == evalAtom && evalFn); }

    void ScriptsNS::marked() noexcept {
        if(_marked)
            return;
        Object::marked();
        if(execFn)
            execFn->marked();
        if(evalFn)
            evalFn->marked();
    }
} // namespace cial

namespace cial::stdlib {
    // ---------- 方法实现 ----------

    Value scriptsExecStorage(VM *, Value thisVal, const size_t argCount, Value *args) {
        auto *ns = scriptsNamespace(thisVal, "Scripts.execStorage");
        if(!ns->state())
            throw std::runtime_error("Scripts is not bound to a VM state in this session");
        const auto path = requireStoragePath(ns, argCount, args, "Scripts.execStorage");
        // 先做存在性检查使"读不到"成为可捕获异常（消息含路径）
        std::error_code ec;
        if(!std::filesystem::is_regular_file(path, ec))
            throw std::runtime_error("cannot open storage: " + path.string());
        return execScriptStorage(*ns->state(), path);
    }

    Value scriptsEvalStorage(VM *, Value thisVal, const size_t argCount, Value *args) {
        auto *ns = scriptsNamespace(thisVal, "Scripts.evalStorage");
        if(!ns->state())
            throw std::runtime_error("Scripts is not bound to a VM state in this session");
        const auto path = requireStoragePath(ns, argCount, args, "Scripts.evalStorage");
        return evalScriptStorage(*ns->state(), path);
    }

    // ---------- 注册 ----------

    void registerScriptsLib(Context &context, vm::VMState *state, std::filesystem::path baseDir) {
        auto *ns = context.registerClass<ScriptsNS>("Scripts"_str);
        ns->setState(state);
        ns->setBaseDir(std::move(baseDir));

        auto &rt = context.rt();
        // 方法对象注册期创建一次并绑定 ns（成员读现场返回同一绑定闭包）。
        // create 的 RootGuard 临时随全表达式析构，存入 ns（全局表保根）后
        // 才创建下一个，两步之间无分配、无 GC 触发点。
        auto *execFn = rt.create<NativeFunction>(&scriptsExecStorage).get();
        ns->setExecMethod(rt.atomTable.intern("execStorage"_str), execFn);
        execFn->setThisObj(Value{ ns });
        auto *evalFn = rt.create<NativeFunction>(&scriptsEvalStorage).get();
        ns->setEvalMethod(rt.atomTable.intern("evalStorage"_str), evalFn);
        evalFn->setThisObj(Value{ ns });
    }
} // namespace cial::stdlib
