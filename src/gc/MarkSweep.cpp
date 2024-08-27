/*
 * Copyright (c) 2024/6/23 下午8:13
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
#include "MarkSweep.hpp"

namespace Ciallang::GC {
    MarkSweep::MarkSweep(Cell* heap,
                         Cell* tail,
                         std::vector<Roots*>& rootsSet) :
        _rootsSet(rootsSet),
        _head(heap),
        _tail(tail) {
        _nextFree = _head;
    }

    void MarkSweep::collect() {
        LOG_EVERY_T(INFO, 5) << "MarkSweepGC Running";
        for(auto& roots : _rootsSet) {
            for(auto& obj : *roots) {
                if(obj && reinterpret_cast<uintptr_t>(obj) >= start())
                    mark(obj);
            }
        }
        sweep();
    }

    Cell* MarkSweep::findIdleNode() {
        auto find = [&] {
            _nextFree = _head;
            while(_nextFree && _nextFree->data) {
                _nextFree = _nextFree->next;
            }
        };

        find();
        if(!_nextFree) collect();
        find();

        if(!_nextFree) {
            LOG(FATAL) << "Allcation Failed! OutOfMemory...";
        }

        return _nextFree;
    }


    void MarkSweep::sweep() {
        for(Cell* cursor = _head; cursor; cursor = cursor->next) {
            if(!cursor->data) continue;

            GCObject* obj = cursor->data;

            if(obj->marked()) obj->marked(false);
            else {
                Cell* cell = reinterpret_cast<Cell*>(obj - 1);

                LOG(INFO) << fmt::format("collection: {}B", obj->size());

                // fill memory with zero
                memset((void*)obj, 0, obj->size());
                cell->data = nullptr;

                _nextFree = cell;
            }
        }
    }

    void MarkSweep::mark(GCObject* obj) {
        if(!obj || obj->marked()) return;
        obj->marked(true);

        auto fields = obj->getFields();
        if(!fields.has_value()) return;
        for(auto& field : fields.value()) {
            if(reinterpret_cast<uintptr_t>(field) >= start())
                mark(field);
        }
    }
} // Ciallang
