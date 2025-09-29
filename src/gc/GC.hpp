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

#include "pch.h"

namespace Ciallang::GC {
    /**
     * GC doesn't call destructor method.
     * All fields must be managed for gc.
     * Don't memory manage itself
     */
    class GCObject {
    public:
        bool remembered() const noexcept { return _remembered; }
        void remembered(const bool remembered) noexcept { _remembered = remembered; }

        size_t age() const noexcept { return _age; }
        void age(const size_t age) noexcept { _age = age; }
        void ageIncrement() noexcept { _age++; }

        bool forwarded() const noexcept { return _forwarded; }
        void forwarded(const bool forwarded) noexcept { _forwarded = forwarded; }

        bool marked() const noexcept { return _marked; }
        void marked(const bool marked) noexcept { _marked = marked; }

        virtual std::vector<GCObject*> getFields() const = 0;

        /**
         * It is called when the gc needs to move an object,
         * Each gc call will pass in a size() of memory
         *
         * Warning!!
         * Don't call methods yourself
         *
         * This is basically a fixed way of writing:
         *
         * return new(to) Emp{ *this };
         *
         */
        virtual GCObject* copyTo(uint8_t*) = 0;

        /**
         * The Object size
         *
         * Usually writing with:
         *
         * return sizeof(Emp);
         */
        virtual size_t size() const noexcept = 0;

        /**
         * take care memory leak
         * Maybe never call desturctor method
         */
        virtual ~GCObject() = default;

    private:
        bool _remembered{};

        size_t _age{};

        bool _forwarded{};

        bool _marked{};
    };

    using Roots = std::vector<GCObject*>;


    template <typename T>
    static constexpr bool is_gc_object_v = std::is_base_of_v<GCObject, T>;
}
