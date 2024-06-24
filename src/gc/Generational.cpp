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
        _majorGC = new MarkSweep{ size - newSize, _heap + newSize };
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
        const auto& [used, capacity] = _majorGC->memoryInfo();
        static constexpr size_t TABLE_SIZE = 60;
        static constexpr size_t TABLE_COL_SIZE = TABLE_SIZE >> 2;

        fmt::print(
            "+{0:-^{3}}+\n"
            "|{2: ^{3}}|\n"
            "+{0:-^{1}}+{0:-^{7}}+{0:-^{8}}+\n"
            "|{4: ^{1}}|{5: ^{7}}|{0: ^{8}}|\n"
            "+{0:-^{1}}+{0:-^{14}}+{0:-^{14}}+{0:-^{8}}+{6: ^{8}}+\n"
            "|{9: ^{1}}|{10: ^{14}}|{11: ^{14}}|{12: ^{8}}|{13: ^{8}}|\n"
            "+{0:-^{1}}+{0:-^{14}}+{0:-^{14}}+{0:-^{8}}+{0:-^{8}}+\n"
            "|{15: ^{1}}|{16: ^{14}}|{17: ^{14}}|{18: ^{8}}|{19: ^{8}}|\n"
            "|{20: ^{1}}|{21: ^{14}}|{22: ^{14}}|{23: ^{8}}|{24: ^{8}}|\n"
            "|{25: ^{1}}|{26: ^{14}}|{27: ^{14}}|{28: ^{8}}|{29: ^{8}}|\n"
            "|{30: ^{1}}|{31: ^{14}.2F}|{32: ^{14}.2F}|{33: ^{8}}|{34: ^{8}.2F}|\n"
            "+{0:-^{3}}+\n",
            "", 15, "Heap Usage", TABLE_SIZE + 15,                                                               // 0 ~ 3
            "Generation", "New", "Old", TABLE_COL_SIZE * 3 - 2, TABLE_COL_SIZE,                                 // 4 ~ 8
            "Space", "Eden", "From", "To", "", TABLE_COL_SIZE - 2,                                              // 9 ~ 14
            "Capacity", _edenSize, _survivorSize, _survivorSize, capacity,                                  // 15 ~ 19
            "Used", _nextFreeOffset, _nextForwardingOffset, 0, used,                                        // 20 ~ 24
            "Free", _edenSize - _nextFreeOffset, _survivorSize - _nextForwardingOffset, 0, capacity - used, // 25 ~ 29
            "Used(%)", static_cast<double>(_nextFreeOffset) / _edenSize * 100,
            static_cast<double>(_nextForwardingOffset) / _survivorSize * 100, 0,
            static_cast<double>(used) / capacity * 100 // 29 ~ 33
        );
    }
}
