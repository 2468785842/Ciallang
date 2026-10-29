/*
 * Copyright (c) 2024/6/23 下午8:18
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

#include <functional>

#include "types/Types.hpp"

namespace cial::vm {
    class VMState;
}

namespace cial {

    class MarkSweepHeader {
        friend class MarkSweep;
        friend class Runtime;

    public:
        MarkSweepHeader() = default;
        virtual ~MarkSweepHeader() = default;

        virtual bool needsFinalization() const noexcept { return false; }
        virtual bool canFinalizeIn(const vm::VMState &) const noexcept { return true; }
        virtual void finalizeDeferred(vm::VMState &) {}

        virtual void marked() noexcept {
            if(!_marked)
                _marked = true;
        }

    protected:
        // 派生类 marked() 必须以 "if(alreadyMarked()) return;" 开头：
        // 对象图存在环（FuncMeta↔Chunk、DataObject↔ClassObject 等），
        // 不检查标志会在环上无限递归
        bool _marked{ false };

    private:
        MarkSweepHeader *_next{ nullptr };
        bool _isFree{ false };
    };

    // 类型 trait
    // template <typename T>
    // static constexpr bool is_gc_object_v = std::is_base_of_v<RefCountHeader, T>;

    static constexpr size_t NODE_SIZE = 512; // Byte

    class MarkSweep {
        friend class Runtime;

    public:
        using MarkFunc = std::function<void()>;
        explicit MarkSweep(const size_t size) : _head(initFreeList(size)) { _nextFree = _head; }

        ~MarkSweep() {
            destroyObjects();
            for(MarkSweepHeader *cursor = _head; cursor;) {
                MarkSweepHeader *nextMarkSweepHeader = cursor->_next;
                cursor->~MarkSweepHeader();
                free(cursor);
                cursor = nextMarkSweepHeader;
            }
        }

        void collect(const MarkFunc &mark) {
            mark();
            // 先快照所有不可达对象，再标记队列；对象之间的环不能使另一个
            // 待终结对象被遗漏。回调仅由 VM 安全点执行，绝不在 sweep 重入。
            const size_t first = pendingFinalizers.size();
            for(auto *node = _head; node; node = node->_next)
                if(!node->_isFree && !node->_marked && node->needsFinalization())
                    pendingFinalizers.push_back(node);
            for(size_t i = first; i < pendingFinalizers.size(); ++i)
                pendingFinalizers[i]->marked();
            sweep();
        }

        MarkSweepHeader *findIdleNode(const MarkFunc &mark);

        void sweep();
        void destroyObjects(); // 释放 C++ 资源；不执行脚本终结回调

        Vec<MarkSweepHeader *> pendingFinalizers;
        bool drainingFinalizers{ false };

        void find() {
            _nextFree = _head;
            while(_nextFree && !_nextFree->_isFree) {
                _nextFree = _nextFree->_next;
            }
        }

        // Byte Unit
        [[nodiscard]] std::pair<u32, u32> memoryInfo() const;

    private:
        MarkSweepHeader *_nextFree{ nullptr };
        MarkSweepHeader *_head{ nullptr };

        static MarkSweepHeader *initFreeList(const size_t freeListSize) {
            MarkSweepHeader *head{ nullptr };
            for(int i = 0; i < freeListSize; ++i) {
                auto *gcObj = static_cast<MarkSweepHeader *>(malloc(NODE_SIZE));
                new(gcObj) MarkSweepHeader{};
                gcObj->_marked = false;
                gcObj->_isFree = true;
                gcObj->_next = head;
                head = gcObj;
            }

            return head;
        }
    };

} // namespace cial
