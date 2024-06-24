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
     * GC will not call destructor method.
     * all fields must manage for gc. don't memory manage for self
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

        virtual std::optional<std::vector<GCObject*>> getFields() const {
            return {};
        };

        // uint8_t is memory area,
        // warn!! caller considers whether enough memory
        virtual GCObject* copyTo(uint8_t*) = 0;

        virtual size_t size() const noexcept = 0;

        virtual ~GCObject() = default;

    private:
        bool _remembered{};

        size_t _age{};

        bool _forwarded{};

        bool _marked{};
    };
}
