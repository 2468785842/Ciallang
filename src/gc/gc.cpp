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

#include "gc.hpp"

#include "logging/Logger.hpp"

namespace cial {

    MarkSweepHeader *MarkSweep::findIdleNode(const MarkFunc &mark) {
        find();
        if(!_nextFree)
            collect(mark);
        find();

        if(!_nextFree) {
            auto [used, total] = memoryInfo();
            CLL_LOG_FATAL("Allocation Failed! OutOfMemory(used/total): %d/%d(B)", used, total);
        }

        return _nextFree;
    }

    void MarkSweep::sweep() {
        for(MarkSweepHeader *cursor = _head; cursor; cursor = cursor->_next) {
            if(cursor->_isFree)
                continue;

            if(cursor->_marked)
                cursor->_marked = false;
            else {
                cursor->_isFree = true;
                cursor->_marked = false;
                cursor->~MarkSweepHeader();
                _nextFree = cursor;
            }
        }
    }

    std::pair<std::uint32_t, std::uint32_t> MarkSweep::memoryInfo() const {
        std::uint32_t used{ 0 };
        std::uint32_t total{ 0 };
        for(const MarkSweepHeader *cursor = _head; cursor; cursor = cursor->_next) {
            if(!cursor->_isFree) {
                used += NODE_SIZE;
            }
            total += NODE_SIZE;
        }

        return { used, total };
    }

} // namespace cial