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
        if(!_inGCList) {
            _inGCList = true;
            GC::instance().track(this);
        }
    }

    void GCObject::addChild(GCObject *pChild) {
        CLL_ASSERT(pChild != nullptr, "GC child is null");
        _children.push_back(pChild);
        if(pChild)
            pChild->incRef();
    }

    void GC::track(GCObject *o) {
        _candidates.push_back(o);
        if(_candidates.size() >= G_Threshold) {
            collect();
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

        // Step 1: 遍历整个循环，把循环内所有对象放入 visited
        while(!stk.empty()) {
            auto obj = stk.top();
            stk.pop();

            if(!obj || visited.contains(obj))
                continue;
            visited.insert(obj);

            // push children
            for(auto child : obj->_children) {
                if(child && !visited.contains(child)) {
                    stk.push(child);
                }
            }
        }

        // Step 2: 销毁循环内对象
        for(const auto &obj : visited) {
            if(!obj)
                continue;

            // 拷贝 children 避免 delete 后访问
            auto children = obj->_children;
            obj->_children.clear();

            // 不调用 decRef，不依赖 refCount
            for(const auto &child : children) {
                if(child) {
                    // 可选：减去循环内部引用计数，但不要触发 delete
                    if(visited.contains(child))
                        child->_refCount--;
                }
            }

            CLL_LOG_DEBUG("Destroying object 0x%llx", obj);
            delete obj;
        }
    }
} // namespace Ciallang