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

        _majorGC = new MarkSweep{
                MarkSweep::initFreeList(
                    MarkSweep::resolveHeapSize(size - newSize),
                    _heap + newSize
                )
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

    void Generational::printState() const {
        static constexpr auto TABLE_SIZE = 60;
        static constexpr auto COL_COUNT = 5;

        int oldSpaceCapacity = _heapSize - _edenSize - (_survivorSize << 1);
        int oldSpaceUsed = _majorGC->freeHeap() - _majorGC->heap();
        int oldSpaceFree = oldSpaceCapacity - oldSpaceUsed;

        int edenFree = _edenSize - _nextFreeOffset;
        int survivorFree = _survivorSize - _nextForwardingOffset;

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
