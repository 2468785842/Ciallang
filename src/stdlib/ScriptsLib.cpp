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

#include "common/UTF16.hpp"
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
            std::string content{ (std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>() };
            return content;
        }

        std::string utf16LeToUtf8(const u8 *data, const size_t bytes) {
            if(bytes % 2 != 0)
                throw std::runtime_error("truncated UTF-16 storage");
            std::u16string units;
            units.reserve(bytes / 2);
            for(size_t i = 0; i < bytes; i += 2)
                units.push_back(static_cast<char16_t>(data[i] | (static_cast<unsigned>(data[i + 1]) << 8)));
            for(size_t i = 0; i < units.size(); ++i) {
                const auto unit = units[i];
                if(unit >= 0xd800 && unit <= 0xdbff) {
                    if(i + 1 >= units.size() || units[i + 1] < 0xdc00 || units[i + 1] > 0xdfff)
                        throw std::runtime_error("unpaired surrogate in UTF-16 storage");
                    ++i;
                } else if(unit >= 0xdc00 && unit <= 0xdfff) {
                    throw std::runtime_error("unpaired surrogate in UTF-16 storage");
                }
            }
            return Common::fromUtf16(units);
        }

        std::string readTextDecoded(const std::filesystem::path &path) {
            const std::string raw = readFileBytes(path);
            if(raw.size() >= 3 && static_cast<u8>(raw[0]) == 0xEF && static_cast<u8>(raw[1]) == 0xBB &&
               static_cast<u8>(raw[2]) == 0xBF)
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
                throw std::runtime_error(std::string{ who } + " requires the Scripts namespace receiver");
            auto *obj = thisVal.asObject().unwrap();
            if(!obj || obj->typeId() != TypeId::Scripts)
                throw std::runtime_error(std::string{ who } + " requires the Scripts namespace receiver");
            return static_cast<ScriptsNS *>(obj); // final 类，typeId 判型后 static_cast 安全
        }

        std::string compileErrorMessage(const Common::Result &r) {
            for(const auto &m : r.messages()) {
                if(m.isError())
                    return fmt::format("{} (line {})", m.message(), m.location().start().line);
            }
            return "unknown compile error";
        }

        Value executeStorage(ScriptsNS &ns, const size_t argCount, Value *args, const bool expression) {
            auto *state = ns.state();
            if(!state)
                throw std::runtime_error("Scripts is not bound to a VM state in this session");
            if(argCount < 1 || !args[0].isString())
                throw std::runtime_error("Scripts storage name must be a string");
            const auto name = args[0].asString().unwrap()->toStdStr();
            std::string mode;
            if(argCount > 1 && !args[1].isVoid()) {
                Value value = args[1];
                if(value.toString().isFailed())
                    throw std::runtime_error("storage mode must be convertible to string");
                mode = value.asString().unwrap()->toStdStr();
            }
            const Value executionContext = argCount > 2 ? args[2] : Value{};
            if(!executionContext.isVoid() && !executionContext.isObject())
                throw std::runtime_error("execution context must be an object");
            TextStorage storage;
            if(auto *reader = ns.storageReader()) {
                const auto *frame = state->curFrame();
                storage = reader->read(name, mode, frame ? frame->chunk->sourceName() : std::string{});
            } else {
                if(!mode.empty())
                    throw std::runtime_error("storage mode requires a host TextStorageReader");
                const auto path = ns.resolveStorage(name);
                storage = { path.generic_string(), readTextDecoded(path) };
            }
            if(storage.sourceName.empty())
                throw std::runtime_error("storage reader returned an empty source identity");
            Common::Result result;
            Common::SourceFile source;
            source.setSourceName(storage.sourceName);
            source.load(result, storage.text);
            VM vm{ state };
            auto value = vm.execute(source, result, expression, executionContext);
            if(result.isFailed())
                throw std::runtime_error(
                    fmt::format("cannot compile storage {}: {}", storage.sourceName, compileErrorMessage(result)));
            return *value;
        }
    } // namespace
} // namespace cial::stdlib

namespace cial {
    // ---------- ScriptsNS ----------

    std::filesystem::path ScriptsNS::resolveStorage(const std::string &name) const {
        const std::filesystem::path p{ name };
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

    void ScriptsNS::markChildren() noexcept {
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
        return executeStorage(*ns, argCount, args, false);
    }

    Value scriptsEvalStorage(VM *, Value thisVal, const size_t argCount, Value *args) {
        auto *ns = scriptsNamespace(thisVal, "Scripts.evalStorage");
        return executeStorage(*ns, argCount, args, true);
    }

    // ---------- 注册 ----------

    void registerScriptsLib(Context &context, vm::VMState *state, std::filesystem::path baseDir,
                            std::shared_ptr<TextStorageReader> reader) {
        auto *ns = context.registerClass<ScriptsNS>("Scripts"_str);
        ns->setState(state);
        ns->setBaseDir(std::move(baseDir));
        ns->setStorageReader(std::move(reader));

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
