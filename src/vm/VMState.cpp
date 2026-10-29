/*
 * Copyright (c) 2024/5/8 上午8:08
 *   /\  _` \   __          /\_ \  /\_ \
 *   \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
 *    \ \ \/_/_\/\ \  /'__`\  \ \ \  \ \ \   /'__`\  /' _ `\  /'_ `\
 *     \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
 *      \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
 *       \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
 *                                                               /\____/
 *                                                               \_/__/
 *
 */

#include "VMState.hpp"

#include "../gen/IRGenerator.hpp"
#include "../gen/Instruction.hpp"
#include "VMDebug.hpp"
#include "common/Defer.hpp"
#include "parser/Parser.hpp"
#include "parser/ast/ExprNode.hpp"
#include "stdlib/NativeRegister.hpp"
#include "types/Array.hpp"
#include "types/Class.hpp"
#include "types/Dictionary.hpp"
#include "types/Function.hpp"
#include "types/Property.hpp"
#include "types/RegExp.hpp"

namespace cial {
    void CallFrame::setThisObj(Value v) {
        thisObj = std::move(v);
        thisData = thisObj.isObject() ? dynamic_cast<DataObject *>(thisObj.asObject().value()) : nullptr;
    }
} // namespace cial

namespace cial::vm {

    [[nodiscard]] bool VMState::globalHas(const Atom atom) const { return context.global()->hasProp(atom); }

    [[nodiscard]] bool VMState::globalHas(const std::string &name) const {
        const auto atom = rt.atomTable.intern(name.c_str(), name.length());
        return context.global()->hasProp(atom);
    }

    const Value *VMState::cachedGlobal(const Atom atom) const {
        auto *global = context.global();
        auto &entry = _globalCache[atom.v % 64];
        const u64 revision = global->slotRevision();
        if(entry.atom != atom || entry.revision != revision)
            entry = { atom, revision, global->findPropRef(atom) };
        return entry.slot;
    }

    [[nodiscard]] Value VMState::global(const Atom atom) const {
        if(const auto *slot = cachedGlobal(atom))
            return *slot;
        throw std::runtime_error("in global object Not found property");
    }

    void VMState::global(const Atom atom, const Value &value) const { context.global()->setProp(atom, value); }

    [[nodiscard]] Value VMState::global(const std::string &name) const {
        const auto atom = rt.atomTable.intern(name.c_str(), name.length());
        return context.global()->getProp(atom);
    }

    void VMState::global(const std::string &name, const Value &value) const {
        const auto atom = rt.atomTable.intern(name.c_str(), name.length());
        context.global()->setProp(atom, value);
    }

    const Value &VMState::reg(const u16 reg) const { return _curFrame->getReg(reg); }

    Value &VMState::regRef(const u16 reg) const { return _curFrame->getReg(reg); }

    bool VMState::hasThis(const Atom atom) const {

        // current thisObj（thisData 为帧创建时缓存的 DataObject*）
        if(_curFrame->thisData) {
            if(_curFrame->thisData->hasProp(atom)) {
                return true;
            }
        }

        if(auto *callFrame = _curFrame->closure; callFrame) {
            if(callFrame->funcMeta) {
                for(const auto &localVar : callFrame->funcMeta->localVars) {
                    if(localVar.endPC.address() > callFrame->pc)
                        continue;
                    if(localVar.identifier == atom)
                        return true;
                }
            }
        }

        // global
        return globalHas(atom);
    }

    Value VMState::getThis(const Atom atom) const {

        // current thisObj（thisData 为帧创建时缓存的 DataObject*）
        if(_curFrame->thisData) {
            if(_curFrame->thisData->hasProp(atom)) {
                return _curFrame->thisData->getProp(atom);
            }
        }

        if(auto *callFrame = _curFrame->closure; callFrame) {
            if(callFrame->funcMeta) {
                for(const auto &[identifier, reg, startPC, endPC] : callFrame->funcMeta->localVars) {
                    if(startPC.address() <= callFrame->pc && callFrame->pc < endPC.address())
                        continue;
                    if(identifier == atom)
                        return callFrame->getReg(reg.index());
                }
            }
        }

        // global
        return global(atom);
    }

    void VMState::setThis(const Atom atom, const Value &v) const {
        // current thisObj（thisData 为帧创建时缓存的 DataObject*）
        if(_curFrame->thisData) {
            if(_curFrame->thisData->hasProp(atom)) {
                _curFrame->thisData->setProp(atom, v);
                return;
            }
        }

        if(auto *callFrame = _curFrame->closure; callFrame) {
            if(callFrame->funcMeta) {
                for(const auto &localVar : callFrame->funcMeta->localVars) {
                    if(localVar.endPC.address() > callFrame->pc)
                        continue;
                    if(localVar.identifier == atom)
                        callFrame->getReg(localVar.reg.index()) = v;
                }
            }
        }

        // global
        return global(atom, v);
    }

    Value VMState::getUpVal(const Atom atom) const {

        // current context
        if(_curFrame->thisData) {
            return _curFrame->thisData->getProp(atom);
        }

        for(u16 i = _stackTop - 1; i > 0; --i) {
            const auto &callFrame = _callStack[i - 1];
            // prev local scope
            if(callFrame.funcMeta) {
                for(const auto &localVar : callFrame.funcMeta->localVars) {
                    if(localVar.endPC.address() > callFrame.pc)
                        continue;
                    if(localVar.identifier == atom)
                        return callFrame.getReg(localVar.reg.index());
                }
            }

            // prev context
            if(callFrame.thisObj.isObject()) {
                if(auto *instanceObject = dynamic_cast<DataObject *>(_curFrame->thisObj.asObject().value())) {
                    return instanceObject->getProp(atom);
                }
            }
        }

        // global
        return global(atom);
    }

    void VMState::unwind() {
        Chunk *lastChunk{}; // 弹空前最后的帧（终态报错带行号用）
        u64 lastPc{};
        while(true) {
            const CallFrame *cFrame = _curFrame;
            // run0 的 pc 是扁平字节码偏移，因此这里查询字节码空间的 try 区间表
            const Opt<ThrowHandler> th = cFrame->chunk->findBcThrowHandler(getPC());
            if(!th) {
                if(cFrame->ret)
                    prevFrame()->getReg(*cFrame->ret) = Value{};
                lastChunk = cFrame->chunk;
                lastPc = getPC();
                freeCallFrame();
                if(_stackTop == 0) {
                    // 终态报错的行号后缀（Rlc 未记录时省略）
                    std::string suffix;
                    if(lastChunk) {
                        if(const auto loc = lastChunk->lineAtBc(lastPc))
                            suffix = fmt::format(" (line {})", loc->start().line + 1); // AST 行号 0-based
                    }
                    // 未捕获异常：Exception 对象优先输出 message（与 TJS2 行为一致，
                    // 否则只显示 <object> 无法定位语料失败点）
                    if(_exValue.isObject()) {
                        if(auto *obj = _exValue.asObject().unwrap(); dynamic_cast<DataObject *>(obj)) {
                            const Atom msgAtom = rt.atomTable.intern("message"_str);
                            if(obj->hasProp(msgAtom)) {
                                throw std::runtime_error(
                                    fmt::format("{}{}", obj->getProp(msgAtom), suffix));
                            }
                        }
                    }
                    throw std::runtime_error(fmt::format("{}{}", _exValue, suffix));
                }
                continue;
            }

            if(th->exValueReg)
                regRef(th->exValueReg) = _exValue;

            setPC(th->tryEnd);
            clearException();
            return;
        }
    }

    static bool propObjectSet(VMState &vmState, const Value &src, const Value &dst) {
        if(dst.isObject()) {
            if(const auto *prop = dynamic_cast<Property *>(dst.asObject().value())) {
                prop->invokeSet(vmState, src);
                return true;
            }
        }
        return false;
    }

    static bool propObjectGet(VMState &vmState, const Value &src, Value &dst) {
        if(src.isObject()) {
            if(const auto object = src.asObject().unwrap()) {
                if(const auto *prop = dynamic_cast<Property *>(object)) {
                    dst = prop->invokeGet(vmState);
                    return true;
                }
            }
        }
        return false;
    }

    // ── 成员访问共享体（GProp/GPropC、DProp/DPropC 等成对指令共用）──
    // 寄存器成员名形态先做整数成员名分支（数组下标）并 intern，随后进入此处

