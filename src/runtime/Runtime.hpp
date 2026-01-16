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

#include "AtomTable.hpp"

#include "gc/gc.hpp"

namespace cial {
    namespace Inter {
        class IRGenerator;
    }
    struct FuncMeta;

    class MarkSweepHeader;

    class Runtime {
    public:
        AtomTable atomTable{};

        MarkSweep markSweep{ 1024 };

        std::vector<MarkSweepHeader *> handles{};

        explicit Runtime() = default;

        explicit Runtime(const std::uint32_t gcSize) noexcept : markSweep{ gcSize } {}

        void addHandleVal(MarkSweepHeader *);

        void removeHandleVal(const MarkSweepHeader *);

        void collectMark() const {
            for(auto *handle : handles) {
                if(handle)
                    handle->marked();
            }
        }

    private:
        template <typename T, typename... Args>
            requires std::is_base_of_v<MarkSweepHeader, T>
        T *allocate(Args &&...args) {

            static_assert(sizeof(T) <= NODE_SIZE, "GC NODE_SIZE too small for object type");

            MarkSweepHeader *next = markSweep._nextFree->_next;
            T *newObj = new(markSweep._nextFree) T(std::forward<Args>(args)...);
            newObj->_next = next;
            newObj->_marked = false;
            newObj->_isFree = false;

            markSweep._nextFree = next;
            return newObj;
        }

    public:
        /**
         * Refactoring is needed because if `create2` is called again after calling the new `create1`,
         * it will trigger garbage collection, and `create1` might be garbage collected.
         * Should the new `create` object be added to the root object?
         *
         * 1. Handle<T> a = create(xxx); // RAII
         *    xxx = move(a);
         * 2. a = create(xxx);
         *    if !a gc(); // self check?
         */
        template <typename T, typename... Args>
            requires std::is_base_of_v<MarkSweepHeader, T>
        T *create(Args &&...args) {
            if(!markSweep._nextFree || !markSweep._nextFree->_isFree) {
                markSweep.findIdleNode([this] { collectMark(); });
            }

            return allocate<T>(std::forward<Args>(args)...);
        }

    private:
        friend class Inter::IRGenerator;
        template <typename T, typename... Args>
            requires std::is_base_of_v<MarkSweepHeader, T>
        T *createNoGC(Args &&...args) {
            if(!markSweep._nextFree || !markSweep._nextFree->_isFree) {
                markSweep.find();
                if(!markSweep._nextFree) {
                    return nullptr;
                }
            }

            return allocate<T>(std::forward<Args>(args)...);
        }
    };
} // namespace cial