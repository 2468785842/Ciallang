/*
 * Copyright (c) 2025/12/24 上午8:08
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

#include "Runtime.hpp"

namespace cial {

    void Runtime::addHandleVal(MarkSweepHeader *ptr) { handles.push_back(ptr); }

    void Runtime::removeHandleVal(const MarkSweepHeader *ptr) {
        for(auto it = handles.rbegin(); it != handles.rend(); ++it) {
            if(*it == ptr) {
                *it = handles.back();
                handles.pop_back();
                return;
            }
        }
    }

} // namespace cial