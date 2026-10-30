//
// Created by LiDong on 2026/1/15.
//

#include "Context.hpp"


#include "AtomTable.hpp"
#include "types/Class.hpp"
#include "vm/CallFrame.hpp"

namespace cial {

    struct Context::Impl {
        Runtime &rt;
        PreProcessor pp{};
        GlobalObject globalObject{ &rt };
        NativeRegister nativeRegister{ rt };

        vm::FastRegisterPool regPool{};

        CallFrame callStack[maxCallDepth];

        size_t stackTop{ 0 }; // callFrame count

        explicit Impl(Runtime &rt) : rt(rt) {}
    };

    Context::Context(Runtime &rt) : MarkRootProvider(), _impl(new Impl{ rt }) {
        try {
            rt.addMarkRoot(this);
        } catch(...) {
            delete _impl;
            throw;
        }
    }

    Context::~Context() noexcept {
        _impl->rt.removeMarkRoot(this);
        delete _impl;
    }

    void Context::collectMark() const {
        _impl->rt.collectMark();
        _impl->globalObject.marked();
        // The global object lives outside the sweep pool; its mark bit is not
        // cleared by sweep, so retain its proxy explicitly on every collection.
        if(_impl->globalObject.proxy)
            _impl->globalObject.proxy->marked();

        for(const auto &[instanceMethods, accessors, instanceProperties] :
            _impl->nativeRegister.tables | std::views::values) {
            for(const auto &method : instanceMethods | std::views::values) {
                method->marked();
            }
            for(const auto &[getter, setter] : accessors | std::views::values) {
                if(getter)
                    getter->marked();
                if(setter)
                    setter->marked();
            }
            // instanceProperties 是原生函数指针（非 GC 对象），无需标记
        }

        for(const auto &val : _impl->globalObject.props() | std::views::values) {
            if(val.isObject()) {
                if(auto *msHeader = val.asObject().value()) {
                    msHeader->marked();
                    if(msHeader->_name != ATOM_INVALID) {
                        _impl->rt.atomTable.get(msHeader->_name)->marked();
                    }
                }
            }
        }

        // scan stack
        const u32 used = _impl->regPool.used();
        for(u32 i = 0; i < used; ++i) {
            // 槽位可能是任意类型（整数/字符串/void），必须先判类型再取对象指针
            if(const Value *val = _impl->regPool.ptrAt(i); val->isObject()) {
                if(auto *msHeader = val->asObject().value()) {
                    msHeader->marked();
                    if(msHeader->_name != ATOM_INVALID) {
                        _impl->rt.atomTable.get(msHeader->_name)->marked();
                    }
                }
            }
        }

        // call frame
        for(u32 i = 0; i < _impl->stackTop; ++i) {
            const CallFrame &callFrame = _impl->callStack[i];
            if(callFrame.funcMeta)
                callFrame.funcMeta->marked();
            else
                callFrame.chunk->marked();
            if(callFrame.thisObj.isObject())
                callFrame.thisObj.asObject().value()->marked();
        }
    }

    void Context::registerGlobal(const String &name, const Value &value) const {
        const Atom a = _impl->rt.atomTable.intern(name);
        _impl->globalObject.setProp(a, value);
    }

    void Context::registryGlobalFunc(const String &name, NativeFunction *func) const {
        registerGlobal(name, Value{ func });
    }

    NativeRegister &Context::nativeRegister() { return _impl->nativeRegister; }

    const NativeRegister &Context::nativeRegister() const { return _impl->nativeRegister; }

    CallFrame *Context::callStack() const { return _impl->callStack; }

    size_t &Context::stackTop() const { return _impl->stackTop; }

    Runtime &Context::rt() const { return _impl->rt; }

    vm::FastRegisterPool &Context::regPool() const { return _impl->regPool; }

    GlobalObject *Context::global() const { return &_impl->globalObject; }

    PreProcessor &Context::pp() const { return _impl->pp; }
} // namespace cial
