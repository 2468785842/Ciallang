//
// Created by LiDong on 2026/1/15.
//
#define _USE_MATH_DEFINES

#include "Context.hpp"

#include <cmath>

#include "AtomTable.hpp"
#include "stdlib/ArrayLib.hpp"
#include "common/UTF8.hpp"
#include "stdlib/DictLib.hpp"
#include "stdlib/MathLib.hpp"
#include "stdlib/RegExpLib.hpp"
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
        registerMethod<String>("replace"_str, &stdlib::stringReplace);
        registerMethod<String>("split"_str, &stdlib::stringSplitWith);
        registerMethod<String>("escape"_str, &stdlib::stringEscape);
        registerMethod<String>("trim"_str, &stdlib::stringTrim);
        registerMethod<String>("reverse"_str, &stdlib::stringReverse);
        registerMethod<String>("repeat"_str, &stdlib::stringRepeat);

        // 只读属性（str.length；与 charAt 一致的字节长度语义）
        registerProperty<String>("length"_str, [](const Value &thisObj) {
            // TJS2 语义：字符数（UTF-16 码元数）；UTF-8 下按码点计数（"abc漢字" == 5）
            const String *str = thisObj.asString().unwrap();
            return Value{ Integer{ Common::utf8Strlen(std::string(str->getData(), str->length())) } };
        });

        // Array 方法
        registerMethod<Array>("push"_str, &stdlib::arrayPush);
        registerMethod<Array>("pop"_str, &stdlib::arrayPop);
        registerMethod<Array>("unshift"_str, &stdlib::arrayUnshift);
        registerMethod<Array>("shift"_str, &stdlib::arrayShift);
        registerMethod<Array>("insert"_str, &stdlib::arrayInsert);
        registerMethod<Array>("remove"_str, &stdlib::arrayRemove);
        registerMethod<Array>("erase"_str, &stdlib::arrayErase);
        registerMethod<Array>("reverse"_str, &stdlib::arrayReverse);
        registerMethod<Array>("join"_str, &stdlib::arrayJoin);
        registerMethod<Array>("split"_str, &stdlib::arraySplitWith);
        registerMethod<Array>("sort"_str, &stdlib::arraySort);
        // count/length 属性（get；写走 Array::setProp）
        const auto arrayCount = [](const Value &thisObj) {
            return Value{ Integer{ static_cast<Integer>(dynamic_cast<Array &>(*thisObj.asObject().unwrap()).elements.size()) } };
        };
        registerProperty<Array>("count"_str, arrayCount);
        registerProperty<Array>("length"_str, arrayCount);

        // Dictionary 方法
        registerMethod<Dictionary>("assign"_str, &stdlib::dictAssign);
        registerMethod<Dictionary>("clear"_str, &stdlib::dictClear);
        registerMethod<Dictionary>("erase"_str, &stdlib::dictErase);
        // count 属性
        registerProperty<Dictionary>("count"_str, [](const Value &thisObj) {
            return Value{ Integer{ static_cast<Integer>(
                dynamic_cast<Dictionary &>(*thisObj.asObject().unwrap()).members.size()) } };
        });

        // RegExp 方法（TJS2 tjsRegExp.cpp 对齐）；matches/start/index 等状态属性走 RegExp::getProp
        registerMethod<RegExp>("test"_str, &stdlib::regexpTest);
        registerMethod<RegExp>("match"_str, &stdlib::regexpMatch);
        registerMethod<RegExp>("exec"_str, &stdlib::regexpExec);
        registerMethod<RegExp>("compile"_str, &stdlib::regexpCompile);
        registerMethod<RegExp>("replace"_str, &stdlib::regexpReplace);
        registerMethod<RegExp>("split"_str, &stdlib::regexpSplit);

        // Math 静态方法与常量（TJS2 tjsMath.cpp 对齐；参数 AsReal、返回 Real）
        registerMethod<MathNS>("abs"_str, &stdlib::mathAbs);
        registerMethod<MathNS>("acos"_str, &stdlib::mathAcos);
        registerMethod<MathNS>("asin"_str, &stdlib::mathAsin);
        registerMethod<MathNS>("atan"_str, &stdlib::mathAtan);
        registerMethod<MathNS>("atan2"_str, &stdlib::mathAtan2);
        registerMethod<MathNS>("ceil"_str, &stdlib::mathCeil);
        registerMethod<MathNS>("exp"_str, &stdlib::mathExp);
        registerMethod<MathNS>("floor"_str, &stdlib::mathFloor);
        registerMethod<MathNS>("log"_str, &stdlib::mathLog);
        registerMethod<MathNS>("pow"_str, &stdlib::mathPow);
        registerMethod<MathNS>("max"_str, &stdlib::mathMax);
        registerMethod<MathNS>("min"_str, &stdlib::mathMin);
        registerMethod<MathNS>("random"_str, &stdlib::mathRandom);
        registerMethod<MathNS>("round"_str, &stdlib::mathRound);
        registerMethod<MathNS>("sin"_str, &stdlib::mathSin);
        registerMethod<MathNS>("cos"_str, &stdlib::mathCos);
        registerMethod<MathNS>("sqrt"_str, &stdlib::mathSqrt);
        registerMethod<MathNS>("tan"_str, &stdlib::mathTan);
        // 常量属性 getter 须为无捕获 lambda（NativePropGetter 是裸函数指针）
        registerProperty<MathNS>("E"_str, [](const Value &) { return Value{ Real{ M_E } }; });
        registerProperty<MathNS>("LOG2E"_str, [](const Value &) { return Value{ Real{ M_LOG2E } }; });
        registerProperty<MathNS>("LOG10E"_str, [](const Value &) { return Value{ Real{ M_LOG10E } }; });
        registerProperty<MathNS>("LN10"_str, [](const Value &) { return Value{ Real{ M_LN10 } }; });
        registerProperty<MathNS>("LN2"_str, [](const Value &) { return Value{ Real{ M_LN2 } }; });
        registerProperty<MathNS>("PI"_str, [](const Value &) { return Value{ Real{ M_PI } }; });
        registerProperty<MathNS>("SQRT1_2"_str, [](const Value &) { return Value{ Real{ M_SQRT2 / 2 } }; });
        registerProperty<MathNS>("SQRT2"_str, [](const Value &) { return Value{ Real{ M_SQRT2 } }; });
        _impl->globalObject.setProp(_impl->rt.atomTable.intern("Math"_str),
                                    Value{ _impl->rt.create<MathNS>().get() });

        // Array/Dictionary "类对象"注册到全局：承载静态方法命名空间（语料惯用法
        // Dictionary.assign incontextof obj——取未绑定方法后 ChgThis 换 this）；
        // call() 即构造入口（new Array() / new Dictionary() / new RegExp(...)，
        // new 编译为透明调用——IRGenerator 对 New 直接求值 rhs）
        _impl->globalObject.setProp(_impl->rt.atomTable.intern("Array"_str),
                                    Value{ _impl->rt.create<Array>(&_impl->rt).get() });
        _impl->globalObject.setProp(_impl->rt.atomTable.intern("Dictionary"_str),
                                    Value{ _impl->rt.create<Dictionary>().get() });
        // RegExp 类对象：空模式占位实例，RegExp(pattern, flags?) 构造入口
        _impl->globalObject.setProp(_impl->rt.atomTable.intern("RegExp"_str),
                                    Value{ _impl->rt.create<RegExp>(&_impl->rt, ""_str).get() });
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