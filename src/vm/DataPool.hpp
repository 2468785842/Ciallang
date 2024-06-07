/*
 * Copyright (c) 2024/5/30 下午6:18
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

#include "Memory.hpp"

namespace Ciallang::VM {
    template <typename VT>
    struct DataPool {
        size_t capacity = 8;
        VT* dataPool{
                Memory::growArray(static_cast<VT*>(nullptr), 0, 8)
        };

        void writeData(VT&& value) {
            if(capacity < _count + 1) {
                int oldCapacity = capacity;
                capacity = Memory::growCapacity(oldCapacity);
                dataPool = Memory::growArray(dataPool,
                    oldCapacity, capacity);
                addressHeader = dataPool;
            }

            // need call constructor
            new(addressHeader + _count) VT(std::forward<VT>(value));
            _count++;
        }

        virtual ~DataPool() noexcept {
            Memory::freeArray(addressHeader, capacity);
        }

        virtual void reset() {
            Memory::freeArray(addressHeader, capacity);

            _count = 0;
            capacity = 8;
            dataPool = Memory::growArray(
                static_cast<VT*>(nullptr),
                _count, capacity
            );
            addressHeader = dataPool;
        }

        [[nodiscard]] VT* baseAddress() const noexcept { return addressHeader; }

        [[nodiscard]] size_t count() const noexcept { return _count; }

    private:
        size_t _count = 0;
        // 指向开始的地址,不然改变 dataPool 无法释放内存
        VT* addressHeader = dataPool;
    };
}
