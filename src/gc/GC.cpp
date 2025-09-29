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
        std::unordered_map<GCObject *, size_t> rc;

        for(const auto &o : _candidates)
            rc[o] = o->_refCount;

        for(const auto &o : _candidates) {
            for(const auto &child : o->_children) {
                if(rc.contains(child))
                    --rc[child];
            }
        }

        // 3. rc==0 的是垃圾循环
        std::vector<GCObject *> garbage{ rc.size() };
        for(auto &[obj, cnt] : rc) {
            if(cnt == 0)
                garbage.push_back(obj);
        }

        std::unordered_set<GCObject *> visited{ garbage.size() };
        for(const auto &g : garbage)
            destroyCycle(g, visited);

        for(const auto &o : _candidates)
            o->_inGCList = false;

        _candidates.clear();
    }

    void GC::destroyCycle(GCObject *root, std::unordered_set<GCObject *> &visited) {
        if(!root)
            return;

        std::stack<GCObject *> stk;
        stk.push(root);

        while(!stk.empty()) {
            GCObject *obj = stk.top();
            stk.pop();

            if(!obj || visited.contains(obj))
                continue;
            visited.insert(obj);

            for(auto child : obj->_children) {
                if(child && !visited.contains(child)) {
                    stk.push(child);
                }
            }

            // 清理 children 引用计数
            for(const auto &child : obj->_children) {
                if(child)
                    child->decRef();
            }
            obj->_children.clear();

            CLL_LOG_DEBUG("Destroying object 0x%llx", obj);
            delete obj;
        }
    }
} // namespace Ciallang