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

#include "../types/TjsValue.hpp"
#include "Memory.hpp"

namespace Ciallang::VM {

    template <typename VT>
    struct DataPool {
        size_t count = 0;
        size_t capacity = 0;
        VT* dataPool{ nullptr };

        void writeData(VT&& value) {
            if(capacity < count + 1) {
                int oldCapacity = capacity;
                capacity = Memory::growCapacity(oldCapacity);
                dataPool = Memory::growArray(dataPool,
                    oldCapacity, capacity);
            }

            dataPool[count] = std::forward<VT>(value);
            count++;
        }

        virtual ~DataPool() noexcept {
            Memory::freeArray(dataPool, capacity);
        }
    };

}