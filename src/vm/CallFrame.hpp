/*
 * Copyright (c) 2025/12/31 上午8:08
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

#include "vm/Chunk.hpp"
#include "vm/FastRegisterPool.hpp"

#include "types/Value.hpp"

namespace cial {
    class DataObject;

    struct CallFrame {
        CallFrame *closure{};
        vm::Chunk *chunk{};
        FuncMeta *funcMeta{}; // funcMeta != nullptr is function call
        Value thisObj{};
        DataObject *thisData{}; // thisObj 为 DataObject 时的缓存，否则 nullptr（消除热路径 dynamic_cast）
        Opt<u16> ret{};
        u64 pc{};
        // 本帧实际收到的实参个数（arity 为补齐后的形参数）：省略透传（CallOmit）、
        // unnamed 展开（CallExpand）与 collapse 收拢（CollapseArgs）依赖。
        // 由各调用入口（Function::call / ClassObject::call 构造路径）在压帧后填写；
        // 非函数帧（eval/顶层/字段初始化）保持 0
        u32 actualArgc{};
        // 进入帧时的 with 上下文栈深（VMState::allocCallFrame 填写）：
        // 帧释放时 withStack 截断到此深度（unwind/正常返回共用 freeCallFrame）
        size_t withDepth{};

        // thisObj 的赋值入口：同步刷新 thisData 缓存（定义在 VMState.cpp，需要完整 DataObject 类型）。
        // 不要绕过它直接写 thisObj，否则缓存失效。
        void setThisObj(Value v);

        // thisObj 为空(全局函数调用)的快速通道:整体清零,无 dynamic_cast
        void setThisObjNull() {
            thisObj = Value{};
            thisData = nullptr;
        }

        explicit CallFrame() = default;

        explicit CallFrame(vm::Chunk *chunk, const Opt<u16> ret, vm::FastRegisterPool &pool,
                           const size_t extraSlots = 0) :
            chunk(chunk), ret(ret), _pool(&pool),
            _sp(pool.allocFrame(chunk->getRegCount() + extraSlots)) {}

        // extraSlots：实际实参超过 arity 的超额数。窗口按超额扩容后，%0.. 仍与
        // 实参重合而静态槽（prologue 的 this/void 写入位）整体上移——否则
        // prologue 会覆盖掉超额实参（省略透传/unnamed 展开/collapse 依赖它们）
        explicit CallFrame(FuncMeta *funcMeta, const Opt<u16> ret, vm::FastRegisterPool &pool,
                           const size_t extraSlots = 0) :
            chunk(funcMeta->chunk), funcMeta(funcMeta), ret(ret), _pool(&pool),
            _sp(pool.allocFrame(funcMeta->chunk->getRegCount() + extraSlots) - funcMeta->arity) {}

        CallFrame(CallFrame &&callFrame) noexcept :
            closure(callFrame.closure), chunk(callFrame.chunk), funcMeta(callFrame.funcMeta),
            thisObj(std::move(callFrame.thisObj)), ret(callFrame.ret), pc(callFrame.pc), _pool(callFrame._pool),
            _sp(callFrame._sp) {
            new(&callFrame) CallFrame{};
        }

        CallFrame &operator=(CallFrame &&callFrame) noexcept {
            if(this != &callFrame) {
                this->~CallFrame();
                new(this) CallFrame(std::move(callFrame));
            }

            return *this;
        }

        CallFrame(const CallFrame &) = delete;
        CallFrame &operator=(const CallFrame &) = delete;

        ~CallFrame() {
            if(_pool) {
                _pool->maybeShrink(_sp);
            }
            new(this) CallFrame{};
        }

        [[nodiscard]] u64 getSP() const noexcept { return _sp; }

        [[nodiscard]] Value *getArgs(const size_t argCount) const { return _pool->ptrAt(_pool->used() - argCount); }

        [[nodiscard]] Value &getReg(const u16 reg) { return *_pool->ptrAt(_sp + reg); }

        [[nodiscard]] const Value &getReg(const u16 reg) const { return *_pool->ptrAt(_sp + reg); }

        [[nodiscard]] Value *regs() const { return _pool->ptrAt(_sp); }

    private:
        friend class vm::VMState;
        vm::FastRegisterPool *_pool{};
        u32 _sp{};
    };
} // namespace cial