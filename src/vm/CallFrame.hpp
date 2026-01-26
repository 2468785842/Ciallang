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
        Bytecode::Chunk *chunk{};
        FuncMeta *funcMeta{}; // funcMeta != nullptr is function call
        Value thisObj{};
        Opt<u64> ret{};
        u64 pc{};

        explicit CallFrame() = default;

        explicit CallFrame(Bytecode::Chunk *chunk, const Opt<u64> ret, Bytecode::FastRegisterPool &pool) :
            chunk(chunk), ret(ret), _pool(&pool), _sp(pool.allocFrame(chunk->getRegCount())) {}

        explicit CallFrame(FuncMeta *funcMeta, const Opt<u64> ret, Bytecode::FastRegisterPool &pool) :
            chunk(funcMeta->chunk), funcMeta(funcMeta), ret(ret), _pool(&pool),
            _sp(pool.allocFrame(funcMeta->chunk->getRegCount()) - funcMeta->arity) {}

        CallFrame(CallFrame &&callFrame) noexcept :
            closure(callFrame.closure), chunk(callFrame.chunk), funcMeta(callFrame.funcMeta),
            thisObj(std::move(callFrame.thisObj)), ret(callFrame.ret), pc(callFrame.pc), _pool(callFrame._pool),
            _sp(callFrame._sp) {
            callFrame.closure = nullptr;
            callFrame.chunk = nullptr;
            callFrame.funcMeta = nullptr;
            callFrame.thisObj = Value{};
            callFrame._pool = nullptr;
        }

        CallFrame &operator=(CallFrame &&callFrame) noexcept {
            if(this != &callFrame) {
                new(this) CallFrame(std::move(callFrame));
            }

            return *this;
        }

        CallFrame(const CallFrame &) = delete;
        CallFrame &operator=(const CallFrame &) = delete;

        ~CallFrame() {
            if(_pool) {
                _pool->maybeShrink(_sp);
                _pool = nullptr;
            }
        }

        [[nodiscard]] u64 getSP() const noexcept { return _sp; }

        [[nodiscard]] Value *getArgs(const size_t argCount) const { return _pool->ptrAt(_pool->used() - argCount); }

        [[nodiscard]] Value &getReg(const u64 reg) { return *_pool->ptrAt(_sp + reg); }

        [[nodiscard]] const Value &getReg(const u64 reg) const { return *_pool->ptrAt(_sp + reg); }

    private:
        friend class Bytecode::VMState;
        Bytecode::FastRegisterPool *_pool{ nullptr };
        u64 _sp{};
    };
} // namespace cial