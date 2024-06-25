/*
 * Copyright (c) 2024/6/23 下午8:14
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
#include "MarkSweep.hpp"

namespace Ciallang::GC {
    // 新生代与老年代比例, New : Old = 2 : 8
    static constexpr size_t NEW_RATIO = 2;

    // 新生代中 Eden 和 from, to 比例
    // Eden: From : To = 8 : 1 : 1
    static constexpr size_t SURVIVOR_RATIO = 8;

    static constexpr size_t MAX_AGE = 3;

    /**
     * New Gen GC: Copying
     * Old Gen GC: Marked-Sweep
     */
    class Generational {
    public:
        using Roots = std::vector<GCObject*>;

        explicit Generational(size_t size);

        ~Generational();

        void collect() {
            minorGC();
            _majorGC->collect();
        }

        void minorGC() {
            _nextForwardingOffset = 0;

            for(auto& roots : _rootsSet) {
                for(auto& root : *roots) {
                    if(reinterpret_cast<uintptr_t>(root)
                       <= reinterpret_cast<uintptr_t>(_eden + _edenSize + _survivorSize)) {
                        copy(root);
                    }
                }
            }

            auto it = _rememberedSet.begin();
            while(it != _rememberedSet.end()) {
                bool hasNewObj = false;

                auto fields = (*it)->getFields();
                if(fields.has_value()) {
                    for(auto& field : fields.value()) {
                        if(reinterpret_cast<uintptr_t>(field) < _majorGC->heap()) {
                            copy(field);
                            hasNewObj = reinterpret_cast<uintptr_t>(field) < _majorGC->heap();
                        }
                    }
                }

                if(hasNewObj) {
                    (*it)->remembered(false);

                    if(it != _rememberedSet.end() - 1) {
                        swap(*it, _rememberedSet.back());
                    }

                    _rememberedSet.pop_back();
                } else {
                    ++it;
                }
            }

            _nextFreeOffset = 0;

            // for(size_t i = 0; i < _edenSize;) {
            //     auto* obj = reinterpret_cast<GCObject*>(_eden + i);
            //     obj.~GCObject();
            //     i += obj->size();
            // }
            //
            // for(size_t i = 0; i < _survivorSize;) {
            //     auto* obj = reinterpret_cast<GCObject*>(_from + i);
            //     obj.~GCObject();
            //     i += obj->size();
            // }

            memset(_eden, 0, _edenSize);
            memset(_from, 0, _survivorSize);

            swap(_from, _to);
        }

        // copy object from new area to from area
        void copy(GCObject*& obj) {
            if(!obj || obj->forwarded()) return;

            if(obj->age() >= MAX_AGE
               // `to area` is full
               || _nextForwardingOffset + obj->size() > _survivorSize) {
                promotion(obj);
                return;
            }

            auto* forwarding = obj->copyTo(_to + _nextForwardingOffset);
            forwarding->forwarded(false);
            forwarding->remembered(false);
            forwarding->ageIncrement();
            obj->forwarded(true);

            for(auto& ptr : _rememberedSet) {
                if(ptr == obj) {
                    ptr = forwarding;
                    break;
                }
            }

            obj = forwarding;

            _nextForwardingOffset += obj->size();

            auto fields = obj->getFields();
            if(!fields.has_value()) return;
            for(auto& field : fields.value()) {
                copy(field);
            }
        }

        void promotion(GCObject*& obj) {
            _majorGC->reallocate(obj);

            // obj->forwarded(true);

            auto fields = obj->getFields();
            if(!fields.has_value()) return;
            for(auto& field : fields.value()) {
                if(reinterpret_cast<uintptr_t>(field)
                   <= reinterpret_cast<uintptr_t>(_majorGC->heap())) {
                    obj->remembered(true);
                    _rememberedSet.push_back(obj);
                    break;
                }
            }
        }

        template <typename T, typename... Args>
            requires std::is_base_of_v<GCObject, T>
        T* allocateNewSpace(Args... args) {
            //检查是否可以分配
            if(_nextFreeOffset + sizeof(T) > _edenSize) {
                printf("[New] Allocation Failed. execute gc...\n");
                minorGC();
                if(_nextFreeOffset + sizeof(T) > _edenSize) {
                    printf("[New] Allocation Failed! OutOfMemory...\n");
                    abort();
                }
            }

            //分配
            T* newObj = new(_eden + _nextFreeOffset) T(std::forward<Args>(args)...);

            //初始化
            newObj->forwarded(false);
            newObj->marked(false);
            newObj->age(0);

            //分配后, free移动至下一个可分配位置
            _nextFreeOffset += sizeof(T);

            return newObj;
        }

        template <typename T, typename... Args>
            requires std::is_base_of_v<GCObject, T>
        T* allocateOldSpace(Args... args) {
            return _majorGC->allocate<T>(std::forward<Args>(args)...);
        }

        template <typename T, typename... Args>
        T* allocatePermSpace(Args... args) {
            return _permHeap.push_back(new T{ std::forward<T>(args)... });
        }

        template <typename T>
            requires std::is_base_of_v<GCObject, T>
        void updatePtr(GCObject* obj, T** fieldRef, T* newObj) {
            writeBarrier(obj, fieldRef, newObj);
        }

        template <typename T>
            requires std::is_base_of_v<GCObject, T>
        void writeBarrier(GCObject* obj, T** fieldRef, T* newObj) {
            if(reinterpret_cast<uintptr_t>(obj) >= _majorGC->heap()
               && reinterpret_cast<uintptr_t>(newObj)
               <= reinterpret_cast<uintptr_t>(_eden + _edenSize + _survivorSize)
               && !obj->remembered()) {
                _rememberedSet.push_back(obj);
            }
            *fieldRef = newObj;
        }

        void printState() const;

        Roots& allocateRoots() {
            auto* roots = new Roots{};
            _rootsSet.push_back(roots);
            return *roots;
        }

    private:
        std::vector<Roots*> _rootsSet{};
        std::vector<GCObject*> _rememberedSet{};

        std::vector<GCObject*> _permHeap{ 100 };

        uint8_t* _heap;
        uint8_t* _eden;
        uint8_t* _from;
        uint8_t* _to;

        size_t _nextForwardingOffset{};
        size_t _nextFreeOffset{};


        size_t _heapSize{};
        size_t _edenSize{};

        // from 和 to 交替使用,
        // survivorSize 是单个区的 from 或 to 的大小,
        // 不是from + to的大小
        size_t _survivorSize{};

        MarkSweep* _majorGC;

        template <typename T>
        static void swap(T*& from, T*& to) {
            T* temp = from;
            from = to;
            to = temp;
        }
    };
}
