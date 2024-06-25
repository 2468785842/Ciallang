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
        explicit MarkSweep(Cell* heap) :
            _head(heap) {
            _nextFree = _head;
        }

        /*~MarkSweep() {
            for(Cell* cursor = _head; cursor;) {
                Cell* nextCell = cursor->next();
                free(cursor);
                cursor = nextCell;
            }
        }*/

        void collect() {
            for(auto& obj : _roots) {
                if(obj && reinterpret_cast<uintptr_t>(obj)
                   > reinterpret_cast<uintptr_t>(_head))
                    mark(obj);
            }
            sweep();
        }

        template <typename T, typename... Args>
            requires std::is_base_of_v<GCObject, T>
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
            requires std::is_base_of_v<GCObject, T>
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

        Cell* findIdleNode() {
            auto find = [&] {
                _nextFree = _head;
                while(_nextFree && _nextFree->data) {
                    _nextFree = _nextFree->next;
                }
            };

            find();
            if(!_nextFree) collect();
            find();

            if(!_nextFree) {
                std::cerr << "Allcation Failed! OutOfMemory..." << std::endl;
                abort();
            }

            return _nextFree;
        }


        void sweep() {
            for(Cell* cursor = _head; cursor; cursor = cursor->next) {
                if(!cursor->data) continue;

                GCObject* obj = cursor->data;

                if(obj->marked()) obj->marked(false);
                else {
                    Cell* cell = reinterpret_cast<Cell*>(obj - 1);

                    std::cout << "collection: " << obj->size() << 'B' << std::endl;

                    // fill memory with zero
                    // obj->~GCObject();
                    memset((void*)obj, 0, obj->size());
                    cell->data = nullptr;

                    _nextFree = cell;
                }
            }
        }

        void mark(GCObject* obj) {
            if(!obj || obj->marked()) return;
            obj->marked(true);

            auto fields = obj->getFields();
            if(!fields.has_value()) return;
            for(auto& field : fields.value()) {
                if(reinterpret_cast<uintptr_t>(field)
                   >= reinterpret_cast<uintptr_t>(_head))
                    mark(field);
            }
        }

        uintptr_t heap() const { return reinterpret_cast<uintptr_t>(_head); }
        uintptr_t freeHeap() const { return reinterpret_cast<uintptr_t>(_nextFree); }

        static constexpr size_t resolveHeapSize(const size_t size) {
            if(size < NODE_SIZE) {
                return NODE_SIZE;
            }
            return size / NODE_SIZE * NODE_SIZE;
        }

        static Cell* initFreeList(const size_t freeListSize, uint8_t* heap) {
            Cell* head{ nullptr };
            for(int i = 0; i < freeListSize; i += NODE_SIZE) {
                head = new(heap + i) Cell{};
                head->next = head;
                head->data = nullptr;
            }

            return head;
        }

    private:
        std::vector<GCObject*> _roots{ nullptr };
        Cell* _nextFree{ nullptr };
        Cell* _head{ nullptr };
    };
} // Ciallang
