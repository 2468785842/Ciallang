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

#include <algorithm>
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

    /**
     * GC 额外根的提供方。Runtime 自身只追踪 RootGuard 句柄，
     * 寄存器池/调用栈/全局对象等运行时根由 Context 注册提供。
     * 没有它，执行期任何一次 GC 都会把活对象当垃圾回收。
     */
    class MarkRootProvider {
    public:
        virtual ~MarkRootProvider() = default;

        virtual void collectMark() const = 0;
    };

    // 高频属性名：构造期驻留一次，热路径（Array::isCountAtom / RegExp 属性协议）
    // 直接比较 Atom，省去每次访问的 intern 哈希 + map 查找
    struct WellKnownAtoms {
        Atom count{};
        Atom length{};
        // RegExp 匹配状态属性（tjsRegExp.cpp 同名属性）
        Atom reMatches{}, reStart{}, reIndex{}, reLastIndex{}, reInput{};
        Atom reLastMatch{}, reLastParen{}, reLeftContext{}, reRightContext{};
        // RegExp.last 静态属性（tTJSNC_RegExp::LastRegExp）
        Atom reLast{};
    };

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

        WellKnownAtoms wkAtoms = [this] {
            WellKnownAtoms w{};
            w.count = atomTable.intern("count"_str);
            w.length = atomTable.intern("length"_str);
            w.reMatches = atomTable.intern("matches"_str);
            w.reStart = atomTable.intern("start"_str);
            w.reIndex = atomTable.intern("index"_str);
            w.reLastIndex = atomTable.intern("lastIndex"_str);
            w.reInput = atomTable.intern("input"_str);
            w.reLastMatch = atomTable.intern("lastMatch"_str);
            w.reLastParen = atomTable.intern("lastParen"_str);
            w.reLeftContext = atomTable.intern("leftContext"_str);
            w.reRightContext = atomTable.intern("rightContext"_str);
            w.reLast = atomTable.intern("last"_str);
            return w;
        }();

        MarkSweep markSweep{ 512 };

        std::vector<MarkSweepHeader *> handles{};

        // RegExp.last 静态属性（tTJSNC_RegExp::LastRegExp）：最近一次执行过
        // test/exec 的 RegExp 实例（失败也更新；match/replace 不更新）。
        // 持有 GC 对象的全局数据，接入下方 collectMark 标记
        MarkSweepHeader *lastRegExp{};

        explicit Runtime() = default;

        explicit Runtime(const u32 gcSize) noexcept : markSweep{ gcSize } {}

        void addHandleVal(MarkSweepHeader *);

        void removeHandleVal(const MarkSweepHeader *);

        void collectMark() const {
            for(auto *handle : handles) {
                if(handle)
                    handle->marked();
            }
            if(lastRegExp)
                lastRegExp->marked();
        }

        void addMarkRoot(const MarkRootProvider *provider) { _markRoots.push_back(provider); }

        void removeMarkRoot(const MarkRootProvider *provider) {
            if(const auto it = std::ranges::find(_markRoots, provider); it != _markRoots.end()) {
                _markRoots.erase(it);
            }
        }

        /** 标记全部根：RootGuard 句柄 + 注册进来的运行时根（寄存器池/调用栈/全局对象等） */
        void collectAllMark() const {
            collectMark();
            for(const auto *provider : _markRoots) {
                provider->collectMark();
            }
        }

    private:
        std::vector<const MarkRootProvider *> _markRoots{};

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
                markSweep.findIdleNode([this] { collectAllMark(); });
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