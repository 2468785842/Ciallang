#include "VM.hpp"

namespace cial {
    VM::Handle<Value> VM::execute(Common::SourceFile &source, Common::Result &result, const bool expression,
                                  const Value &executionContext) const {
        Handle<Value> contextRoot{ &rt(), executionContext };
        const bool hasContext = !executionContext.isVoid() && !executionContext.isNull();
        if(hasContext && !executionContext.isObject())
            throw std::runtime_error("execution context must be an object");
        if(result.isFailed())
            return { &rt(), Value{} };
        // 声明文件不可执行（doc/declarations.md §4）：Scripts/Engine/未来模块
        // 加载器的统一闸口，仅写 Result，不编译
        if(source.isDeclaration()) {
            result.error(Common::DeclarationNotExecutableMessage, {}, {}, "decl.not-executable", source.sourceName());
            return { &rt(), Value{} };
        }
        syntax::Parser parser{ source, context().pp() };
        syntax::AstNode *node = expression ? parser.parseExpression(result, true) : parser.parse(result);
        if(result.isFailed() || !node)
            return { &rt(), Value{} };
        inter::IRGenerator generator{ result, rt(), source };
        if(hasContext)
            generator.useScriptContext();
        OptReg output = expression ? OptReg{ inter::Register{ 0xffff } } : OptReg{};
        auto compiled = generator.parseAst(node, output);
        if(result.isFailed() || !compiled)
            return { &rt(), Value{} };
        if(compiled->getInstVec().empty())
            return { &rt(), Value{} };
        if(expression) {
            if(!output) {
                result.error("expression produced no value", {}, {}, "CIAL-SDK-COMPILE", source.sourceName());
                return { &rt(), Value{} };
            }
            compiled->emit<inter::Ret>(*output);
        }
        auto chunk = std::move(*compiled).toHeap(rt());
        constexpr u16 retSlot = 0, exceptionSlot = 1;
        vm::Chunk sentinel;
        sentinel.setRegCount(2);
        sentinel.emit<inter::NOP>();
        sentinel.addThrowHandler({ 0, 1, exceptionSlot });
        // 哨兵帧永不派发，必须提前编码异常表。
        sentinel.toBytecode();
        auto &state = *_vmState;
        const auto entryDepth = context().stackTop();
        const auto sentinelBase = state.getRegPoolTop();
        DEFER {
            while(context().stackTop() > entryDepth)
                state.freeCallFrame();
        };
        if(entryDepth == 0)
            state.resetFault();
        state.allocCallFrame(&sentinel);
        state.curFrame()->exceptionBoundary = true;
        state.makeClosure();
        state.allocCallFrame(chunk.get(), retSlot);
        state.makeClosure();
        if(hasContext)
            state.curFrame()->setThisObj(executionContext);
        try {
            state.run();
        } catch(...) {
            if(entryDepth == 0)
                state.clearException();
            throw;
        }
        if(context().stackTop() == entryDepth ||
           (context().stackTop() == entryDepth + 1 && state.curFrame()->chunk == &sentinel &&
            state.curFrame()->pc == 1))
            throw vm::ScriptExceptionCarrier{ state.poolAt(sentinelBase + exceptionSlot) };
        // Ret 已弹目标帧；无 Ret 的语句块仍在栈顶，返回值槽在哨兵帧。
        if(context().stackTop() > entryDepth + 1)
            state.freeCallFrame();
        return { &rt(), state.reg(retSlot) };
    }
} // namespace cial
