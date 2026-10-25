/*
 * Copyright (c) 2024/5/8 上午8:08
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
#pragma once

#include <cstdlib>
#include <fmt/format.h>

#include "CallFrame.hpp"
#include "Chunk.hpp"
#include "FastRegisterPool.hpp"
#include "runtime/Context.hpp"
#include "runtime/Runtime.hpp"

#include "types/Value.hpp"

#include "gen/Register.hpp"

namespace cial::vm {

    enum class PendingCF { None, Throw };

    enum class DispatchRet {
        Continue,
        Call,
        Return,
    };

    // 脚本异常载体：VM::callValue 的回调异常（哨兵截获后）以此 C++ 异常解开
    // native 调用链（std::sort 等），主循环接住后转 _pending+unwind 恢复脚本
    // try/catch——不走 throwException+runtime_error 组合（那会被主循环
    // 「传播中不吞噬」分支直接重抛出 run0，外层脚本 handler 永远接不到）
    struct ScriptExceptionCarrier {
        Value value{};
    };

    class VMState {
    public:
        Runtime &rt;
        Context &context;

        explicit VMState(Context &context) : rt(context.rt()), context(context) {}

        void run0();

        [[nodiscard]] const Value &reg(u16 reg) const;

        [[nodiscard]] Value &regRef(u16 reg) const;

        [[nodiscard]] bool globalHas(Atom atom) const;
        [[nodiscard]] bool globalHas(const std::string &name) const;
        [[nodiscard]] Value global(Atom atom) const;

        void global(Atom atom, const Value &value) const;

        [[nodiscard]] Value global(const std::string &name) const;

        void global(const std::string &name, const Value &value) const;
        [[nodiscard]] bool hasThis(Atom atom) const;
        [[nodiscard]] Value getThis(Atom atom) const;
        void setThis(Atom atom, const Value &v) const;
        [[nodiscard]] Value getUpVal(Atom atom) const;

        void setZF(const bool zf) { _zf = zf; }

        [[nodiscard]] bool getZF() const { return _zf; }

        // pc 语义：下一条待执行指令的偏移（扁平字节码空间）。
        // 多态执行路径(run)删除后，唯一调用方是 unwind 的异常恢复
        void setPC(const u64 pc) const { _curFrame->pc = pc; }

        [[nodiscard]] u64 getPC() const { return _curFrame->pc; }

        void pushVoid(const size_t n) const {
            const size_t base = _pool.allocFrame(n);
            for(size_t i = 0; i < n; i++) {
                *_pool.ptrAt(base + i) = Value{};
            }
        }

        void push(const Value &v) const { *_pool.ptrAt(_pool.allocFrame(1)) = v; }

        void pop(const size_t count) const { _pool.freeFrame(count); }

        void makeClosure() { _curFrame->closure = _stackTop > 1 ? prevFrame() : nullptr; }

        template <typename T>
        void allocCallFrame(T *arg, const Opt<u16> ret = {}) {
            if(_stackTop >= Context::maxCallDepth)
                throw std::runtime_error("Call stack overflow");
            _curFrame = new(&_callStack[_stackTop++]) CallFrame{ arg, ret, _pool };
            _curFrame->withDepth = withStack.size();
        }

        void freeCallFrame() {
            if(_stackTop == 0)
                throw std::runtime_error("Call stack underflow");
            --_stackTop;
            // with 上下文随帧截断（unwind / 正常返回共用此路径）
            while(withStack.size() > _callStack[_stackTop].withDepth) {
                rt.removeHandleVal(withStack.back());
                withStack.pop_back();
            }
            // 先出栈再取新栈顶；栈清空时 _curFrame 置空，
            // 不能写 &_callStack[-1]（会越界踩坏相邻成员）
            _curFrame = _stackTop > 0 ? &_callStack[_stackTop - 1] : nullptr;
            _callStack[_stackTop].~CallFrame();
        }

        [[nodiscard]] size_t getRegPoolTop() const { return _pool.used(); }

        // 读池内任意槽（VM::callValue 取哨兵异常值等；槽须仍在已分配窗口内）
        [[nodiscard]] Value poolAt(const size_t globalIndex) const { return *_pool.ptrAt(globalIndex); }

        [[nodiscard]] CallFrame *curFrame() noexcept { return _curFrame; }
        [[nodiscard]] const CallFrame *curFrame() const noexcept { return _curFrame; }

        [[nodiscard]] CallFrame *prevFrame() noexcept { return _curFrame - 1; }
        [[nodiscard]] const CallFrame *prevFrame() const noexcept { return _curFrame - 1; }

        void unwind();

        void throwException(const Value &v) {
            _pending = PendingCF::Throw;
            _exValue = v;
            if(_exValue.isObject()) {
                rt.addHandleVal(_exValue.asObject().value());
            }
        }

        void clearException() {
            if(_exValue.isObject()) {
                rt.removeHandleVal(_exValue.asObject().value());
            }
            _pending = PendingCF::None;
            _exValue = Value{};
        }

        [[nodiscard]] std::string dumpCurConstants() const { return _curFrame->chunk->dumpConstants(&rt).toStdStr(); }

        [[nodiscard]] std::string dumpCurInstructions() const {
            return _curFrame->chunk->dumpInstructions(this).toStdStr();
        }

        [[nodiscard]] std::string dumpCurLocalVars() const {
            std::stringstream ss{};
            for(size_t i = 0; i < _stackTop; i++) {
                const auto &call = _callStack[i];
                if(!call.funcMeta)
                    continue;

                for(const auto &[identifier, reg, startPC, endPC] : call.funcMeta->localVars) {
                    if(startPC.address() <= getPC() && getPC() < endPC.address())
                        continue;
                    const auto *entry = rt.atomTable.get(identifier);
                    auto varName = "?unknow_var_name?"_str;
                    if(entry)
                        varName = *entry->str;
                    ss << fmt::format("{} = {}\n", varName, call.getReg(reg.index()));
                }
            }
            return ss.str();
        }

        [[nodiscard]] std::string dumpCurRegisters() const {
            std::stringstream ss{};
            for(u32 j = 0; j < _curFrame->chunk->getRegCount(); j++) {
                ss << fmt::format("(%{}): {}\n", j, _curFrame->getReg(j));
            }
            return ss.str();
        }

    private:
        FastRegisterPool &_pool{ context.regPool() };
        CallFrame *_curFrame{ context.callStack() };
        CallFrame *_callStack{ context.callStack() };
        size_t &_stackTop{ context.stackTop() }; // callFrame count
        bool _zf{ false };
        PendingCF _pending{};
        Value _exValue{};
        // with 语句上下文栈（裸词链 .member 的查找基座）；对象挂 Runtime handle 保 GC，
        // 帧释放时截断到 CallFrame::withDepth（unwind/正常返回共用 freeCallFrame）
        Vec<Object *> withStack{};

#define DISPATCH_OP_METHOD(op)                                                                                         \
    CIAL_INLINE DispatchRet dispatch##op(CallFrame *cur, const Constant *constants, Value *regs, const u8 *ip, u64 &pc);
        CIAL_BYTECODE_OPCODE_ENUMS(DISPATCH_OP_METHOD)
#undef DISPATCH_OP_METHOD
    };
} // namespace cial::vm
