/*
 * Copyright (c) 2025/12/24 上午8:08
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

#include <ranges>
#include <unordered_map>

#include "Chunk.hpp"
#include "FastRegisterPool.hpp"

#include "gc/GC.hpp"
#include "parser/AtomTable.hpp"
#include "parser/OctetTable.hpp"
#include "types/Object.hpp"
#include "types/Value.hpp"

namespace Cial {
    struct FuncMeta;

    class MarkSweepHeader;

    struct CallFrame {
        const FuncMeta *funcMeta{};
        const Bytecode::Chunk *chunk{};
        const Object *thisObj{};
        std::optional<Bytecode::Register> ret{};
        std::uint64_t baseRegSP{};
        std::uint64_t pc{};

        explicit CallFrame() = default;

        explicit CallFrame(const Bytecode::Chunk *chunk_, const std::optional<Bytecode::Register> ret,
                           Bytecode::FastRegisterPool &pool) :
            chunk(chunk_), ret(ret), baseRegSP(pool.allocFrame(chunk_->getRegCount())), _pool(&pool) {}

        CallFrame(CallFrame &&callFrame) noexcept :
            chunk(callFrame.chunk), ret(callFrame.ret), baseRegSP(callFrame.baseRegSP), pc(callFrame.pc),
            _pool(callFrame._pool) {
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

        [[nodiscard]] Value &getReg(const Bytecode::Register reg) { return *_pool->ptrAt(baseRegSP + reg.index()); }

        [[nodiscard]] const Value &getReg(const Bytecode::Register reg) const {
            return *_pool->ptrAt(baseRegSP + reg.index());
        }

    private:
        Bytecode::FastRegisterPool *_pool{ nullptr };
    };

    class Runtime {
    public:
        AtomTable atomTable{};
        OctetTable octetTable{};

        MarkSweep markSweep{ 1024 };

        std::unordered_map<Atom, Value> gObj{};

        Bytecode::FastRegisterPool regPool{};

        std::vector<MarkSweepHeader *> handles{};

        // Stack Max Depth Is 1024
        static constexpr auto maxCallDepth = 1024;
        CallFrame callStack[maxCallDepth];

        size_t stackTop{ 0 }; // callFrame count

        explicit Runtime() = default;

        explicit Runtime(const std::uint32_t gcSize) noexcept : markSweep{ gcSize } {}

        void addHandleVal(MarkSweepHeader *);

        void removeHandleVal(const MarkSweepHeader *);

        void collectMark() {

            for(const auto &val : gObj | std::views::values) {
                if(val.isObject()) {
                    if(auto *msHeader = val.toObject()) {
                        msHeader->marked();
                        if(msHeader->_name != ATOM_INVALID) {
                            atomTable.get(msHeader->_name)->marked();
                        }
                    }
                }
            }

            // scan stack
            for(std::uint32_t i = 0; i < stackTop; ++i) {
                const CallFrame &callFrame = callStack[i];
                const std::uint32_t regCount = callFrame.chunk->getRegCount();
                for(std::uint32_t j = 0; j < regCount; ++j) {
                    Value val{};

                    for(const auto &localVar : callFrame.funcMeta->localVars) {
                        if(localVar.reg.index() == j && localVar.endPC <= callFrame.pc) {
                            val = callFrame.getReg(Bytecode::Register{ j });
                            break;
                        }
                    }

                    if(val.isObject()) {
                        if(auto *msHeader = val.toObject()) {
                            msHeader->marked();
                            if(msHeader->_name != ATOM_INVALID) {
                                atomTable.get(msHeader->_name)->marked();
                            }
                        }
                    }
                }
            }

            for(auto *handle : handles) {
                if(handle)
                    handle->marked();
            }
        }

        template <typename T, typename... Args>
            requires std::is_base_of_v<MarkSweepHeader, T>
        T *allocate(Args &&...args) {
            if(!markSweep._nextFree || !markSweep._nextFree->_isFree) {
                markSweep.findIdleNode([this] { collectMark(); });
            }

            MarkSweepHeader *next = markSweep._nextFree->_next;
            T *newObj = new(markSweep._nextFree) T(std::forward<Args>(args)...);
            newObj->_next = next;
            newObj->_marked = false;
            newObj->_isFree = false;

            markSweep._nextFree = next;
            return newObj;
        }
    };
} // namespace Cial