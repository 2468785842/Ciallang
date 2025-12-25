// Copyright (c) 2025/9/29 14:34
//
// /\  _` \   __          /\_ \  /\_ \
// \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
//  \ \ \/_/_\/\ \  /'__`\  \ \ \  \ \ \   /'__`\  /' _ `\  /'_ `\
//   \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
//    \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
//     \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
//                                                            /\____
//                                                            \_/__/
//

//
// Created by LiDon on 2025/9/29.
//

#include "GC.hpp"

#include "types/Object.hpp"
#include "types/Octet.hpp"
#include "types/String.hpp"

#include "logging/Logger.hpp"

namespace Cial {

    void GCObject::incRef() { ++_refCount; }

    void GCObject::decRef() {
        // MarkSweep may free this portion of memory
        if(_refCount == 0)
            return;
        // MarkSweep owners. We are only calling the destructor. We aren't freeing the memory!
        if(--_refCount == 0)
            this->~GCObject();
    }

    GCObject *MarkSweep::findIdleNode() {
        auto find = [&] {
            _nextFree = _head;
            while(_nextFree && _nextFree->_refCount > 0) {
                _nextFree = _nextFree->_next;
            }
        };

        find();
        if(!_nextFree)
            collect();
        find();

        if(!_nextFree) {
            auto [used, total] = memoryInfo();
            CLL_LOG_FATAL("Allocation Failed! OutOfMemory(used/total): %d/%d", used, total);
        }

        return _nextFree;
    }

    void MarkSweep::sweep() {
        for(GCObject *cursor = _head; cursor; cursor = cursor->_next) {
            if(cursor->_refCount == 0)
                continue;

            if(cursor->_marked)
                cursor->_marked = false;
            else {
                cursor->_refCount = 0;
                cursor->_marked = false;
                cursor->~GCObject();
                // fill memory with zero
                // memset(reinterpret_cast<void*>(cursor + 1), 0, NODE_SIZE - sizeof(GCObject));

                _nextFree = cursor;
            }
        }
    }

    std::pair<std::uint32_t, std::uint32_t> MarkSweep::memoryInfo() const {
        std::uint32_t used{ 0 };
        std::uint32_t total{ 0 };
        for(const GCObject *cursor = _head; cursor; cursor = cursor->_next) {
            if(cursor->_refCount > 0) {
                used += NODE_SIZE;
            }
            total += NODE_SIZE;
        }

        return { used, total };
    }

    GCObject *toGCObject(const Value &v) noexcept {
        if(v.isObject())
            return v.toObject();
        if(v.isString())
            return v.toString();
        if(v.isOctet())
            return v.toOctet();
        return nullptr;
    }

} // namespace Cial