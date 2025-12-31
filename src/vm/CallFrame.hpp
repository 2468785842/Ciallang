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

namespace Cial {
    struct CallFrame {
        Bytecode::Chunk *chunk{};
        FuncMeta *funcMeta{}; // funcMeta != nullptr is function call
        Object *context{};
        OptReg ret{};
        std::uint64_t pc{};

        explicit CallFrame() = default;

        explicit CallFrame(Bytecode::Chunk *chunk_, const OptReg ret, Bytecode::FastRegisterPool &pool) :
            chunk(chunk_), ret(ret), _pool(&pool), _sp(pool.allocFrame(chunk_->getRegCount())) {}

        explicit CallFrame(FuncMeta *funcMeta, const OptReg ret, Bytecode::FastRegisterPool &pool) :
            chunk(funcMeta->chunk), funcMeta(funcMeta), ret(ret), _pool(&pool),
            _sp(pool.allocFrame(funcMeta->chunk->getRegCount()) - funcMeta->arity) {}

        CallFrame(CallFrame &&callFrame) noexcept :
            chunk(callFrame.chunk), funcMeta(callFrame.funcMeta), context(callFrame.context), ret(callFrame.ret),
            pc(callFrame.pc), _pool(callFrame._pool), _sp(callFrame._sp) {
            callFrame.funcMeta = nullptr;
            callFrame.context = nullptr;
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
                _pool->freeFrame(chunk->getRegCount());
            }
        }

        [[nodiscard]] Value &getReg(const Bytecode::Register reg) { return *_pool->ptrAt(_sp + reg.index()); }

        [[nodiscard]] const Value &getReg(const Bytecode::Register reg) const {
            return *_pool->ptrAt(_sp + reg.index());
        }

    private:
        Bytecode::FastRegisterPool *_pool{ nullptr };
        std::uint64_t _sp{};
    };
} // namespace Cial