    // obj.member 读：原生成员（方法/count 属性）→ Global/Class/普通对象分支 →
    // 非对象值的原生 property 表。getter 经 invokeGet 嵌套 run0 同步完成，恒 Continue
    static void propGetMember(VMState &vmState, CallFrame *cur, const Value &val, const Atom atom,
                              Value &dst) {
        if(val.isObject()) {
            auto *obj = val.asObject().value();

            // Array/Dictionary/RegExp 的原生成员（方法 / count 属性）：与字符串原生表同机制；
            // 未命中落到对象自身 getProp（RegExp 的 matches/index 等状态属性走此路径）
            const TypeId tid = obj->typeId();
            if(tid == TypeId::Array || tid == TypeId::Dictionary || tid == TypeId::RegExp ||
               tid == TypeId::Math || tid == TypeId::Date || tid == TypeId::RandomGen) {
                if(auto *getter = vmState.context.findProperty(tid, atom)) {
                    dst = getter(val);
                    return;
                }
                if(auto *nativeFn = vmState.context.findMethod(tid, atom)) {
                    nativeFn = vmState.rt.create<NativeFunction>(*nativeFn).get();
                    nativeFn->setThisObj(val);
                    dst = Value{ nativeFn };
                    return;
                }
                // 未命中落到对象自身 getProp（count 读在 Array::getProp 有兜底）
            }

            if(tid == TypeId::Global) {
                // for global, proxy object, maybe get extends object in dataObject?
                auto *global = static_cast<GlobalObject *>(obj);
                if(global->proxy) {
                    throw std::runtime_error("Not implemented");
                }
                auto tmp = global->getProp(atom);
                propObjectGet(vmState, tmp, tmp); // 顶层 property 读（字节码 SPD/GPD 经 this=全局）
                dst = tmp;
                return;
            }

            if(tid == TypeId::Class) {
                auto tmp = obj->getProp(atom);
                propObjectGet(vmState, tmp, tmp);
                // 构造器名访问返回 ClassObject 本身（super.<Ctor>() 路径），非 Function
                if(tmp.isObject()) {
                    if(auto *fun = dynamic_cast<Function *>(tmp.asObject().value())) {
                        fun->thisObj = cur->thisObj.asObject().value();
                    }
                }
                dst = tmp;
                return;
            }

            auto tmp = obj->getProp(atom);
            propObjectGet(vmState, tmp, tmp);
            dst = tmp;
            return;
        }

        // Native property（如 str.length / octet.length）
        // void 等无方法表的值：直接报脚本异常（旧实现 Debug 下 assert 崩溃，
        // Release 走 findProperty/None 落空后 throw——两配置行为不一致）
        if(val.isVoid())
            throw std::runtime_error("cannot access member of void");
        const TypeId tId = ValueToTypeId::getId(val);
        if(auto *getter = vmState.context.findProperty(tId, atom)) {
            dst = getter(val);
            return;
        }

        // Native method
        NativeFunction *nativeFn = vmState.context.findMethod(tId, atom);
        if(!nativeFn) {
            // 成员不存在:脚本可捕获的运行期异常(与对象成员缺失同语义)
            const auto *entry = vmState.rt.atomTable.get(atom);
            const std::string nameStr = entry && entry->str ? entry->str->toStdStr() : "?atom?";
            throw std::runtime_error("in native object Not found property: " + nameStr);
        }
        nativeFn = vmState.rt.create<NativeFunction>(*nativeFn).get();
        nativeFn->setThisObj(val);
        dst = Value{ nativeFn };
    }

    // obj.member = v 写：槽位当前值是 Property 对象则调 setter（invokeSet 只压
    // 帧不执行，与 DGlobal 同款检测栈变化交还主循环）；普通成员纯写。
    // Global 不豁免：字节码路径顶层 property 写入即 SPD this.x（this=全局），
    // TJS2 PropSet 对 property 槽同样触发 setter。返回 true 表示 setter 已压帧
    static bool propSetMember(VMState &vmState, const Value &objVal, const Atom atom, const Value &src) {
        if(!objVal.isObject()) {
            throw std::runtime_error("dprop obj is not object");
        }
        auto *obj = objVal.asObject().unwrap();
        Value curVal{};
        bool hasCur = false;
        if(obj->hasProp(atom)) {
            hasCur = true;
            curVal = obj->getProp(atom);
        }
        if(hasCur && propObjectSet(vmState, src, curVal)) {
            return true;
        }
        obj->setProp(atom, src);
        return false;
    }

    /**
     * 扁平字节码解释器主循环。
     *
     * 嵌套进入（property getter / 类默认值初始化 / 构造函数）时，
     * entryStackTop 记录进入时的栈深：被调帧执行完毕（栈回落到入口之下）即返回，
     * 交还给触发嵌套调用的 dispatch 继续。
     */
    void VMState::drainFinalizers() {
        auto &gc = rt.markSweep;
        if(gc.drainingFinalizers || gc.pendingFinalizers.empty())
            return;
        gc.drainingFinalizers = true;
        const bool savedZF = _zf;
        DEFER {
            gc.drainingFinalizers = false;
            _zf = savedZF;
        };
        // 新回调产生的垃圾留给下一安全点，避免一条指令无限延迟。
        size_t remaining = gc.pendingFinalizers.size();
        while(remaining-- && !gc.pendingFinalizers.empty()) {
            auto *object = gc.pendingFinalizers.front();
            if(!object->canFinalizeIn(*this)) {
                gc.pendingFinalizers.erase(gc.pendingFinalizers.begin());
                gc.pendingFinalizers.push_back(object);
                continue;
            }
            DEFER { gc.pendingFinalizers.erase(gc.pendingFinalizers.begin()); };
            object->finalizeDeferred(*this);
        }
    }

    void VMState::run0() {
        const size_t entryStackTop = _stackTop;
        if(entryStackTop == 0)
            return;

        for(;;) {
        FETCH_NEW_FRAME:
            CallFrame *cur = _curFrame;
            u64 pc = cur->pc;
            Chunk *chunk = cur->chunk;
            // 字节码按需编译（幂等）：进入字节码路径的 chunk 才编译
            chunk->toBytecode();
            const Constant *constants = chunk->getConstants().data();
            Value *regs = cur->regs();

            const Vec<u8> &codeVec = chunk->code();
            const u8 *ip = codeVec.data();
            const size_t bcSize = codeVec.size();

            while(pc < bcSize) {
                DispatchRet r{};
                try {
                    if(!rt.markSweep.pendingFinalizers.empty() && !rt.markSweep.drainingFinalizers) {
                        cur->pc = pc;
                        drainFinalizers();
                    }
                    switch(static_cast<VmOpCode>(ip[pc++])) {
#define DISPATCH_CASE(op)                                                                                              \
    case VmOpCode::op:                                                                                                 \
        r = dispatch##op(cur, constants, regs, ip, pc);                                                                \
        break;
                        CIAL_BYTECODE_OPCODE_ENUMS(DISPATCH_CASE)
#undef DISPATCH_CASE
                    }
                } catch(ScriptExceptionCarrier &carried) {
                    // 回调异常（VM::callValue 哨兵截获）：转入脚本异常协议，
                    // 由下方 unwind 恢复外层脚本 try/catch（栈空时 unwind 抛终态）
                    _pending = PendingCF::Throw;
                    _exValue = carried.value;
                    if(_exValue.isObject())
                        rt.addHandleVal(_exValue.asObject().value());
                    carried.value = Value{}; // 所有权移交 _exValue，防析构减引用
                    cur->pc = pc;
                } catch(const std::exception &e) {
                    if(_pending != PendingCF::None) {
                        throw; // 脚本异常传播中,不吞噬
                    }
                    // 原生异常(属性缺失/操作数类型错误等)转脚本异常:
                    // 有 catch 处理器则被脚本 try/catch 捕获(TJS2 语义),
                    // 无则最终仍向上报告。
                    // 包装为 Exception 实例(语料 catch(e) 后访问 e.message):
                    // 直接构造 DataObject 挂 message 槽位,不执行 TJS 构造器
                    _pending = PendingCF::Throw;
                    const Atom excAtom = rt.atomTable.intern("Exception"_str);
                    bool wrapped = false;
                    if(const Value *klassVal = context.global()->findPropRef(excAtom); klassVal && klassVal->isObject()) {
                        if(auto *klass = dynamic_cast<ClassObject *>(klassVal->asObject().unwrap())) {
                            auto *obj = rt.create<DataObject>(this, klass).get();
                            obj->setProp(rt.atomTable.intern("message"_str), Value{ String{ e.what() } });
                            _exValue = Value{ obj };
                            rt.addHandleVal(obj); // 根保护至 clearException
                            wrapped = true;
                        }
                    }
                    if(!wrapped) {
                        _exValue = Value{ String{ e.what() } };
                    }
                    // 原生错误消息带行号（Rlc 未记录时省略）；经 setProp 重建消息，
                    // 不原地改共享 String payload
                    if(const auto loc = cur->chunk->lineAtBc(pc)) {
                        const Atom msgAtom = rt.atomTable.intern("message"_str);
                        if(_exValue.isObject()) {
                            auto *obj = _exValue.asObject().unwrap();
                            if(obj->hasProp(msgAtom)) {
                                obj->setProp(msgAtom, Value{ String{
                                    fmt::format("{} (line {})", obj->getProp(msgAtom),
                                                loc->start().line + 1) // AST 行号 0-based
                                        .c_str() } });
                            }
                        } else {
                            _exValue = Value{
                                String{ fmt::format("{} (line {})", _exValue, loc->start().line + 1) } };
                        }
                    }
                    cur->pc = pc;
                }

                if(r == DispatchRet::Call) {
                    goto FETCH_NEW_FRAME;
                }

                if(r == DispatchRet::Return) {
                    break;
                }

                if(_pending == PendingCF::Throw) {
                    // 同步局部 pc 到帧上（unwind 依据它查找 try 区间）；
                    // unwind 可能释放当前帧及以上帧，之后以 _curFrame 为准重新取指
                    cur->pc = pc;
                    unwind();
                    if(_curFrame != cur) {
                        goto FETCH_NEW_FRAME;
                    }
                    pc = _curFrame->pc;
                }
            }

            // 内层循环退出：Return / 帧代码执行到末尾
            if(_stackTop > 1) {
                freeCallFrame();
                if(_stackTop < entryStackTop)
                    return; // 嵌套进入：目标帧已完成，交还原调用方
                continue; // 取父帧，从其保存的 pc 继续
            }
            break; // 最底层帧执行完毕
        }
    }

#define DISPATCH_OP_METHOD(op)                                                                                         \
    DispatchRet VMState::dispatch##op(CallFrame *cur, const Constant *constants, Value *regs, const u8 *ip, u64 &pc)

