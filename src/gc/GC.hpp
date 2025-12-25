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

#include <ranges>

#include "vm/Runtime.hpp"

namespace Cial {

    class GCObject {
        friend class MarkSweep;

    public:
        GCObject() = default;
        virtual ~GCObject() = default;

        void incRef();
        void decRef();

        virtual Opt<Vec<GCObject *>> getRefs() = 0;

    private:
        std::uint32_t _refCount{ 0 };
        bool _marked{ false };
        GCObject *_next{ nullptr };
    };

    GCObject *toGCObject(const Value &v) noexcept;

    // 类型 trait
    template <typename T>
    static constexpr bool is_gc_object_v = std::is_base_of_v<GCObject, T>;

    static constexpr size_t NODE_SIZE = 128; // Byte

    class MarkSweep {
    public:
        explicit MarkSweep(const size_t size, Runtime &rt) : _roots(rt), _head(initFreeList(resolveHeapSize(size))) {
            _nextFree = _head;
        }

        ~MarkSweep() {
            for(GCObject *cursor = _head; cursor;) {
                GCObject *nextGCObject = cursor->_next;
                free(cursor);
                cursor = nextGCObject;
            }
        }

        void collect() {
            for(const auto &v : _roots.gObj | std::views::values) {
                if(GCObject *gcObj = toGCObject(v))
                    mark(gcObj);
            }
            sweep();
        }

        template <typename T, typename... Args>
            requires std::is_base_of_v<GCObject, T>
        T *allocate(Args &&...args) {
            if(!_nextFree || _nextFree->_refCount > 0) {
                findIdleNode();
            }

            GCObject *next = _nextFree->_next;
            T *newObj = new(_nextFree) T(std::forward<Args>(args)...);
            newObj->_marked = false;
            newObj->_refCount = 0;
            newObj->_next = next;

            _nextFree = _nextFree->_next;
            return newObj;
        }

        GCObject *findIdleNode();

        void sweep();

        // Byte Unit
        [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> memoryInfo() const;

    private:
        Runtime &_roots;
        GCObject *_nextFree{ nullptr };
        GCObject *_head{ nullptr };

        static constexpr size_t resolveHeapSize(const size_t size) {
            if(size < NODE_SIZE) {
                return NODE_SIZE;
            }
            return size / NODE_SIZE * NODE_SIZE;
        }

        static GCObject *initFreeList(const size_t freeListSize) {
            GCObject *head{ nullptr };
            for(int i = 0; i < freeListSize; ++i) {
                auto *gcObj = static_cast<GCObject *>(malloc(NODE_SIZE));
                gcObj->_refCount = 0;
                gcObj->_marked = false;
                gcObj->_next = head;
                head = gcObj;
            }

            return head;
        }

        static void mark(GCObject *obj) {
            if(!obj || obj->_marked)
                return;
            obj->_marked = true;

            const auto refs = obj->getRefs();
            if(!refs)
                return;

            for(const auto &field : *refs) {
                mark(field);
            }
        }
    };

} // namespace Cial
