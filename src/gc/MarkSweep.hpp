/*
 * Copyright (c) 2024/6/23 下午8:13
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

#include "GC.hpp"

namespace Ciallang::GC {
    static constexpr size_t NODE_SIZE = 128; // Byte

    struct Cell {
        Cell* next{ nullptr };
        GCObject* data{ nullptr };
    };

    class MarkSweep {
    public:
        explicit MarkSweep(Cell* heap, Cell* tail, std::vector<Roots*>& rootsSet);

        void collect();

        template <typename T, typename... Args>
            requires is_gc_object_v<T>
        T* allocate(Args&&... args) {
            if(!_nextFree || !_nextFree->data) {
                findIdleNode();
            }
            Cell* cell = _nextFree;

            T* newObj = new(cell + 1) T(std::forward<Args>(args)...);
            newObj->marked(false);
            newObj->size(sizeof(T));

            cell->data = newObj;
            _nextFree = _nextFree->next;

            return newObj;
        }

        template <typename T>
            requires is_gc_object_v<T>
        void reallocate(T*& obj) {
            if(!_nextFree || _nextFree->data) {
                findIdleNode();
            }
            Cell* cell = _nextFree;

            if(obj->size() > NODE_SIZE) {
                LOG(FATAL) << "object too large";
            }

            auto* newObj = obj->copyTo(reinterpret_cast<uint8_t*>(cell + 1));

            newObj->forwarded(false);
            newObj->marked(false);

            cell->data = newObj;

            _nextFree = _nextFree->next;
            obj = newObj;
        }

        Cell* findIdleNode();

        void sweep();

        void mark(GCObject* obj);

        uintptr_t start() const {
            return reinterpret_cast<uintptr_t>(_head);
        }

        uintptr_t free() const {
            if(!_nextFree) return start();
            return reinterpret_cast<uintptr_t>(_nextFree);
        }

        uintptr_t end() const {
            return reinterpret_cast<uintptr_t>(_tail);
        }

        static constexpr size_t resolveHeapSize(const size_t size) {
            if(size < NODE_SIZE) {
                return NODE_SIZE;
            }
            return size / NODE_SIZE * NODE_SIZE;
        }

        static Cell* initFreeList(const size_t freeListSize, uint8_t* heap) {
            Cell* tail = new(heap) Cell{};
            for(size_t i = 1; i < freeListSize; i += NODE_SIZE) {
                tail->next = new(heap + i) Cell{};
                tail->data = nullptr;

                tail = tail->next;

                tail->next = nullptr;
                tail->data = nullptr;
            }

            return tail;
        }

    private:
        std::vector<Roots*>& _rootsSet;
        Cell* _nextFree{ nullptr };
        Cell* _head{ nullptr };
        Cell* _tail{ nullptr };
    };
} // Ciallang
