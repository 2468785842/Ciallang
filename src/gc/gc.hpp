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

        // 标记自身并入队；子对象遍历延后到 drainMarkQueue（显式工作队列），
        // 深对象图不再消耗宿主线程栈。守卫语义与旧递归版一致：对象图可能有环，
        // 已标记节点不得重复入队，否则遍历不终止。
        virtual void marked() noexcept {
            if(!_marked) {
                _marked = true;
                markQueue().push_back(this);
            }
        }

        // 枚举子对象：仅在自身已标记、由工作队列弹出时调用（无并发、无重入）。
        // 子对象经 marked()（基类）标记并入队；默认无子对象。
        virtual void markChildren() noexcept {}

        // 标记工作队列：仅由 VM 安全点的 collect/drainMarkQueue 访问（单线程协议，
        // 见 MarkSweep::collect 注释），每线程一份避免多 Runtime 串扰
        static Vec<MarkSweepHeader *> &markQueue() noexcept {
            static thread_local Vec<MarkSweepHeader *> queue;
            return queue;
        }

    protected:
        // 派生类 markChildren() 遍历子对象；漏调基类标记语义由基类 marked() 承担，
        // 派生类不再自行判守卫
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

        // 排空标记队列：弹出已标记节点并枚举其子对象，子对象经 marked() 再入队，
        // 直至队列为空。标记集合与旧递归遍历一致，仅访问顺序不同（对 sweep 与
        // 弱引用不可观察）；队列峰值 ≤ 活跃对象数（每项一个指针）
        static void drainMarkQueue() noexcept {
            auto &queue = MarkSweepHeader::markQueue();
            while(!queue.empty()) {
                MarkSweepHeader *node = queue.back();
                queue.pop_back();
                node->markChildren();
            }
        }

        void collect(const MarkFunc &mark) {
            mark();
            drainMarkQueue();
            // 先快照所有不可达对象，再标记队列；对象之间的环不能使另一个
            // 待终结对象被遗漏。回调仅由 VM 安全点执行，绝不在 sweep 重入。
            const size_t first = pendingFinalizers.size();
            for(auto *node = _head; node; node = node->_next)
                if(!node->_isFree && !node->_marked && node->needsFinalization())
                    pendingFinalizers.push_back(node);
            for(size_t i = first; i < pendingFinalizers.size(); ++i)
                pendingFinalizers[i]->marked();
            drainMarkQueue();
            sweep();
        }

        // 无空闲节点时触发 collect，终结对象占满残余节点时扩池；仍无可分配
        // 节点则抛出 std::runtime_error，绝不返回空指针
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
