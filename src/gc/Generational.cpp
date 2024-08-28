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
#include "Generational.hpp"

#include "common/TableFormatter.hpp"

namespace Ciallang::GC {
    Generational::Generational(
        const size_t size
    ) : _heap(static_cast<uint8_t*>(malloc(size))),
        _heapSize(size) {
        size_t newSize = size / 10 * NEW_RATIO;
        _edenSize = newSize / 10 * SURVIVOR_RATIO;
        _survivorSize = newSize / 10;

        _eden = _heap;
        _from = _eden + _edenSize;
        _to = _from + _survivorSize;
        auto tail = MarkSweep::initFreeList(
            MarkSweep::resolveHeapSize(size - newSize),
            _heap + newSize
        );
        _majorGC = new MarkSweep{
                reinterpret_cast<Cell*>(_heap + newSize), tail, _rootsSet
        };
    }

    Generational::~Generational() {
        delete _majorGC;

        for(auto& roots : _rootsSet) {
            delete roots;
        }

        for(auto& obj : _permHeap) {
            delete obj;
        }

        free(_heap);
    }

    void Generational::minorGC() {
        LOG_EVERY_T(INFO, 5) << "MinorGC(CopyingGC) Running";

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
                    if(reinterpret_cast<uintptr_t>(field) < _majorGC->start()) {
                        copy(field);
                        hasNewObj = reinterpret_cast<uintptr_t>(field) < _majorGC->start();
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

        memset(_eden, 0, _edenSize);
        memset(_from, 0, _survivorSize);

        swap(_from, _to);
    }

    void Generational::copy(GCObject*& obj) {
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

        handleFields(obj, [&](GCObject*& field) {
            copy(field);
        });
    }

    void Generational::promotion(GCObject*& obj) {
        _majorGC->reallocate(obj);

        obj->forwarded(true);

        handleFields(obj, [&](GCObject*& field) {
            if(reinterpret_cast<uintptr_t>(field) < _majorGC->start()) {
                obj->remembered(true);
                _rememberedSet.push_back(obj);
            }
        });
    }

    void Generational::printState() const {
        static constexpr auto TABLE_SIZE = 60;
        static constexpr auto COL_COUNT = 5;

        size_t edenFree = _edenSize - _nextFreeOffset;
        size_t survivorFree = _survivorSize - _nextForwardingOffset;

        size_t oldSpaceCapacity = _heapSize - _edenSize - (_survivorSize << 1);
        size_t oldSpaceUsed = _majorGC->end() - _majorGC->free();
        size_t oldSpaceFree = oldSpaceCapacity - oldSpaceUsed;


        const std::any table[][COL_COUNT] = {
                { "Heap Usage", "Heap Usage", "Heap Usage", "Heap Usage", "Heap Usage" },
                {},
                { "Generation", "New", "New", "New", "Old" },
                {},
                { "Space", "Eden", "From", "To", "Old" },
                {},
                { "Capacity", _edenSize, _survivorSize, _survivorSize, oldSpaceCapacity },
                { "Used", _nextFreeOffset, _nextForwardingOffset, 0, oldSpaceUsed },
                { "Free", edenFree, survivorFree, 0, oldSpaceFree },
                { "Used(%)",
                  static_cast<double>(_nextFreeOffset) / _edenSize * 100,
                  static_cast<double>(_nextForwardingOffset) / _survivorSize * 100,
                  0.00,
                  static_cast<double>(oldSpaceUsed) / oldSpaceCapacity * 100
                }
        };
        static Common::TableFormatter formatter{ TABLE_SIZE, COL_COUNT };
        formatter.parse(table);
        formatter.println();
    }
}
