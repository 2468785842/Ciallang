/*
 * Copyright (c) 2024/5/29 下午9:07
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

#include <exception>

namespace Ciallang::VM::Memory {
    static constexpr size_t growCapacity(const size_t capacity) {
        return capacity < 8 ? 8 : capacity * 2;
    }

    static void* reallocate(void* pointer, const size_t oldSize, const size_t newSize) {
        if(newSize == 0) {
            free(pointer);
            return nullptr;
        }

        void* result = realloc(pointer, newSize);
        if(result == nullptr) throw std::bad_alloc();

        return result;
    }

    template <typename T>
    static T* growArray(T* pointer, const size_t oldCount, const size_t newCount) {
        return static_cast<T*>(reallocate(pointer, sizeof(T) * oldCount,
            sizeof(T) * newCount));
    }

    template <typename T>
    static T* freeArray(T* pointer, const size_t oldCount) {
        return static_cast<T*>(reallocate(pointer, sizeof(T) * oldCount, 0));
    }
}