    DISPATCH_OP_METHOD(NOP) { return DispatchRet::Continue; }

    DISPATCH_OP_METHOD(Debugger) {
        std::string regsDump = dumpCurRegisters();
        std::string localVarDump = dumpCurLocalVars();
        std::string instructionsDump = dumpCurInstructions();
        std::string constantsDump = dumpCurConstants();

        fmt::println("========================");
        fmt::println("========= Dump =========");
        fmt::println("========================");
        fmt::println("========= reg =========");
        fmt::println("{}", regsDump);
        fmt::println("======= local var =======");
        fmt::println("{}", localVarDump);
        fmt::println("========= const =========");
        fmt::println("{}", constantsDump);
        fmt::println("========= inst =========");
        fmt::println("{}", instructionsDump);
        DEBUG_BREAK();
        return DispatchRet::Continue;
    }

    // ── 栈操作 ───────────────────────────────────────────────
    DISPATCH_OP_METHOD(Push) {
        const u16 r1 = read<u16>(ip, pc);
        push(regs[r1]);
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(PopN) {
        const i64 count = decodeSleb128(ip, pc, 10);
        pop(static_cast<size_t>(count));
        return DispatchRet::Continue;
    }

    // ── 上下文对象 ───────────────────────────────────────────
    DISPATCH_OP_METHOD(Global) {
        const u16 r1 = read<u16>(ip, pc);
        regs[r1] = Value{ context.global() };
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(Super) {
        const u16 r1 = read<u16>(ip, pc);
        auto *dataObj = dynamic_cast<DataObject *>(cur->thisObj.asObject().unwrap());
        // 方法内：_lastSuperClass = 构造链中最后初始化的基类（"后继承优先"语义）
        if(dataObj->getSuperClass()) {
            regs[r1] = Value{ dataObj->getSuperClass() };
            return DispatchRet::Continue;
        }
        // 构造器内 super.<baseCtor>()：setSuperClass 尚未发生（基类构造完成时才记录），
        // _lastSuperClass 为 null——按类定义的 extends 解析到最左（首个）基类
        const auto &exts = dataObj->klass()->meta->extends;
        for(const Atom extName : exts) {
            if(globalHas(extName)) {
                regs[r1] = global(extName);
                return DispatchRet::Continue;
            }
        }
        throw std::runtime_error("super used in class without base class");
    }

    DISPATCH_OP_METHOD(This) {
        const u16 r1 = read<u16>(ip, pc);
        regs[r1] = Value{ cur->thisObj };
        return DispatchRet::Continue;
    }

    // ── 标志/失效 ────────────────────────────────────────────
    DISPATCH_OP_METHOD(Test) {
        const u16 r1 = read<u16>(ip, pc);
        _zf = regs[r1].asBool();
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(Inv) {
        const u16 r1 = read<u16>(ip, pc);
        // invalidate 会把 finalize 帧压栈（如果类定义了 finalize）；
        // run0 缓存了当前帧，必须跳到新帧执行完再回来
        const size_t top = _stackTop;
        dynamic_cast<DataObject *>(regs[r1].asObject().unwrap())->invalidate();
        if(_stackTop != top) {
            cur->pc = pc;
            return DispatchRet::Call;
        }
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(Throw) {
        const u16 r1 = read<u16>(ip, pc);
        throwException(regs[r1]);
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(Ret) {
        const u16 r1 = read<u16>(ip, pc);
        // 顶层帧（无父帧/无返回寄存器）直接结束，不回写
        if(_stackTop > 1 && cur->ret) {
            Value &value = regs[r1];
            Value &dst = (cur - 1)->getReg(*cur->ret);
            if(value.isIntegerFast()) {
                // 整数直写:跳过 Value 拷贝构造/析构(数值递归的返回主路径)
                dst.type(ValueType::Integer);
                dst.asIntegerStore(value.asIntegerUnsafe());
            } else {
                dst = value;
            }
        }
        return DispatchRet::Return;
    }

    // ── 跳转（u32 目标已在编译期回填） ───────────────────────
    DISPATCH_OP_METHOD(Jmp) {
        pc = read<u32>(ip, pc);
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(JmpE) {
        const u32 addr = read<u32>(ip, pc);
        if(_zf)
            pc = addr;
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(JmpNE) {
        const u32 addr = read<u32>(ip, pc);
        if(!_zf)
            pc = addr;
        return DispatchRet::Continue;
    }

    // ── 类型转换/单目 ─────────────────────────────────────────
    DISPATCH_OP_METHOD(ToInt) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        Value &dst = regs[r2];
        dst = regs[r1];
        if(const auto r = dst.toInteger(); r.isFailed()) {
            throw r.getErr();
        }
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(ToReal) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        Value &dst = regs[r2];
        dst = regs[r1];
        if(const auto r = dst.toReal(); r.isFailed()) {
            throw r.getErr();
        }
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(ToString) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        Value &dst = regs[r2];
        dst = regs[r1];
        if(const auto r = dst.toString(); r.isFailed()) {
            throw r.getErr();
        }
        return DispatchRet::Continue;
    }

    // #s：取操作数字符串化后首字符的字符码（空串为 0）。TJS2 语义:整数 65 → "65"
    // → '6' → 54;octet 无字符串转换,抛错(语料 test_variant 期望其进入 catch)
    DISPATCH_OP_METHOD(Asc) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        Value strVal = regs[r1];
        if(strVal.isOctet()) {
            throw std::runtime_error("'#' operator expects a string operand");
        }
        if(!strVal.isString()) {
            if(const auto r = strVal.toString(); r.isFailed()) {
                throw r.getErr();
            }
        }
        const String *str = strVal.asString().unwrap();
        regs[r2] =
            Value{ Integer{ str->isEmpty() ? 0 : static_cast<Integer>(static_cast<unsigned char>(str->getData()[0])) } };
        return DispatchRet::Continue;
    }

    // $n：字符码转 1 字符字符串（与 charAt 一致的字节语义）
    DISPATCH_OP_METHOD(Chr) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        const Value &src = regs[r1];
        if(!src.isInteger() && !src.isReal()) {
            throw std::runtime_error("'$' operator expects an integer operand");
        }
        const auto code = static_cast<unsigned char>(
            src.isInteger() ? src.asIntegerUnsafe() : static_cast<Integer>(src.asReal().unwrap().value()));
        regs[r2] = Value{ String{ static_cast<char>(code) } };
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(TypeOf) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        // 与 TJS 一致的类型名:Integer/Real/String/Object/void/Octet
        const Value &src = regs[r1];
        switch(src.type()) {
            case ValueType::Integer:
                regs[r2] = Value{ String{ "Integer" } };
                break;
            case ValueType::Real:
                regs[r2] = Value{ String{ "Real" } };
                break;
            case ValueType::String:
                regs[r2] = Value{ String{ "String" } };
                break;
            case ValueType::Octet:
                regs[r2] = Value{ String{ "Octet" } };
                break;
            case ValueType::Object: {
                // TJS 中对象返回类名(全局对象为 "Object")
                const auto objR = src.asObject();
                if(objR.isOk() && objR.value()) {
                    const Atom name = objR.value()->getName();
                    if(const auto *entry = rt.atomTable.get(name);
                       entry && entry->str && entry->str->length() > 0) {
                        regs[r2] = Value{ String{ entry->str->getData(), entry->str->length() } };
                    } else {
                        regs[r2] = Value{ String{ "Object" } };
                    }
                } else {
                    regs[r2] = Value{ String{ "Object" } };
                }
                break;
            }
            case ValueType::Void:
                regs[r2] = Value{ String{ "void" } };
                break;
        }
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(LNot) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        Value &dst = regs[r2];
        dst = regs[r1];
        dst.toLogicalNot();
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(ChgSign) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        Value &dst = regs[r2];
        dst = regs[r1];
        if(const auto r = dst.toSignChange(); r.isFailed()) {
            throw r.getErr();
        }
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(ChgThis) {
        const u16 r1 = read<u16>(ip, pc); // 函数
        const u16 r2 = read<u16>(ip, pc); // 新 this
        // incontextof 的对象可为脚本函数、原生函数或 property 闭包
        //（TJS2 顶层 property 定义经 CHGTHIS 绑定 this=global）
        if(auto *fn = dynamic_cast<Function *>(regs[r1].asObject().unwrap())) {
            fn->thisObj = regs[r2].asObject().unwrap();
        } else if(auto *nativeFn = dynamic_cast<NativeFunction *>(regs[r1].asObject().unwrap())) {
            nativeFn->setThisObj(regs[r2]);
        } else if(auto *prop = dynamic_cast<Property *>(regs[r1].asObject().unwrap())) {
            prop->thisObj = regs[r2].asObject().unwrap();
        } else {
            throw std::runtime_error("incontextof expects a function");
        }
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(ChkInv) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        regs[r2] = Value{ dynamic_cast<DataObject *>(regs[r1].asObject().unwrap())->isValid() };
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(EvalStr) {
        // 后缀 `!` 即时求值（tjs.y priority_expr "!" → T_EVAL → VM_EVAL 对应物）：
        // src 字符串化为 TJS 表达式编译执行；空串/非串原值不变（原版 Eval 语义）。
        // 嵌套执行用 eval() 同款双帧模式（tmp 宿主帧承接返回值 + 栈平衡）——外层
        // 脚本帧存活时不能用单帧模式，否则嵌套 run0 会继续执行外层帧
        const u16 src = read<u16>(ip, pc); // 与 encode 同序：先 src 后 dst
        const u16 dst = read<u16>(ip, pc);
        const Value &val = regs[src];
        if(!val.isString() || val.asString().unwrap()->isEmpty()) {
            regs[dst] = val;
            return DispatchRet::Continue;
        }
        const String expr = *val.asString().unwrap();

        // 表达式编译（失败抛脚本异常，与原版 EvalExpression 一致）
        Common::SourceFile sourceFile{};
        Common::Result r{};
        sourceFile.load(r, expr.toStdStr());
        if(r.isFailed())
            throw std::runtime_error("cannot evaluate expression");
        syntax::Parser parser{ sourceFile, context.pp() };
        syntax::ExprNode *node = parser.parseExpression(r, true);
        if(r.isFailed() || !node)
            throw std::runtime_error("cannot evaluate expression");
        inter::IRGenerator codeGen{ r, rt, sourceFile };
        OptReg retReg{};
        Opt<vm::Chunk> compiled = codeGen.parseAst(node, retReg);
        if(r.isFailed() || !compiled || !retReg || compiled->getRegCount() == 0
           || compiled->getInstVec().empty())
            throw std::runtime_error("cannot evaluate expression");
        // 表达式块以 Ret 收尾：返回值经 Ret 写回 tmp 帧槽位，嵌套 run0 在本帧
        // 弹出后即停（callValue 的帧契约——无 Ret 的块会由 run0 的"代码到末尾"
        // 路径自行弹帧，返回值无人回写）
        compiled->emit<inter::Ret>(*retReg);
        auto *evalChunk = rt.create<vm::Chunk>(std::move(*compiled)).get();

        constexpr u16 retSlot = 0;
        vm::Chunk tmpChunk{};
        tmpChunk.setRegCount(1); // reg0 = 返回值槽 / 哨兵异常值槽
        tmpChunk.emit<inter::NOP>();
        tmpChunk.addThrowHandler({ 0, 1, retSlot }); // 全区哨兵：截住 unwind
        tmpChunk.toBytecode(); // 哨兵表必须显式落 bc 层（callValue 同款陷阱）

        const size_t tmpSp = getRegPoolTop(); // tmp 帧 reg0 的池内位置（异常值落点）
        allocCallFrame(&tmpChunk);
        makeClosure();
        allocCallFrame(evalChunk, retSlot);
        makeClosure();
        const size_t stackTop = context.stackTop();
        run0();

        // 哨兵路径：eval/tmp 帧均被弹出（栈深 -2）→ 载体解开 native 调用链
        if(context.stackTop() == stackTop - 2)
            throw ScriptExceptionCarrier{ poolAt(tmpSp) };

        const Value ret = reg(retSlot);
        if(stackTop - 1 != context.stackTop())
            freeCallFrame();
        freeCallFrame();
        _curFrame->regs()[dst] = ret; // 嵌套 run 可能迁移寄存器池，重取当前窗口
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(ChkIns) {
        const u16 r1 = read<u16>(ip, pc); // 值(任意类型),同时是输出 bool 的槽位
        const u16 r2 = read<u16>(ip, pc); // 类名字符串
        Value &dst = regs[r1];
        if(const Value &src = regs[r2]; src.isString()) {
            const String &name = *src.asString().unwrap();
            // TJS2 语义:基础类型用类型名匹配(Integer/Real 同属 "Number"),
            // 其余走对象的类名 instanceOf
            bool result = false;
            if(name == "Number") {
                result = dst.isInteger() || dst.isReal();
            } else if(name == "Integer") {
                result = dst.isInteger();
            } else if(name == "Real") {
                result = dst.isReal();
            } else if(name == "String") {
                result = dst.isString();
            } else if(name == "Octet") {
                result = dst.isOctet();
            } else if(dst.isObject()) {
                const Atom atom = rt.atomTable.intern(name);
                result = dst.asObject().unwrap()->instanceOf(atom);
            }
            dst = Value{ Integer{ result } };
            return DispatchRet::Continue;
        }
        dst = Value{ Integer{ false } };
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(TestVoid) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        regs[r2] = Value{ Integer{ regs[r1].isVoid() } };
        return DispatchRet::Continue;
    }

    // ── 装载 ─────────────────────────────────────────────────
    DISPATCH_OP_METHOD(Load) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 cIdx = read<u16>(ip, pc);
        regs[r1] = constants[cIdx].createValue(&rt);
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(LoadImm) {
        const u16 r1 = read<u16>(ip, pc);
        const i64 imm = decodeSleb128(ip, pc, 10);
        if(regs[r1].isIntegerFast())
            regs[r1].asIntegerStore(imm);
        else
            regs[r1] = Value{ imm };
        return DispatchRet::Continue;
    }

    // ── 全局/this/局部变量访问 ────────────────────────────────
    // 属性 setter（Property::invokeSet）只压帧不执行，需要返回 Call
    // 让主循环跳到新帧；getter（invokeGet）内部嵌套 run0 执行完毕，不影响本帧。
    DISPATCH_OP_METHOD(DGlobal) {
        const u32 atom = read<u32>(ip, pc);
        const u16 r1 = read<u16>(ip, pc);
        const Atom a{ atom };
        const Value &srcVal = regs[r1];
        if(const Value *slot = cachedGlobal(a); slot && slot->isIntegerFast() && srcVal.isIntegerFast()) {
            const_cast<Value *>(slot)->asIntegerStore(srcVal.asIntegerUnsafe());
            return DispatchRet::Continue;
        }
        if(globalHas(a) && propObjectSet(*this, srcVal, global(a))) {
            cur->pc = pc;
            return DispatchRet::Call;
        }
        global(a, Value{ srcVal });
        return DispatchRet::Continue;
    }

    void VMState::loadGlobal(const Atom atom, Value &dst) {
        if(const Value *g = cachedGlobal(atom)) {
            // Property 触发 getter 的慢路径之外,直接从槽位零拷贝取值。
            // 注意寄存器写入可能触发 setter 语义无关,只读安全;
            // Property 调用可能修改 _props(重入),先拷出再降级慢路径
            if(g->isObject() && dynamic_cast<const Property *>(g->asObject().unwrap()) != nullptr) {
                Value srcVal = *g;
                propObjectGet(*this, srcVal, srcVal);
                dst = srcVal;
                return;
            }
            if(g->isIntegerFast() && dst.isIntegerFast())
                dst.asIntegerStore(g->asIntegerUnsafe());
            else
                dst = *g;
            return;
        }
        throw std::runtime_error("in global object Not found property");
    }
    DISPATCH_OP_METHOD(GGlobal) {
        const Atom atom{ read<u32>(ip, pc) };
        const u16 dst = read<u16>(ip, pc);
        loadGlobal(atom, regs[dst]);
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(GThis) {
        const u32 atom = read<u32>(ip, pc);
        const u16 r1 = read<u16>(ip, pc);
        if(!cur->thisData && (!cur->closure || !cur->closure->funcMeta)) {
            loadGlobal(Atom{ atom }, regs[r1]);
            return DispatchRet::Continue;
        }
        Value srcVal = getThis({ atom });
        propObjectGet(*this, srcVal, srcVal);
        regs[r1] = srcVal;
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(DThis) {
        const u32 atom = read<u32>(ip, pc);
        const u16 r1 = read<u16>(ip, pc);
        const Atom a{ atom };
        const Value &srcVal = regs[r1];
        if(!cur->thisData && (!cur->closure || !cur->closure->funcMeta) && srcVal.isIntegerFast()) {
            if(const Value *slot = cachedGlobal(a); slot && slot->isIntegerFast()) {
                const_cast<Value *>(slot)->asIntegerStore(srcVal.asIntegerUnsafe());
                return DispatchRet::Continue;
            }
        }

        if(hasThis(a)) {
            if(propObjectSet(*this, srcVal, getThis(a))) {
                cur->pc = pc;
                return DispatchRet::Call;
            }
        }

        setThis(a, srcVal);
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(GLocal) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        // 局部变量读取是纯拷贝（TJS2 对应 VM_CP）：不触发 property getter。
        // 否则 &prop 引用存入局部变量后一读就被触发，Property 无法作为一等值传递；
        // 属性触发只应发生在对象成员访问（GProp/GThis/GGlobal）
        regs[r2] = regs[r1];
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(DLocal) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        // 局部变量写入是纯写：目标槽位即使持有 Property 对象（& 引用）也不调 setter，
        // setter 只在写 this/global 成员（DThis/DGlobal）或 *p 解引用（DerefSet）时触发
        regs[r2] = regs[r1];
        return DispatchRet::Continue;
    }

    // ── 算术/位运算（三寄存器） ───────────────────────────────
    // 整数特化：双 int 时绕开 Ret<Value> 装配（optional 构造/析构），
    // 直接写寄存器槽——循环体算术的主路径
#define DISPATCH_RRR_INT(op, optr, method) \
    DISPATCH_OP_METHOD(op) { \
        const u16 r1 = read<u16>(ip, pc); \
        const u16 r2 = read<u16>(ip, pc); \
        const u16 r3 = read<u16>(ip, pc); \
        Value &a = regs[r1]; \
        Value &b = regs[r2]; \
        if(a.isIntegerFast() && b.isIntegerFast()) { \
            const Integer result = a.asIntegerUnsafe() optr b.asIntegerUnsafe(); \
            if(regs[r3].isIntegerFast()) regs[r3].asIntegerStore(result); \
            else regs[r3] = Value{ result }; \
            return DispatchRet::Continue; \
        } \
        regs[r3] = (a.method(b)).unwrap(); \
        return DispatchRet::Continue; \
    }

#define DISPATCH_RRR(op, expr)                                                                                          \
    DISPATCH_OP_METHOD(op) {                                                                                            \
        const u16 r1 = read<u16>(ip, pc);                                                                               \
        const u16 r2 = read<u16>(ip, pc);                                                                               \
        const u16 r3 = read<u16>(ip, pc);                                                                               \
        regs[r3] = (expr);                                                                                              \
        return DispatchRet::Continue;                                                                                    \
    }

    DISPATCH_RRR_INT(Add, +, add)
    DISPATCH_RRR_INT(Sub, -, sub)
    DISPATCH_RRR_INT(Mul, *, mul)
    DISPATCH_RRR(Div, (regs[r1].div(regs[r2]).unwrap()))
    DISPATCH_RRR(Idiv, (regs[r1].idiv(regs[r2]).unwrap()))
    DISPATCH_RRR(Mod, (regs[r1].mod(regs[r2]).unwrap()))

    DISPATCH_RRR(BXor, (regs[r1].bitwiseXor(regs[r2]).unwrap()))
    DISPATCH_RRR(BOr, (regs[r1].bitwiseOr(regs[r2]).unwrap()))
    DISPATCH_RRR(BAnd, (regs[r1].bitwiseAnd(regs[r2]).unwrap()))
    DISPATCH_RRR(BlShift, (regs[r1].bitwiseLeftShift(regs[r2]).unwrap()))
    DISPATCH_RRR(BrShift, (regs[r1].bitwiseRightShift(regs[r2]).unwrap()))
    DISPATCH_RRR(BurShift, (regs[r1].bitwiseUnsignedRightShift(regs[r2]).unwrap()))

    // ── 比较/逻辑（结果为 bool） ──────────────────────────────
#define DISPATCH_CMP(op, expr)                                                                                          \
    DISPATCH_OP_METHOD(op) {                                                                                            \
        const u16 r1 = read<u16>(ip, pc);                                                                               \
        const u16 r2 = read<u16>(ip, pc);                                                                               \
        const u16 r3 = read<u16>(ip, pc);                                                                               \
        regs[r3] = Value{ (expr) };                                                                                     \
        return DispatchRet::Continue;                                                                                    \
    }

    DISPATCH_CMP(EQ, regs[r1].equals(regs[r2]))
    DISPATCH_CMP(NEQ, !regs[r1].equals(regs[r2]))
    DISPATCH_CMP(AbsEQ, regs[r1].discernEquals(regs[r2]))
    DISPATCH_CMP(AbsNEQ, !regs[r1].discernEquals(regs[r2]))
    DISPATCH_OP_METHOD(LT) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        const u16 r3 = read<u16>(ip, pc);
        const Value &a = regs[r1];
        const Value &b = regs[r2];
        if(a.isIntegerFast() && b.isIntegerFast()) {
            regs[r3] = Value{ Integer{ a.asIntegerUnsafe() < b.asIntegerUnsafe() } };
            return DispatchRet::Continue;
        }
        regs[r3] = Value{ a.littlerThan(b).unwrap() };
        return DispatchRet::Continue;
    }
    DISPATCH_CMP(LE, !regs[r1].greaterThan(regs[r2]).unwrap())
    DISPATCH_CMP(GT, regs[r1].greaterThan(regs[r2]).unwrap())
    DISPATCH_CMP(GE, !regs[r1].littlerThan(regs[r2]).unwrap())
    DISPATCH_CMP(LAnd, regs[r1].logicalAnd(regs[r2]))
    DISPATCH_CMP(LOr, regs[r1].logicalOr(regs[r2]))

    // ── M3：比较直写 _zf（省 %F 物化与 Test，配 JmpE/JmpNE 直连分支） ──
    DISPATCH_OP_METHOD(CompareTest) {
        const u8 kind = ip[pc++];
        const u16 lhs = read<u16>(ip, pc);
        const u16 rhs = read<u16>(ip, pc);
        const u16 dst = read<u16>(ip, pc);
        const Value &a = regs[lhs], &b = regs[rhs];
        bool result{};
        if(a.isIntegerFast() && b.isIntegerFast()) {
            const Integer x = a.asIntegerUnsafe(), y = b.asIntegerUnsafe();
            switch(kind) {
                case 0:
                case 2:
                    result = x == y;
                    break;
                case 1:
                case 3:
                    result = x != y;
                    break;
                case 4:
                    result = x < y;
                    break;
                case 5:
                    result = x <= y;
                    break;
                case 6:
                    result = x > y;
                    break;
                case 7:
                    result = x >= y;
                    break;
                default:
                    throw std::runtime_error("invalid fused comparison");
            }
        } else {
            switch(kind) {
                case 0:
                    result = a.equals(b);
                    break;
                case 1:
                    result = !a.equals(b);
                    break;
                case 2:
                    result = a.discernEquals(b);
                    break;
                case 3:
                    result = !a.discernEquals(b);
                    break;
                case 4:
                    result = a.littlerThan(b).unwrap();
                    break;
                case 5:
                    result = !a.greaterThan(b).unwrap();
                    break;
                case 6:
                    result = a.greaterThan(b).unwrap();
                    break;
                case 7:
                    result = !a.littlerThan(b).unwrap();
                    break;
                default:
                    throw std::runtime_error("invalid fused comparison");
            }
        }
        if(regs[dst].isIntegerFast())
            regs[dst].asIntegerStore(Integer{ result });
        else
            regs[dst] = Value{ Integer{ result } };
        _zf = result;
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(CmpEQ) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        _zf = regs[r1].equals(regs[r2]);
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(CmpDEQ) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        _zf = regs[r1].discernEquals(regs[r2]);
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(CmpLT) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        _zf = regs[r1].littlerThan(regs[r2]).unwrap();
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(CmpGT) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        _zf = regs[r1].greaterThan(regs[r2]).unwrap();
        return DispatchRet::Continue;
    }

    // 自增/自减单指令（旧版 INC/DEC 基型）：int ±1 / real ±1.0、其余报错
    //（TJS increment 语义；字符串不拼接）
    DISPATCH_OP_METHOD(Inc) {
        const u16 r1 = read<u16>(ip, pc);
        Value &v = regs[r1];
        if(v.isIntegerFast())
            v = Value{ Integer{ v.asIntegerUnsafe() + 1 } };
        else if(v.isReal())
            v = Value{ Real{ v.asReal().unwrap().value() + 1.0 } };
        else
            throw std::runtime_error("increment expects a number");
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(Dec) {
        const u16 r1 = read<u16>(ip, pc);
        Value &v = regs[r1];
        if(v.isIntegerFast())
            v = Value{ Integer{ v.asIntegerUnsafe() - 1 } };
        else if(v.isReal())
            v = Value{ Real{ v.asReal().unwrap().value() - 1.0 } };
        else
            throw std::runtime_error("decrement expects a number");
        return DispatchRet::Continue;
    }

    // 按位非（旧版 BNOT/bitnot）
    DISPATCH_OP_METHOD(BNot) {
        const u16 r1 = read<u16>(ip, pc); // src
        const u16 r2 = read<u16>(ip, pc); // dst
        Value v = regs[r1];
        if(const auto r = v.bitNot(); r.isFailed())
            throw r.getErr();
        regs[r2] = v;
        return DispatchRet::Continue;
    }

    // 数值规范化（旧版 NUM/tonumber）
    DISPATCH_OP_METHOD(ToNumber) {
        const u16 r1 = read<u16>(ip, pc); // src
        const u16 r2 = read<u16>(ip, pc); // dst
        Value v = regs[r1];
        if(const auto r = v.toNumber(); r.isFailed())
            throw r.getErr();
        regs[r2] = v;
        return DispatchRet::Continue;
    }

    // 转 octet（旧版 OCTET/ToOctet）：void → 空 octet、octet 不变、其余报错
    DISPATCH_OP_METHOD(ToOctet) {
        const u16 r1 = read<u16>(ip, pc); // src
        const u16 r2 = read<u16>(ip, pc); // dst
        const Value &v = regs[r1];
        if(v.isOctet()) {
            regs[r2] = v;
            return DispatchRet::Continue;
        }
        if(v.isVoid()) {
            auto *payload = SharedPayload<Octet>::makeSingle(Octet(nullptr, 0));
            regs[r2] = Value{};
            regs[r2].type(ValueType::Octet);
            regs[r2].octetStore(payload);
            return DispatchRet::Continue;
        }
        throw std::runtime_error("cannot convert to octet");
    }

    // ── 调用/属性 ────────────────────────────────────────────
    DISPATCH_OP_METHOD(Call) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        const i64 imm = decodeSleb128(ip, pc, 10);
        const auto &object = regs[r2].asObject().unwrap();
        cur->pc = pc;
        object->call(*this, r1, imm);
        return DispatchRet::Call;
    }

    // 变长实参调用。Function 被调者走实参直写：一次分配被调帧窗口，实参寄存器
    // 直接拷入 %0..%arity-1（缺位补 void，超额/hasCollapse 按调用序备份窗口尾区，
    // 语义与 Function::call 一致），省去 N×push 的池顶中转与逐次 allocFrame。
    // 其余可调用对象（NativeFunction/类对象/property 闭包/容器）回落压栈 + 虚
    // call 旧路径（压帧约定：池顶实参区与被调 %0.. 重合）。
    // 两路径都先读全寄存器号——压栈/扩池后再读 ip 无碍，但不依赖边读边写
    DISPATCH_OP_METHOD(CallV) {
        constexpr u64 maxCallArgs = 64;
        const u16 dst = read<u16>(ip, pc);
        const u16 func = read<u16>(ip, pc);
        const u64 argc = decodeUleb128(ip, pc, 10);
        if(argc > maxCallArgs)
            throw std::runtime_error("too many arguments");
        u16 argRegs[maxCallArgs]{};
        for(u64 i = 0; i < argc; ++i)
            argRegs[i] = read<u16>(ip, pc);

        return invokeCallV(cur, regs, dst, func, argRegs, argc, pc);
    }

    DISPATCH_OP_METHOD(CallMethodV) {
        const u16 receiverReg = read<u16>(ip, pc);
        const u16 dst = read<u16>(ip, pc);
        const u16 func = read<u16>(ip, pc);
        const u64 argc = decodeUleb128(ip, pc, 10);
        if(argc > 64)
            throw std::runtime_error("too many arguments");
        u16 args[64]{};
        for(u64 i = 0; i < argc; ++i)
            args[i] = read<u16>(ip, pc);
        return invokeCallV(cur, regs, dst, func, args, argc, pc, &regs[receiverReg]);
    }

    DispatchRet VMState::invokeCallV(CallFrame *cur, Value *regs, const u16 dst, const u16 func, const u16 *argRegs,
                                     const u64 argc, const u64 pc, const Value *receiver) {
        const Value &funcVal = regs[func];
        if(funcVal.isObject()) {
            auto *callee = funcVal.asObject().unwrap();
            // typeId 一次虚调用判型（替代 dynamic_cast）；NativeFunction 保持
            // Object 标识走通用回落
            if(callee->typeId() == TypeId::Function) {
                auto *fn = static_cast<Function *>(callee);
                const FuncMeta *meta = fn->meta;
                const u32 arity = meta->arity;
                const u32 argc32 = static_cast<u32>(argc);
                const bool needTail = argc32 > arity || meta->hasCollapse;
                // 压帧前取：调用者 this（调用点绑定）与函数的绑定 this（incontextof）
                const Value callerThis = cur->thisObj;
                Object *boundThis = fn->thisObj;

                // 直写布局：%0 = 新分配基址，窗口整体在调用者上方（无槽位别名）
                allocCallFrame(fn->meta, dst, needTail ? argc : 0, /*argWindowDirect=*/true);
                CallFrame *nf = curFrame();
                nf->actualArgc = argc32;
                Value *w = nf->regs();
                const u32 head = argc32 < arity ? argc32 : arity;
                for(u32 i = 0; i < head; ++i)
                    w[i] = regs[argRegs[i]];
                for(u32 i = head; i < arity; ++i)
                    w[i] = Value{}; // 缺位实参补 void（prologue TestVoid 守卫依赖）
                if(needTail) {
                    // 超额实参/collapse 声明：全部实参按调用序备份到窗口尾区
                    //（frameArg 读取源；CollapseArgs 写参数槽不丢原始值）
                    const u16 tail = meta->chunk->getRegCount();
                    for(u32 j = 0; j < argc32; ++j)
                        w[tail + j] = regs[argRegs[j]];
                }
                if(boundThis) {
                    nf->setThisObj(Value{ boundThis });
                } else if(callerThis.isObject()) {
                    // TJS2 调用点绑定语义：clo.ObjThis 为空时 this 取调用者的 this
                    nf->setThisObj(callerThis);
                } else {
                    nf->setThisObjNull();
                }
                // 交还主循环前同步调用者 pc（被调帧 Return 后从这继续）
                cur->pc = pc;
                return DispatchRet::Call;
            }
        }

        for(u64 i = 0; i < argc; ++i)
            push(regs[argRegs[i]]);
        cur->pc = pc;
        if(receiver && !receiver->isVoid())
            static_cast<NativeFunction *>(regs[func].asObject().unwrap())->callWithThis(*this, dst, argc, *receiver);
        else
            regs[func].asObject().unwrap()->call(*this, dst, argc);
        return DispatchRet::Call;
    }

    namespace {
        // 下标 → Array 的 0 基位置；负数从尾数（-1 = 末元素）；越界返回 npos
        constexpr size_t npos = static_cast<size_t>(-1);
        size_t arrayIndex(const i64 idx, const size_t n) {
            if(idx >= 0)
                return static_cast<u64>(idx) < n ? static_cast<size_t>(idx) : npos;
            const auto u = static_cast<u64>(-idx);
            return u <= n && u > 0 ? n - static_cast<size_t>(u) : npos;
        }
    } // namespace

    // 本帧实际实参读取：超额调用（argCount > arity）时全部实参由
    // Function::call 按调用顺序复制在窗口顶部保留区 regCount..；
    // 未超额时 %0..%actualArgc-1 即实参（与池顶重合）
    static Value frameArg(CallFrame *cur, const Value *regs, const u32 i) {
        // collapse 声明的帧：Function::call 已把全部实参按调用序备份到尾区
        //（CollapseArgs 会覆盖参数槽，原始值只在尾区）
        if(cur->funcMeta && cur->funcMeta->hasCollapse)
            return regs[cur->chunk->getRegCount() + i];
        if(cur->actualArgc > cur->funcMeta->arity)
            return regs[cur->chunk->getRegCount() + i];
        return regs[i];
    }

    // 省略透传调用（旧版 argc=-1，TJS `f(...)`）：把本帧实际实参
    // %0..%actualArgc-1 原样转发。压池可能迁移寄存器窗口，先拷贝再压
    DISPATCH_OP_METHOD(CallOmit) {
        const u16 r1 = read<u16>(ip, pc); // dst
        const u16 r2 = read<u16>(ip, pc); // func
        const u32 argc = cur->actualArgc;
        Vec<Value> args{};
        args.reserve(argc);
        for(u32 i = 0; i < argc; ++i)
            args.push_back(frameArg(cur, regs, i));
        // 被调者与实参都在压池前取值：push 扩池会迁移寄存器窗口使 regs 悬空
        const Value callee = regs[r2];
        for(const auto &v : args)
            push(v);
        cur->pc = pc;
        callee.asObject().unwrap()->call(*this, r1, static_cast<size_t>(argc));
        return DispatchRet::Call;
    }

    // 展开调用（旧版 argc=-2，TJS `f(1, arr*, *)`）：entries 逐条
    // Normal/Expand(Array 元素)/UnnamedExpand(本帧 unnamed 实参) 收集后统一压池
    DISPATCH_OP_METHOD(CallExpand) {
        const u16 r1 = read<u16>(ip, pc);  // dst
        const u16 r2 = read<u16>(ip, pc);  // func
        const u64 unnamedBase = decodeUleb128(ip, pc, 10);
        const u64 count = decodeUleb128(ip, pc, 10);
        Vec<Value> args{};
        for(u64 k = 0; k < count; ++k) {
            const u8 type = ip[pc++]; // 1 字节 fatType（read<> 需 ≥2 字节）
            const u16 reg = read<u16>(ip, pc);
            switch(type) {
                case 0: // fatNormal
                    args.push_back(regs[reg]);
                    break;
                case 1: { // fatExpand：展开 Array 元素
                    const auto &obj = regs[reg];
                    auto *arr = obj.isObject()
                        ? dynamic_cast<Array *>(obj.asObject().unwrap())
                        : nullptr;
                    if(!arr)
                        throw std::runtime_error("expand call '*' expects an array");
                    for(auto &el : arr->elements)
                        args.push_back(el);
                    break;
                }
                default: { // fatUnnamedExpand：本帧 unnamed 实参（%base..）
                    const u32 base = static_cast<u32>(unnamedBase);
                    for(u32 i = base; i < cur->actualArgc; ++i)
                        args.push_back(frameArg(cur, regs, i));
                    break;
                }
            }
        }
        // 被调者在压池前取值（同 CallOmit）
        const Value callee = regs[r2];
        for(const auto &v : args)
            push(v);
        cur->pc = pc;
        callee.asObject().unwrap()->call(*this, r1, args.size());
        return DispatchRet::Call;
    }

    // collapse 参数收拢（旧版 FuncDeclCollapseBase，TJS `function f(a, b*)`）：
    // 本帧实际实参 %base..%actualArgc-1 收拢成新 Array 写 dst；无超额实参时
    // 写空 Array（TJS2 无条件建 Array）
    DISPATCH_OP_METHOD(CollapseArgs) {
        const u16 r1 = read<u16>(ip, pc); // dst（TJS 位置 ra[-3-base]）
        const u64 base = decodeUleb128(ip, pc, 10);
        const u32 argc = cur->actualArgc;
        const size_t n = argc > base ? argc - base : 0;
        auto arr = rt.create<Array>(&rt, n);
        for(size_t i = 0; i < n; ++i)
            arr->elements[i] = frameArg(cur, regs, static_cast<u32>(base + i));
        regs[r1] = Value{ arr.get() };
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(GProp) {
        const u16 r1 = read<u16>(ip, pc); // obj
        const u16 r2 = read<u16>(ip, pc); // member
        const u16 r3 = read<u16>(ip, pc); // dst
        const auto &val = regs[r1];
        // 整数成员名（旧版 PropGetByNum，TJS `a[0]` 编译为 GPI+整数名）：数组下标
        if(regs[r2].isInteger()) {
            auto *arr = val.isObject() ? dynamic_cast<Array *>(val.asObject().unwrap())
                                       : nullptr;
            if(!arr)
                throw std::runtime_error("integer member access expects an array");
            const auto i = regs[r2].asInteger().unwrap();
            const size_t pos = arrayIndex(i, arr->elements.size());
            if(pos == npos)
                throw std::runtime_error("array index out of range");
            regs[r3] = arr->elements[pos];
            return DispatchRet::Continue;
        }
        const String &name = *regs[r2].asString().unwrap();
        propGetMember(*this, cur, val, rt.atomTable.intern(name), regs[r3]);
        return DispatchRet::Continue;
    }

    // 成员名编译期 Atom 直引（同 GProp，跳过名字寄存器与 dispatch 再 intern）
    const VMState::NativeMemberCache &VMState::nativeMember(TypeId type, Atom atom, u64 site) {
        auto &entry = _nativeCache[(site ^ atom.v ^ static_cast<u64>(type)) % 64];
        const u64 revision = context.nativeRevision();
        if(entry.type != type || entry.atom != atom || entry.revision != revision) {
            entry = { type, atom, revision, context.findMethod(type, atom), context.findProperty(type, atom) };
        }
        return entry;
    }

    DISPATCH_OP_METHOD(GMethodC) {
        const u16 obj = read<u16>(ip, pc);
        const Atom atom{ read<u32>(ip, pc) };
        const u16 dst = read<u16>(ip, pc);
        const u16 receiver = read<u16>(ip, pc);
        const Value value = regs[obj];
        const TypeId type = ValueToTypeId::getId(value);
        const bool native = !value.isObject() || type == TypeId::Array || type == TypeId::Dictionary ||
            type == TypeId::RegExp || type == TypeId::Math || type == TypeId::Date || type == TypeId::RandomGen;
        if(native) {
            const auto &entry = nativeMember(type, atom, pc);
            if(entry.method && !entry.getter) {
                regs[receiver] = value;
                regs[dst] = Value{ entry.method };
                return DispatchRet::Continue;
            }
        }
        regs[receiver] = Value{};
        propGetMember(*this, cur, value, atom, regs[dst]);
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(GPropC) {
        const u16 r1 = read<u16>(ip, pc);    // obj
        const u32 atomV = read<u32>(ip, pc); // 成员名 Atom
        const u16 r3 = read<u16>(ip, pc);    // dst
        propGetMember(*this, cur, regs[r1], Atom{ atomV }, regs[r3]);
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(DProp) {
        const u16 r1 = read<u16>(ip, pc); // obj
        const u16 r2 = read<u16>(ip, pc); // member
        const u16 r3 = read<u16>(ip, pc); // src
        const auto &objVal = regs[r1];
        // 整数成员名（旧版 PropSetByNum）：数组下标写，越界自动扩展补 void
        if(regs[r2].isInteger()) {
            if(!objVal.isObject())
                throw std::runtime_error("dprop obj is not object");
            auto *arr = dynamic_cast<Array *>(objVal.asObject().unwrap());
            if(!arr)
                throw std::runtime_error("integer member access expects an array");
            const auto i = regs[r2].asInteger().unwrap();
            if(i >= 0) {
                const auto u = static_cast<u64>(i);
                if(u >= arr->elements.size())
                    arr->elements.resize(static_cast<size_t>(u) + 1);
                arr->elements[static_cast<size_t>(u)] = regs[r3];
            } else {
                const auto u = static_cast<u64>(-i);
                if(u == 0 || u > arr->elements.size())
                    throw std::runtime_error("array index out of range");
                arr->elements[arr->elements.size() - static_cast<size_t>(u)] = regs[r3];
            }
            return DispatchRet::Continue;
        }
        const String &name = *regs[r2].asString().unwrap();
        if(propSetMember(*this, objVal, rt.atomTable.intern(name), regs[r3])) {
            cur->pc = pc;
            return DispatchRet::Call;
        }
        return DispatchRet::Continue;
    }

    // 成员名编译期 Atom 直引（同 DProp，跳过名字寄存器与 dispatch 再 intern）
    DISPATCH_OP_METHOD(DPropC) {
        const u16 r1 = read<u16>(ip, pc);    // obj
        const u32 atomV = read<u32>(ip, pc); // 成员名 Atom
        const u16 r3 = read<u16>(ip, pc);    // src
        if(propSetMember(*this, regs[r1], Atom{ atomV }, regs[r3])) {
            cur->pc = pc;
            return DispatchRet::Call;
        }
        return DispatchRet::Continue;
    }

    // &obj.member = v：写槽位本体，绕过 setter（旧版 SPDS）——& 作赋值左值，
    // 用于注册/覆盖 property 引用成员
    DISPATCH_OP_METHOD(DPropRaw) {
        const u16 r1 = read<u16>(ip, pc); // obj
        const u16 r2 = read<u16>(ip, pc); // member
        const u16 r3 = read<u16>(ip, pc); // src
        const auto &objVal = regs[r1];
        if(!objVal.isObject()) {
            throw std::runtime_error("'&' operand is not an object");
        }
        if(regs[r2].isInteger()) { // 整数成员名：数组下标写（越界扩展补 void）
            auto *arr = dynamic_cast<Array *>(objVal.asObject().unwrap());
            if(!arr)
                throw std::runtime_error("integer member access expects an array");
            const auto i = regs[r2].asInteger().unwrap();
            if(i >= 0) {
                const auto u = static_cast<u64>(i);
                if(u >= arr->elements.size())
                    arr->elements.resize(static_cast<size_t>(u) + 1);
                arr->elements[static_cast<size_t>(u)] = regs[r3];
            } else {
                const auto u = static_cast<u64>(-i);
                if(u == 0 || u > arr->elements.size())
                    throw std::runtime_error("array index out of range");
                arr->elements[arr->elements.size() - static_cast<size_t>(u)] = regs[r3];
            }
            return DispatchRet::Continue;
        }
        const String &name = *regs[r2].asString().unwrap();
        objVal.asObject().unwrap()->setProp(rt.atomTable.intern(name), regs[r3]);
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(DPropRawC) {
        const u16 r1 = read<u16>(ip, pc);    // obj
        const u32 atomV = read<u32>(ip, pc); // 成员名 Atom
        const u16 r3 = read<u16>(ip, pc);    // src
        const auto &objVal = regs[r1];
        if(!objVal.isObject()) {
            throw std::runtime_error("'&' operand is not an object");
        }
        objVal.asObject().unwrap()->setProp(Atom{ atomV }, regs[r3]);
        return DispatchRet::Continue;
    }

    // delete obj.member：删除成员，dst 写入是否删除成功（bool，TJS2 DeleteMember 语义）
    DISPATCH_OP_METHOD(DelProp) {
        const u16 r1 = read<u16>(ip, pc); // obj
        const u16 r2 = read<u16>(ip, pc); // member
        const u16 r3 = read<u16>(ip, pc); // dst
        const auto &objVal = regs[r1];
        if(!objVal.isObject()) {
            regs[r3] = Value{ Integer{ false } };
            return DispatchRet::Continue;
        }
        const String &name = *regs[r2].asString().unwrap();
        regs[r3] = Value{ Integer{ objVal.asObject().unwrap()->delProp(rt.atomTable.intern(name)) } };
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(DelPropC) {
        const u16 r1 = read<u16>(ip, pc);    // obj
        const u32 atomV = read<u32>(ip, pc); // 成员名 Atom
        const u16 r3 = read<u16>(ip, pc);    // dst
        const auto &objVal = regs[r1];
        if(!objVal.isObject()) {
            regs[r3] = Value{ Integer{ false } };
            return DispatchRet::Continue;
        }
        regs[r3] = Value{ Integer{ objVal.asObject().unwrap()->delProp(Atom{ atomV }) } };
        return DispatchRet::Continue;
    }

    // &obj.member：取成员本体（旧版 GPDS）——property 不触发 getter，直接得到
    // Property 对象；普通成员与 GProp 等价
    DISPATCH_OP_METHOD(GPropRaw) {
        const u16 r1 = read<u16>(ip, pc); // obj
        const u16 r2 = read<u16>(ip, pc); // member
        const u16 r3 = read<u16>(ip, pc); // dst
        const auto &val = regs[r1];
        if(regs[r2].isInteger()) { // 整数成员名：数组下标读
            auto *arr = val.isObject() ? dynamic_cast<Array *>(val.asObject().unwrap())
                                       : nullptr;
            if(!arr)
                throw std::runtime_error("integer member access expects an array");
            const size_t pos = arrayIndex(regs[r2].asInteger().unwrap(), arr->elements.size());
            if(pos == npos)
                throw std::runtime_error("array index out of range");
            regs[r3] = arr->elements[pos];
            return DispatchRet::Continue;
        }
        const String &name = *regs[r2].asString().unwrap();
        if(!val.isObject()) {
            throw std::runtime_error("'&' operand is not an object");
        }
        regs[r3] = val.asObject().unwrap()->getProp(rt.atomTable.intern(name));
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(GPropRawC) {
        const u16 r1 = read<u16>(ip, pc);    // obj
        const u32 atomV = read<u32>(ip, pc); // 成员名 Atom
        const u16 r3 = read<u16>(ip, pc);    // dst
        const auto &val = regs[r1];
        if(!val.isObject()) {
            throw std::runtime_error("'&' operand is not an object");
        }
        regs[r3] = val.asObject().unwrap()->getProp(Atom{ atomV });
        return DispatchRet::Continue;
    }

    // &identifier：取 this/global 槽位本体（旧版 GPDS 的符号形态）——
    // 不触发 getter；getThis 内置 thisData→closure→global 的查找
    DISPATCH_OP_METHOD(GRefRaw) {
        const u32 atom = read<u32>(ip, pc);
        const u16 r1 = read<u16>(ip, pc);
        regs[r1] = getThis({ atom });
        return DispatchRet::Continue;
    }

    // *p：强制属性访问读（旧版 GETP）——Property 调 getter（invokeGet 内嵌套 run0
    // 同步完成）；非 Property 按 TJS2 语义报错
    DISPATCH_OP_METHOD(DerefGet) {
        const u16 r1 = read<u16>(ip, pc); // prop
        const u16 r2 = read<u16>(ip, pc); // dst
        const auto &propVal = regs[r1];
        if(!propVal.isObject()) {
            throw std::runtime_error("'*' operand is not a property");
        }
        auto *prop = dynamic_cast<Property *>(propVal.asObject().unwrap());
        if(!prop) {
            throw std::runtime_error("'*' operand is not a property");
        }
        regs[r2] = prop->invokeGet(*this);
        return DispatchRet::Continue;
    }

    // *p = v：强制属性访问写（旧版 SETP）——Property 调 setter；invokeSet 只压帧
    // 不执行，与 DGlobal 同款：检测栈变化后交还主循环（DispatchRet::Call）
    DISPATCH_OP_METHOD(DerefSet) {
        const u16 r1 = read<u16>(ip, pc); // prop
        const u16 r2 = read<u16>(ip, pc); // src
        const auto &propVal = regs[r1];
        if(!propVal.isObject()) {
            throw std::runtime_error("'*' operand is not a property");
        }
        auto *prop = dynamic_cast<Property *>(propVal.asObject().unwrap());
        if(!prop) {
            throw std::runtime_error("'*' operand is not a property");
        }
        const size_t top = _stackTop;
        prop->invokeSet(*this, regs[r2]);
        if(_stackTop != top) {
            cur->pc = pc;
            return DispatchRet::Call;
        }
        return DispatchRet::Continue;
    }

    // [a, b, ...]：分配 count 长度的新 Array（元素 void，随后逐个 SetIndex 填充）
    DISPATCH_OP_METHOD(NewArray) {
        const u16 r1 = read<u16>(ip, pc);
        const i64 count = decodeSleb128(ip, pc, 10);
        regs[r1] = Value{ rt.create<Array>(&rt, static_cast<size_t>(count < 0 ? 0 : count)).get() };
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(NewDict) {
        const u16 r1 = read<u16>(ip, pc);
        regs[r1] = Value{ rt.create<Dictionary>().get() };
        return DispatchRet::Continue;
    }

    // a[i] 读：数组整数下标（负=从尾，越界报脚本异常）、字典键（字符串化）、
    // 字符串字符（保持旧 charAt 字节语义：越界得空串）、其余对象按成员名访问
    DISPATCH_OP_METHOD(GetIndex) {
        const u16 r1 = read<u16>(ip, pc); // obj
        const u16 r2 = read<u16>(ip, pc); // idx
        const u16 r3 = read<u16>(ip, pc); // dst
        const Value &obj = regs[r1];
        const Value &idx = regs[r2];
        if(obj.isObject()) {
            if(auto *arr = dynamic_cast<Array *>(obj.asObject().unwrap())) {
                const auto i = idx.isVoid() ? 0 : idx.asInteger().unwrap();
                const size_t pos = arrayIndex(i, arr->elements.size());
                if(pos == npos)
                    throw std::runtime_error("array index out of range");
                regs[r3] = arr->elements[pos];
                return DispatchRet::Continue;
            }
            if(dynamic_cast<Dictionary *>(obj.asObject().unwrap()) != nullptr) {
                Value key = idx;
                if(key.isVoid()) {
                    throw std::runtime_error("dictionary key is void");
                }
                if(const auto r = key.toString(); r.isFailed()) {
                    throw std::runtime_error("dictionary key must be convertible to string");
                }
                const Atom atom = rt.atomTable.intern(*key.asString().unwrap());
                regs[r3] = obj.asObject().unwrap()->getProp(atom);
                return DispatchRet::Continue;
            }
            // 其余对象：成员名访问
            Value key = idx;
            if(const auto r = key.toString(); r.isFailed()) {
                throw std::runtime_error("index must be convertible to string");
            }
            const Atom atom = rt.atomTable.intern(*key.asString().unwrap());
            regs[r3] = obj.asObject().unwrap()->getProp(atom);
            return DispatchRet::Continue;
        }
        if(obj.isString()) {
            const String *str = obj.asString().unwrap();
            const auto i = idx.isVoid() ? 0 : idx.asInteger().unwrap();
            const auto n = static_cast<i64>(str->length());
            // TJS2 字符串下标语义（语料 test_string）：str[len] 得空串，str[>len] 报错，
            // 负数从尾数（-1 = 末字符）
            if(i >= 0) {
                if(i > n)
                    throw std::runtime_error("string index out of range");
                regs[r3] = i == n ? Value{ String(""_str) } : Value{ String(str->getData()[static_cast<size_t>(i)]) };
                return DispatchRet::Continue;
            }
            const auto u = -i;
            if(u > n)
                throw std::runtime_error("string index out of range");
            regs[r3] = u == n ? Value{ String(""_str) } : Value{ String(str->getData()[static_cast<size_t>(n - u)]) };
            return DispatchRet::Continue;
        }
        throw std::runtime_error("value is not indexable");
    }

    // a[i] = v 写：数组越界自动扩展补 void（TJS2 行为）；负下标从尾；
    // 字典键写入；其余对象按成员名写入
    DISPATCH_OP_METHOD(SetIndex) {
        const u16 r1 = read<u16>(ip, pc); // obj
        const u16 r2 = read<u16>(ip, pc); // idx
        const u16 r3 = read<u16>(ip, pc); // src
        const Value &obj = regs[r1];
        const Value &idx = regs[r2];
        if(!obj.isObject()) {
            throw std::runtime_error("value is not indexable");
        }
        if(auto *arr = dynamic_cast<Array *>(obj.asObject().unwrap())) {
            const auto i = idx.isVoid() ? 0 : idx.asInteger().unwrap();
            if(i >= 0) {
                const auto u = static_cast<u64>(i);
                if(u >= arr->elements.size())
                    arr->elements.resize(static_cast<size_t>(u) + 1);
                arr->elements[static_cast<size_t>(u)] = regs[r3];
                return DispatchRet::Continue;
            }
            const auto u = static_cast<u64>(-i);
            if(u == 0 || u > arr->elements.size())
                throw std::runtime_error("array index out of range");
            arr->elements[arr->elements.size() - static_cast<size_t>(u)] = regs[r3];
            return DispatchRet::Continue;
        }
        Value key = idx;
        if(key.isVoid()) {
            throw std::runtime_error("index is void");
        }
        if(const auto r = key.toString(); r.isFailed()) {
            throw std::runtime_error("index must be convertible to string");
        }
        const Atom atom = rt.atomTable.intern(*key.asString().unwrap());
        obj.asObject().unwrap()->setProp(atom, regs[r3]);
        return DispatchRet::Continue;
    }

    // with(obj)：压入 with 上下文栈（裸词链 .member 的查找基座）；
    // 对象挂 Runtime handle 保 GC，帧释放/异常 unwind 时随帧截断
    DISPATCH_OP_METHOD(WithPush) {
        const u16 r1 = read<u16>(ip, pc);
        if(!regs[r1].isObject()) {
            throw std::runtime_error("with expects an object");
        }
        auto *obj = regs[r1].asObject().unwrap();
        rt.addHandleVal(obj);
        withStack.push_back(obj);
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(WithPop) {
        if(!withStack.empty()) {
            rt.removeHandleVal(withStack.back());
            withStack.pop_back();
        }
        return DispatchRet::Continue;
    }

    // 裸词链基座 .member：取 with 栈顶对象；空栈（无 with 包裹）得 global——
    // TJS2 顶层裸词链语义（语料 test_with 的 .gvarl = 7 写全局）
    DISPATCH_OP_METHOD(GetWith) {
        const u16 r1 = read<u16>(ip, pc);
        if(withStack.empty())
            regs[r1] = Value{ context.global() };
        else
            regs[r1] = Value{ withStack.back() };
        return DispatchRet::Continue;
    }

    // /pattern/flags 正则字面量：常量为 lexer 编码的 "pattern\nflags"（无 flags 时尾随 '\n'）
    // → RegExp 对象（构造时编译，非法模式抛脚本异常）
    DISPATCH_OP_METHOD(NewRegExp) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 cIdx = read<u16>(ip, pc);
        const auto &constant = constants[cIdx];
        // 短串内联在 Value 内，不能借用 createValue 临时对象的字符串。
        const Value encodedValue = constant.createValue(&rt);
        const String &encoded = *encodedValue.asString().unwrap();
        const char *data = encoded.getData();
        const u32 len = encoded.length();
        u32 sep = 0;
        while(sep < len && data[sep] != '\n')
            ++sep;
        // 模式段 [0, sep) + flags 段 (sep, len)；无 flags 时空串
        const String pattern{ data, sep };
        const String flags{ sep + 1 < len ? data + sep + 1 : "", sep + 1 < len ? len - sep - 1 : 0 };
        regs[r1] = Value{ rt.create<RegExp>(&rt, pattern, flags).get() };
        return DispatchRet::Continue;
    }

} // namespace cial::vm
