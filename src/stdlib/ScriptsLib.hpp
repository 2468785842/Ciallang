// Copyright (c) 2026/10/30
//
// Scripts 宿主类（krkr2 tTJSNC_Scripts 对应物，ScriptMgnIntf.cpp:1295-1570）：
// execStorage/evalStorage 读兄弟/指定存储脚本并执行（TVPExecuteStorage 的
// CLI 简化版——无 xp3 归档与字节码直载，modestr/context 实参按 TJS2 形参
// 个数接受但本实现仅取第 1 参）。execStorage 按语句脚本执行（嵌套双帧，
// 全局上下文，顶层 var/函数声明经 DGlobal 落全局），evalStorage 按
// 表达式求值返回。compileStorage 依赖字节码导出，本引擎暂无库内等价能力，
// 不实现（见 doc/compatibility.md 偏差记录）。
#pragma once

#include <filesystem>

#include "types/Function.hpp"
#include "types/Object.hpp"
#include "types/String.hpp"
#include "types/TypeId.hpp"
#include "types/Value.hpp"

namespace cial {
    class Context;

    namespace vm {
        class VMState;
    }

    // Scripts 命名空间承载对象（不能 new）。方法经注册期创建的绑定
    // NativeFunction 暴露：成员读走 propGetMember 的对象自身 getProp 回落
    // 路径（与 Math.RandomGenerator 同机制），不经 NativeRegister 方法表。
    // 持有所属 VMState（嵌套 run 的执行引擎句柄）与存储基目录。
    class ScriptsNS final : public Object {
    public:
        ScriptsNS() = default;

        [[nodiscard]] TypeId typeId() const noexcept override { return TypeId::Scripts; }

        void call(vm::VMState &, u32, size_t) override { throw std::runtime_error("Scripts is not callable"); }

        void setState(vm::VMState *state) noexcept { _state = state; }

        [[nodiscard]] vm::VMState *state() const noexcept { return _state; }

        // 存储基目录：相对路径相对当前执行脚本所在目录；为空时相对进程 cwd
        void setBaseDir(std::filesystem::path dir) { _baseDir = std::move(dir); }

        [[nodiscard]] const std::filesystem::path &baseDir() const noexcept { return _baseDir; }

        void setExecMethod(const Atom name, NativeFunction *fn) {
            execAtom = name;
            execFn = fn;
        }

        void setEvalMethod(const Atom name, NativeFunction *fn) {
            evalAtom = name;
            evalFn = fn;
        }

        // 存储名解析：绝对路径直用；相对路径相对基目录（krkr2
        // TVPSearchPlacedPath 的 CLI 简化版，不支持归档内路径）
        [[nodiscard]] std::filesystem::path resolveStorage(const std::string &name) const;

        Value getProp(Atom a) override;

        [[nodiscard]] bool hasProp(Atom a) const override;

        void marked() noexcept override;

    private:
        std::filesystem::path _baseDir;
        Atom execAtom = ATOM_INVALID;
        Atom evalAtom = ATOM_INVALID;
        NativeFunction *execFn{};
        NativeFunction *evalFn{};
        vm::VMState *_state{}; // 非 GC 对象，不参与标记
    };

    namespace stdlib {
        // 注册 Scripts 宿主类（重注册即替换全局 Scripts，用于 CLI/测试回填
        // VMState 与基目录）。state 为空时方法调用抛"未绑定"运行期异常；
        // baseDir 为空时相对路径相对进程 cwd。
        void registerScriptsLib(Context &context, vm::VMState *state = nullptr,
                                std::filesystem::path baseDir = {});

        Value scriptsExecStorage(VM *vm, Value thisVal, size_t argCount, Value *args);
        Value scriptsEvalStorage(VM *vm, Value thisVal, size_t argCount, Value *args);
    } // namespace stdlib
} // namespace cial
