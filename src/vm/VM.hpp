/*
 * Copyright (c) 2025/12/29 上午8:08
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
#pragma once


#include "VMState.hpp"
#include "common/Defer.hpp"
#include "runtime/Runtime.hpp"

#include "gen/IRGenerator.hpp"
#include "parser/Parser.hpp"
#include "parser/ast/ExprNode.hpp"

#include "types/Function.hpp"
#include "types/Value.hpp"

namespace cial {

    template <typename T>
    struct HandleConvert;

    struct DefaultHandleConvert {
        static void handleValue(Runtime *, const Value &) noexcept {}
        static void releaseValue(Runtime *, const Value &) noexcept {}
    };

    template <>
    struct HandleConvert<Value> : DefaultHandleConvert {
        static Value fromValue(const Value &v) { return v; }
    };

    template <>
    struct HandleConvert<bool> : DefaultHandleConvert {
        static bool fromValue(const Value &v) { return v.asBool(); }
    };

    template <>
    struct HandleConvert<Integer> : DefaultHandleConvert {
        static Integer fromValue(const Value &v) { return v.asInteger().unwrap(); }
    };

    template <>
    struct HandleConvert<Real> : DefaultHandleConvert {
        static Real fromValue(const Value &v) { return v.asReal().unwrap(); }
    };

    template <>
    struct HandleConvert<String *> : DefaultHandleConvert {
        static String *fromValue(const Value &v) { return v.asString().unwrap(); }
    };

    template <>
    struct HandleConvert<Octet *> : DefaultHandleConvert {
        static Octet *fromValue(const Value &v) { return v.asOctet().unwrap(); }
    };

    template <>
    struct HandleConvert<Object *> {

        static Object *fromValue(const Value &v) { return v.asObject().unwrap(); }

        static void handleValue(Runtime *rt, const Value &v) noexcept {
            if(v.isObject()) {
                rt->addHandleVal(v.asObject().value());
            }
        }

        static void releaseValue(Runtime *rt, const Value &v) noexcept {
            if(v.isObject()) {
                rt->removeHandleVal(v.asObject().value());
            }
        }
    };

    template <typename T>
    struct HandleConvert<T *> : HandleConvert<Object *> {
        static T *fromValue(const Value &v) { return dynamic_cast<T *>(v.asObject().unwrap()); }
    };

    class VM {
    public:
        template <typename T>
        class Handle {
            Runtime *_rt;
            Value _value;

        public:
            Handle(Runtime *rt, const Value &value) : _rt(rt), _value(value) { retain(); }

            Handle(const Handle &other) : Handle(other._rt, other._value) {}

            Handle(Handle &&other) noexcept : _rt(std::exchange(other._rt, nullptr)), _value(std::move(other._value)) {}

            Handle &operator=(const Handle &other) {
                if(this != &other) {
                    Handle copy{ other };
                    std::swap(_rt, copy._rt);
                    std::swap(_value, copy._value);
                }
                return *this;
            }

            Handle &operator=(Handle &&other) noexcept {
                if(this != &other) {
                    Handle moved{ std::move(other) };
                    std::swap(_rt, moved._rt);
                    std::swap(_value, moved._value);
                }
                return *this;
            }

            ~Handle() noexcept {
                if(_rt && _value.isObject())
                    _rt->removeHandleVal(_value.asObject().value());
            }

            [[nodiscard]] T get() const { return HandleConvert<T>::fromValue(_value); }

            T operator*() const { return get(); }

            auto operator->() const {
                if constexpr(std::is_same_v<T, Value>) {
                    return &_value;
                } else if constexpr(std::is_pointer_v<T>) {
                    return HandleConvert<T>::fromValue(_value);
                } else {
                    static_assert(
                        std::is_same_v<T, Value>,
                        "operator-> is only supported for Handle<Value> or Handle<T*> to access raw Value methods.");
                    return nullptr;
                }
            }

        private:
            void retain() {
                if(_rt && _value.isObject())
                    _rt->addHandleVal(_value.asObject().value());
            }
        };

        explicit VM(vm::VMState *vmState) : _vmState(vmState) {}

        [[nodiscard]] Runtime &rt() const { return _vmState->rt; }

        template <typename T, typename... Args>
        T *create(Args &&...args) {
            return _vmState->rt.create<T>(std::forward<Args>(args)...).get();
        }

        // 原生代码同步调用脚本函数（TJS2 FuncCall 对应物）：Array.sort 比较器、
        // RegExp.replace 回调等"原生方法回调脚本"的统一底座。
        // 结构仿 eval：tmp 宿主帧承接返回值（reg0），不占用当前脚本帧的寄存器窗口；
        // 嵌套 run（entryStackTop 语义）。
        // 回调异常：tmp 帧挂全区哨兵 throwHandler 截住 unwind（否则异常会恢复调用方
        // 帧的 pc、在本次 run 内执行其 catch 块，native 调用方控制流错乱）——
        // 异常值落入哨兵 exValueReg(reg0)，func/tmp 帧由 run 弹出（栈深比正常少 1，
        // 以此判定），此处经 throwException 重新进入脚本异常协议向上传播
        Value callValue(const Value &callee, const size_t argCount, const Value *args,
                        const Value &fallbackThis = Value{}) {
            if(!callee.isObject())
                throw std::runtime_error("value is not callable");
            // 回调可重入并触发 GC；宿主栈上的实参和接收者在压入脚本帧前也须保根。
            Handle<Value> calleeRoot{ &rt(), callee };
            Handle<Value> receiverRoot{ &rt(), fallbackThis };
            Vec<Handle<Value>> argumentRoots;
            argumentRoots.reserve(argCount);
            for(size_t i = 0; i < argCount; ++i)
                argumentRoots.emplace_back(&rt(), args[i]);
            auto *obj = callee.asObject().unwrap();
            if(!obj)
                throw std::runtime_error("value is not callable");

            // 原生函数：无帧直调（callProc 同步返回）
            if(obj->typeId() == TypeId::NativeFunction) {
                auto *native = static_cast<NativeFunction *>(obj);
                const size_t arity = native->arity();
                // 实参 placement new 到栈缓冲直传（与 invokeCallV 原生分支同款），
                // 免每次回调的 Vec 堆分配；Value 非平凡构造/析构，只构造实际数量。
                // 实参 GC 根由上方 argumentRoots 承接，栈副本无需注册。
                if(argCount <= 64 && arity <= 64) {
                    alignas(Value) unsigned char argBuf[64 * sizeof(Value)];
                    Value *const argv = reinterpret_cast<Value *>(argBuf);
                    size_t built = 0;
                    DEFER {
                        while(built > 0)
                            argv[--built].~Value();
                    };
                    for(; built < argCount; ++built)
                        new(argBuf + built * sizeof(Value)) Value(args[built]);
                    for(; built < arity; ++built) // 缺省参数补 void（布局与 resize 等价）
                        new(argBuf + built * sizeof(Value)) Value();
                    if(native->boundThis().isVoid() && !fallbackThis.isVoid())
                        return native->callDirect(*_vmState, *receiverRoot, argCount, argv);
                    return native->callProc(this, argCount, argv);
                }
                Vec<Value> argv; // 超宽调用（>64 实参，罕见）回落堆缓冲
                if(argCount)
                    argv.assign(args, args + argCount);
                if(argv.size() < arity)
                    argv.resize(arity);
                if(native->boundThis().isVoid() && !fallbackThis.isVoid())
                    return native->callDirect(*_vmState, *receiverRoot, argCount, argv.data());
                return native->callProc(this, argCount, argv.data());
            }

            auto *fn = obj->typeId() == TypeId::Function ? static_cast<Function *>(obj) : nullptr;
            if(!fn)
                throw std::runtime_error("value is not callable");

            constexpr u16 retReg = 0;
            vm::Chunk tmpChunk{};
            tmpChunk.setRegCount(1); // reg0 = 返回值槽 / 哨兵异常值槽
            tmpChunk.emit<inter::NOP>(); // 哨兵区间 [0,1) 覆盖的 1 字节指令
            tmpChunk.addThrowHandler({ 0, 1, retReg }); // 全区哨兵：截住 unwind

            // 压栈顺序必须是 tmp 帧 → 实参 → func 帧：CallFrame 构造以
            // allocFrame(regCount) - arity 回退对齐参数寄存器，前提是实参
            // 紧贴 func 帧窗口（先 push 实参会把 tmp 帧的槽夹进参数区，整体错位 1 槽）
            tmpChunk.toBytecode(); // 哨兵必须落进 bc 层 handlers 表：unwind 只查 bc 表，
                                   // 而哨兵表惰性编译由 FETCH_NEW_FRAME 触发、tmp 帧永不
                                   // 被派发执行——不显式编译哨兵形同虚设，异常会穿透直达
                                   // 调用方帧的 handler（嵌套 run 内执行其 catch，控制流错乱）
            _vmState->allocCallFrame(&tmpChunk);
            _vmState->makeClosure();
            _vmState->curFrame()->setThisObj(*receiverRoot);
            for(size_t i = 0; i < argCount; ++i)
                _vmState->push(args[i]);
            const size_t tmpSp = _vmState->getRegPoolTop() - argCount; // tmp 帧窗口起点（异常值落点）

            fn->call(*_vmState, retReg, argCount);
            _vmState->makeClosure();
            const u32 stackTop = _vmState->context.stackTop();
            _vmState->run();

            // 哨兵路径：func/tmp 帧均已被 run 弹出（栈深 = 进入时 - 2）；
            // 正常路径 Ret 只弹 func 帧（栈深 = 进入时 - 1，tmp 帧由 eval 同款平衡逻辑弹）
            if(_vmState->context.stackTop() == stackTop - 2) {
                // 以专用载体解开 native 调用链（std::sort 等）；主循环 catch 接住后
                // 转 _pending + unwind，外层脚本 try/catch 正常接续
                throw vm::ScriptExceptionCarrier{ _vmState->poolAt(tmpSp) };
            }

            Value ret = _vmState->reg(retReg);
            if(stackTop - 1 != _vmState->context.stackTop())
                _vmState->freeCallFrame();
            _vmState->freeCallFrame();
            return ret;
        }

        template <typename T>
        Handle<T> getGlobal(const String &name) const {
            return { &_vmState->rt, _vmState->global(name.getData()) };
        }

        template <typename T>
        [[nodiscard]] Handle<T> eval(const String &str, bool filepath = false) const {
            Runtime &rt = _vmState->rt;
            if(str.isEmpty())
                return Handle<T>{ &rt, Value{} };

            Common::Result r{};
            Common::SourceFile sourceFile;
            if(filepath) {
                sourceFile = Common::SourceFile{ str.toStdStr() };
                sourceFile.load(r);
            } else {
                sourceFile.load(r, str.toStdStr());
            }
            if(r.isFailed()) {
                reportEvalError(r);
                return Handle<T>{ &rt, Value{} };
            }

            syntax::Parser parser{ sourceFile, _vmState->context.pp() };

            syntax::AstNode *node = parser.parse(r);
            if(r.isFailed()) {
                reportEvalError(r);
                return Handle<T>{ &rt, Value{} };
            }

            inter::IRGenerator codeGen{ r, rt, sourceFile };

            OptReg ignoreReg{};
            Opt<vm::Chunk> chunk = codeGen.parseAst(node, ignoreReg);

            // 解析/生成失败或产出空代码时直接报错返回，
            // 不能靠 assert 兜底（Release 下不存在）继续执行
            if(r.isFailed() || !chunk || chunk->getRegCount() == 0 || chunk->getInstVec().empty()) {
                if(!r.isFailed())
                    reportEvalError(str.isEmpty() ? "empty script" : "script produced no code");
                else
                    reportEvalError(r);
                return Handle<T>{ &rt, Value{} };
            }

            auto evalChunk = std::move(*chunk).toHeap(rt);

            u16 retReg{ 0 };
            vm::Chunk tmpChunk{};
            tmpChunk.setRegCount(1); // accept ret val

            _vmState->allocCallFrame(&tmpChunk);
            _vmState->makeClosure();
            _vmState->allocCallFrame(evalChunk.get(), retReg);
            _vmState->makeClosure();
            u32 stackTop = _vmState->context.stackTop();
            try {
                _vmState->run();
            } catch(...) {
                // 未捕获脚本异常经 unwind 以 C++ 异常逃逸:
                // 清理挂起状态(_pending/_exValue),否则毒害宿主的后续 eval
                _vmState->clearException();
                throw;
            }
            Value ret = _vmState->reg(retReg);

            // when evalChunk include `ret` inst, `ret` will call freeCallFrame, so we need check
            // keep Stack balancing
            if(stackTop - 1 != _vmState->context.stackTop()) {
                _vmState->freeCallFrame();
            }
            _vmState->freeCallFrame();

            return Handle<T>{ &rt, ret };
        }

        template <typename T>
        [[nodiscard]] Handle<T> evalExpr(const String &expr) const {
            Runtime &rt = _vmState->rt;
            if(expr.isEmpty())
                return Handle<T>{ &rt, Value{} };

            Common::SourceFile sourceFile{};
            Common::Result r{};

            sourceFile.load(r, expr.toStdStr());
            if(r.isFailed()) {
                reportEvalError(r);
                return Handle<T>{ &rt, Value{} };
            }

            syntax::Parser parser{ sourceFile, _vmState->context.pp() };

            syntax::ExprNode *node = parser.parseExpression(r, true);
            if(r.isFailed()) {
                reportEvalError(r);
                return Handle<T>{ &rt, Value{} };
            }

            inter::IRGenerator codeGen{ r, rt, sourceFile };

            OptReg retReg{ inter::Register{ 0xFFFF } };
            Opt<vm::Chunk> chunk = codeGen.parseAst(node, retReg);

            if(r.isFailed() || !chunk || !retReg || chunk->getRegCount() == 0 || chunk->getInstVec().empty()) {
                if(!r.isFailed())
                    reportEvalError("expression produced no code");
                else
                    reportEvalError(r);
                return Handle<T>{ &rt, Value{} };
            }

            auto evalChunk = std::move(*chunk).toHeap(rt);
            _vmState->allocCallFrame(evalChunk.get());
            _vmState->makeClosure();
            try {
                _vmState->run();
            } catch(...) {
                _vmState->clearException();
                throw;
            }

            Value ret = _vmState->reg(retReg->index());
            _vmState->freeCallFrame();

            return Handle<T>{ &rt, ret };
        }

        void eval(const String &str, const bool filepath = false) const { auto h = eval<Value>(str, filepath); }
        void evalExpr(const String &expr) const { auto h = evalExpr<Value>(expr); }

        [[nodiscard]] String dumpCurCallFrameInst() const { return _vmState->curFrame()->chunk->dumpInstructions(); }

    private:
        vm::VMState *_vmState;

        static void reportEvalError(const Common::Result &r) {
            for(const auto &m : r.messages()) {
                if(m.isError())
                    fmt::println(stderr, "cial: error: {} (line {})", m.message(), m.location().start().line);
            }
        }

        static void reportEvalError(const char *message) { fmt::println(stderr, "cial: error: {}", message); }
    };

} // namespace cial
