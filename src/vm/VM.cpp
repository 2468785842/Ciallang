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
#include "VM.hpp"

#include "Runtime.hpp"
#include "VMState.hpp"
#include "ast/ExprNode.hpp"

#include "gen/IRGenerator.hpp"
#include "parser/Parser.hpp"

namespace Cial {
    VM::Handle::Handle(Runtime *rt, const Value &o) noexcept : rt(rt), value(o) {
        if(value.isObject()) {
            rt->addHandleVal(value.toObject());
        }
    }

    VM::Handle::~Handle() noexcept {
        if(value.isObject()) {
            rt->removeHandleVal(value.toObject());
        }
    }

    VM::VM(Bytecode::VMState *vmState) : _vmState(vmState) {}

    VM::Handle VM::getGlobal(const String &name) const { return { &_vmState->rt(), _vmState->global(name.getData()) }; }

    VM::Handle VM::evalExpr(const String &expr) const {
        Runtime &rt = _vmState->rt();
        if(expr.isEmpty())
            return { &rt, Value{} };

        Common::SourceFile sourceFile{};
        Common::Result r{};

        sourceFile.load(r, expr.getData());
        assert(!r.isFailed());

        Syntax::Parser parser{ rt, sourceFile };

        Syntax::ExprNode *node = parser.parseExpression(r);
        assert(!r.isFailed());

        Inter::IRGenerator codeGen{ sourceFile };

        codeGen.beginScope();

        OptReg retReg{};
        auto chunk = codeGen.parseAst(r, node, retReg);
        assert(!r.isFailed());
        assert(retReg);
        if(chunk->getRegCount() == 0)
            return { &rt, Value{} };

        auto &instVec = chunk->getInstVec();
        if(instVec.empty())
            return { &rt, Value{} };

        assert(instVec.back()->opcode != Bytecode::Op::OpCode::Ret);
        _vmState->allocCallFrame(chunk.get());
        _vmState->run();
        Value ret = _vmState->reg(*retReg);
        _vmState->freeCallFrame();

        return { &rt, ret };
    }
} // namespace Cial