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

#include <stack>

#include "GC.hpp"
#include "logging/Logger.hpp"

namespace Ciallang {

    // GCObject 实现
    void GCObject::incRef() { ++_refCount; }

    void GCObject::decRef() {
        if(--_refCount == 0) {
            delete this;
            return;
        }
        // 非零：可能是循环引用，需要 GC 检测
        GC::instance().track(this);
    }

    void GCObject::addChild(GCObject *pChild) {
        CLL_ASSERT(pChild != nullptr, "GC child is null");
        _children.push_back(pChild);
        pChild->incRef();
    }

    void GC::track(GCObject *o) {
        if(!o->_inGCList) {
            o->_inGCList = true;
            _candidates.push_back(o);
            if(_candidates.size() >= G_Threshold) {
                collect();
            }
        }
    }

    void GC::collect() {
        std::unordered_set<GCObject *> visited;

        for(auto obj : _candidates) {
            if(obj && !visited.contains(obj)) {
                destroyCycle(obj, visited);
            }
        }

        _candidates.clear();
    }

    void GC::destroyCycle(GCObject *root, std::unordered_set<GCObject *> &visited) {
        if(!root)
            return;

        std::stack<GCObject *> stk;
        stk.push(root);

        std::vector<GCObject *> cycle_objects; // Objects potentially in a cycle

        // Step 1: Find all objects reachable from root within the potential cycle
        while(!stk.empty()) {
            auto obj = stk.top();
            stk.pop();

            if(!obj || visited.contains(obj))
                continue;
            visited.insert(obj);
            cycle_objects.push_back(obj);

            for(auto child : obj->_children) {
                if(child && !visited.contains(child)) {
                    stk.push(child);
                }
            }
        }

        // Step 2: Perform a "trial deletion" by decrementing reference counts
        // for all internal references within the identified cycle_objects.
        // This simulates breaking the cycle.
        for(const auto obj : cycle_objects) {
            if(!obj)
                continue;
            for(auto child : obj->_children) {
                if(child && visited.contains(child)) { // If child is also part of this cycle
                    child->_refCount--;
                }
            }
        }

        // Step 3: Identify and delete objects that are truly unreferenced (refCount == 0)
        // after the trial deletion. These objects were only referenced by the cycle.
        for(const auto obj : cycle_objects) {
            if(obj && obj->_refCount == 0) {
                CLL_LOG_DEBUG("Destroying object 0x%llx", obj);
                delete obj;
            } else if(obj) {
                // Step 4: For objects that still have refCount > 0, they are externally referenced.
                // We need to restore their reference counts that were decremented in Step 2.
                // And also clear their children to prevent dangling pointers.
                for(auto child : obj->_children) {
                    if(child && visited.contains(child)) {
                        child->_refCount++; // Restore ref count
                    }
                }
                obj->_children.clear(); // Clear children to prevent dangling pointers
            }
        }
    }
} // namespace Ciallang