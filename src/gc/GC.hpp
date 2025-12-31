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

namespace Cial {

    class RefCountHeader {

    public:
        RefCountHeader() = default;
        virtual ~RefCountHeader() = default;

        void incRef();
        void decRef();

    private:
        std::uint32_t _refCount{ 0 };
    };

    class MarkSweepHeader {
        friend class MarkSweep;
        friend class Runtime;

    public:
        MarkSweepHeader() = default;
        virtual ~MarkSweepHeader() = default;
        virtual void marked() noexcept {
            if(!_marked)
                _marked = true;
        }

    private:
        MarkSweepHeader *_next{ nullptr };
        bool _marked{ false };
        bool _isFree{ false };
    };

    // 类型 trait
    // template <typename T>
    // static constexpr bool is_gc_object_v = std::is_base_of_v<RefCountHeader, T>;

    static constexpr size_t NODE_SIZE = 128; // Byte

    class MarkSweep {
        friend class Runtime;

    public:
        using MarkFunc = std::function<void()>;
        explicit MarkSweep(const size_t size) : _head(initFreeList(size)) { _nextFree = _head; }

        ~MarkSweep() {
            for(MarkSweepHeader *cursor = _head; cursor;) {
                MarkSweepHeader *nextMarkSweepHeader = cursor->_next;
                free(cursor);
                cursor = nextMarkSweepHeader;
            }
        }

        void collect(const MarkFunc &mark) {
            mark();
            sweep();
        }

        MarkSweepHeader *findIdleNode(const MarkFunc &mark);

        void sweep();

        void find() {
            _nextFree = _head;
            while(_nextFree && !_nextFree->_isFree) {
                _nextFree = _nextFree->_next;
            }
        }

        // Byte Unit
        [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> memoryInfo() const;

    private:
        MarkSweepHeader *_nextFree{ nullptr };
        MarkSweepHeader *_head{ nullptr };

        static MarkSweepHeader *initFreeList(const size_t freeListSize) {
            MarkSweepHeader *head{ nullptr };
            for(int i = 0; i < freeListSize; ++i) {
                auto *gcObj = static_cast<MarkSweepHeader *>(malloc(NODE_SIZE));
                gcObj->_marked = false;
                gcObj->_isFree = true;
                gcObj->_next = head;
                head = gcObj;
            }

            return head;
        }
    };

} // namespace Cial
