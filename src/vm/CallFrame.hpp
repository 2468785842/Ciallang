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
    struct CallFrame {
        CallFrame *closure{};
        vm::Chunk *chunk{};
        FuncMeta *funcMeta{}; // funcMeta != nullptr is function call
        Value thisObj{};
        Opt<u16> ret{};
        u64 pc{};

        explicit CallFrame() = default;

        explicit CallFrame(vm::Chunk *chunk, const Opt<u16> ret, vm::FastRegisterPool &pool) :
            chunk(chunk), ret(ret), _pool(&pool), _sp(pool.allocFrame(chunk->getRegCount())) {}

        explicit CallFrame(FuncMeta *funcMeta, const Opt<u16> ret, vm::FastRegisterPool &pool) :
            chunk(funcMeta->chunk), funcMeta(funcMeta), ret(ret), _pool(&pool),
            _sp(pool.allocFrame(funcMeta->chunk->getRegCount()) - funcMeta->arity) {}

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

    private:
        friend class vm::VMState;
        vm::FastRegisterPool *_pool{};
        u32 _sp{};
    };
} // namespace cial