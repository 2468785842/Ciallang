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

#include <unordered_set>
#include "pch.h"

namespace Ciallang {

    class GCObject {
    public:
        GCObject() : _refCount(0), _inGCList(false) {}
        virtual ~GCObject() = default;

        void incRef();

        void decRef();

        [[nodiscard]] size_t getRefCount() const { return _refCount; }

        void addChild(GCObject *pChild);

    private:
        uint32_t _refCount;
        bool _inGCList;
        std::vector<GCObject *> _children;

        friend class GC;
    };

    class GC {
    public:
        static inline size_t G_Threshold = 5000;
        static GC &instance() {
            static GC gc;
            return gc;
        }

        /**
         * track gc obj
         * @param o gc obj
         */
        void track(GCObject *o);

        /**
         * Dealing with circular references
         */
        void collect();

    private:
        std::vector<GCObject *> _candidates;

        // 递归销毁
        static void destroyCycle(GCObject *root, std::unordered_set<GCObject *> &visited);
    };

    // 类型 trait
    template <typename T>
    static constexpr bool is_gc_object_v = std::is_base_of_v<GCObject, T>;

} // namespace Ciallang
