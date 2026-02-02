/*
 * Copyright (c) 2024/5/6 下午8:16
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

#include "stdlib/Print.hpp"
#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace cial;

int main(const int argc, char **argv) {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };

    Common::Result r{};
    Common::SourceFile sourceFile;
    sourceFile.load(r, R"(
            function fib(n) {
                if(n < 2) return n;
                return fib(n - 2) + fib(n - 1);
            }
            return fib(25);
        )");

    assert(!r.isFailed());

    syntax::Parser parser{ sourceFile, vmState.context.pp() };

    syntax::AstNode *node = parser.parse(r);
    assert(!r.isFailed());

    inter::IRGenerator codeGen{ r, rt, sourceFile };

    OptReg ignoreReg{};
    Opt<vm::Chunk> chunk = codeGen.parseAst(node, ignoreReg);

    assert(!r.isFailed());
    assert(chunk);
    assert(chunk->getRegCount() != 0);
    assert(!chunk->code().empty());

    auto *evalChunk = vmState.rt.create<vm::Chunk>(std::move(*chunk)).get();

    u16 retReg{ 0 };
    vm::Chunk tmpChunk{};
    tmpChunk.setRegCount(1); // accept ret val
    tmpChunk.toBytecode();

    vmState.allocCallFrame(&tmpChunk);
    vmState.makeClosure();
    vmState.allocCallFrame(evalChunk, retReg);
    vmState.makeClosure();

    u32 stackTop = vmState.context.stackTop();
    vmState.run0();
    Value ret = vmState.reg(retReg);

    fmt::println("{}", ret.asInteger().value());

    if(stackTop - 1 != vmState.context.stackTop()) {
        vmState.freeCallFrame();
    }
    vmState.freeCallFrame();
    return 0;
}
