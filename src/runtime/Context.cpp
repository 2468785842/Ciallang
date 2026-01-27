//
// Created by LiDong on 2026/1/15.
//
#include "Context.hpp"

#include "AtomTable.hpp"
#include "stdlib/StringLib.hpp"
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

        explicit Impl(Runtime &rt) noexcept : rt(rt) {}
    };

    Context::Context(Runtime &rt) noexcept : _impl(new Impl{ rt }) {}
    Context::~Context() noexcept { delete _impl; }

    void Context::initNativeMethod() {
        registerMethod<String>("charAt"_str, &stdlib::stringCharAt);
        registerMethod<String>("indexOf"_str, &stdlib::stringIndexOf);
        registerMethod<String>("toUpperCase"_str, &stdlib::stringToUpperCase);
        registerMethod<String>("toLowerCase"_str, &stdlib::stringToLowerCase);
        registerMethod<String>("substring"_str, &stdlib::stringSubstring);
        registerMethod<String>("substr"_str, &stdlib::stringSubstring);
        registerMethod<String>("sprintf"_str, &stdlib::stringSprintf);
        // registerMethod<String>("replace"_str, &stdlib::stringReplace);
        registerMethod<String>("escape"_str, &stdlib::stringEscape);
        // registerMethod<String>("split"_str, &stringSplit);
        registerMethod<String>("trim"_str, &stdlib::stringTrim);
        registerMethod<String>("reverse"_str, &stdlib::stringReverse);
        registerMethod<String>("repeat"_str, &stdlib::stringRepeat);
    }

    void Context::collectMark() const {
        _impl->rt.collectMark();
        _impl->globalObject.marked();

        for(const auto &[instanceMethods] : _impl->nativeRegister.tables | std::views::values) {
            for(const auto &method : instanceMethods | std::views::values) {
                method->marked();
            }
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
            if(const Value *val = _impl->regPool.ptrAt(i)) {
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

    void Context::registryGlobalFunc(const String &name, NativeFunction *func) const noexcept {
        const Atom a = _impl->rt.atomTable.intern(name);
        _impl->globalObject.setProp(a, Value{ func });
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