//
// Created by LiDong on 2026/1/15.
//
#include "Context.hpp"

#include "AtomTable.hpp"
#include "stdlib/ArrayLib.hpp"
#include "stdlib/DictLib.hpp"
#include "stdlib/StringLib.hpp"
#include "types/Array.hpp"
#include "types/Dictionary.hpp"
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

    Context::Context(Runtime &rt) noexcept : MarkRootProvider(), _impl(new Impl{ rt }) { rt.addMarkRoot(this); }

    Context::~Context() noexcept {
        _impl->rt.removeMarkRoot(this);
        delete _impl;
    }

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

        // 只读属性（str.length；与 charAt 一致的字节长度语义）
        registerProperty<String>("length"_str, [](const Value &thisObj) {
            return Value{ Integer{ thisObj.asString().unwrap()->length() } };
        });

        // Array 方法
        nativeRegister().registerMethod(TypeId::Array, _impl->rt.atomTable.intern("push"_str), &stdlib::arrayPush);
        nativeRegister().registerMethod(TypeId::Array, _impl->rt.atomTable.intern("pop"_str), &stdlib::arrayPop);
        nativeRegister().registerMethod(TypeId::Array, _impl->rt.atomTable.intern("unshift"_str), &stdlib::arrayUnshift);
        nativeRegister().registerMethod(TypeId::Array, _impl->rt.atomTable.intern("shift"_str), &stdlib::arrayShift);
        nativeRegister().registerMethod(TypeId::Array, _impl->rt.atomTable.intern("insert"_str), &stdlib::arrayInsert);
        nativeRegister().registerMethod(TypeId::Array, _impl->rt.atomTable.intern("remove"_str), &stdlib::arrayRemove);
        nativeRegister().registerMethod(TypeId::Array, _impl->rt.atomTable.intern("erase"_str), &stdlib::arrayErase);
        nativeRegister().registerMethod(TypeId::Array, _impl->rt.atomTable.intern("reverse"_str), &stdlib::arrayReverse);
        nativeRegister().registerMethod(TypeId::Array, _impl->rt.atomTable.intern("join"_str), &stdlib::arrayJoin);
        // count/length 属性（get；写走 Array::setProp）
        nativeRegister().registerProperty(TypeId::Array, _impl->rt.atomTable.intern("count"_str), [](const Value &thisObj) {
            return Value{ Integer{ static_cast<Integer>(dynamic_cast<Array &>(*thisObj.asObject().unwrap()).elements.size()) } };
        });
        nativeRegister().registerProperty(TypeId::Array, _impl->rt.atomTable.intern("length"_str), [](const Value &thisObj) {
            return Value{ Integer{ static_cast<Integer>(dynamic_cast<Array &>(*thisObj.asObject().unwrap()).elements.size()) } };
        });

        // Dictionary 方法
        nativeRegister().registerMethod(TypeId::Dictionary, _impl->rt.atomTable.intern("assign"_str), &stdlib::dictAssign);
        nativeRegister().registerMethod(TypeId::Dictionary, _impl->rt.atomTable.intern("clear"_str), &stdlib::dictClear);
        nativeRegister().registerMethod(TypeId::Dictionary, _impl->rt.atomTable.intern("erase"_str), &stdlib::dictErase);
        // count 属性
        nativeRegister().registerProperty(
            TypeId::Dictionary, _impl->rt.atomTable.intern("count"_str), [](const Value &thisObj) {
                return Value{ Integer{ static_cast<Integer>(
                    dynamic_cast<Dictionary &>(*thisObj.asObject().unwrap()).members.size()) } };
            });

        // Array/Dictionary "类对象"注册到全局：承载静态方法命名空间（语料惯用法
        // Dictionary.assign incontextof obj——取未绑定方法后 ChgThis 换 this）
        _impl->globalObject.setProp(_impl->rt.atomTable.intern("Array"_str),
                                    Value{ _impl->rt.create<Array>(&_impl->rt).get() });
        _impl->globalObject.setProp(_impl->rt.atomTable.intern("Dictionary"_str),
                                    Value{ _impl->rt.create<Dictionary>().get() });
    }

    void Context::collectMark() const {
        _impl->rt.collectMark();
        _impl->globalObject.marked();

        for(const auto &[instanceMethods, instanceProperties] : _impl->nativeRegister.tables | std::views::values) {
            for(const auto &method : instanceMethods | std::views::values) {
                method->marked();
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
            const Value *val = _impl->regPool.ptrAt(i);
            // 槽位可能是任意类型（整数/字符串/void），必须先判类型再取对象指针
            if(val->isObject()) {
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