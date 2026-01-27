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
    namespace inter {
        class IRGenerator;
    }
    struct FuncMeta;

    class MarkSweepHeader;

    template <typename T>
    class RootGuard;

    class Runtime {
    public:
        static constexpr Atom ATOM_OBJECT{ 1 };
        static constexpr Atom ATOM_FUNCTION{ 2 };
        static constexpr Atom ATOM_CLASS{ 3 };

        AtomTable atomTable = [] {
            AtomTable at{};
            at.intern("Object"_str); // ATOM_OBJECT
            at.intern("Function"_str); // ATOM_FUNCTION
            at.intern("Class"_str); // ATOM_CLASS
            return std::move(at);
        }();

        MarkSweep markSweep{ 512 };

        std::vector<MarkSweepHeader *> handles{};

        explicit Runtime() = default;

        explicit Runtime(const u32 gcSize) noexcept : markSweep{ gcSize } {}

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
        template <typename T, typename... Args>
            requires std::is_base_of_v<MarkSweepHeader, T>
        RootGuard<T> create(Args &&...args) {
            if(!markSweep._nextFree || !markSweep._nextFree->_isFree) {
                markSweep.findIdleNode([this] { collectMark(); });
            }

            return RootGuard<T>{ this, allocate<T>(std::forward<Args>(args)...) };
        }

    private:
        friend class inter::IRGenerator;
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

    template <typename T>
    class RootGuard {
        Runtime *_rt;
        T *_ptr;

    public:
        RootGuard(Runtime *rt, T *ptr) : _rt(rt), _ptr(ptr) {
            if(_ptr)
                _rt->addHandleVal(_ptr);
        }

        ~RootGuard() {
            if(_ptr)
                _rt->removeHandleVal(_ptr);
        }

        T *get() const { return _ptr; }
        T *operator->() const { return _ptr; }
    };

} // namespace cial