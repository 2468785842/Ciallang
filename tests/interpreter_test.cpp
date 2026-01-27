/*
 * Copyright (c) 2024/5/30 上午9:14
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

#include <catch.hpp>

#include "stdlib/Print.hpp"
#include "test_config.h"

#include "vm/VM.hpp"
#include "vm/VMState.hpp"

#include "types/Function.hpp"

using namespace cial;

TEST_CASE("解释器 - Hello World") {
    Runtime rt{};
    Context context{ rt };
    context.registryGlobalFunc("println"_str, &stdlib::S_PrintlnFunction);
    vm::VMState vmState{ context };
    const VM vm{ &vmState };
    vm.evalExpr(R"(println("Hello World!"))"_str);
}

TEST_CASE("解释器 - 执行测试") {
    Runtime rt{};
    Context context{ rt };
    context.registryGlobalFunc("println"_str, &stdlib::S_PrintlnFunction);
    vm::VMState vmState{ context };
    const VM vm{ &vmState };
    vm.eval(String(TEST_FILES_PATH R"(/startup.tjs)"), true);
}

int fib(const int n) {
    if(n < 2)
        return n;
    return fib(n - 2) + fib(n - 1);
}

TEST_CASE("解释器 - 脚本执行性能") {
    const int correct = fib(15);
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
            return fib(15);
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
    assert(!chunk->getInstVec().empty());

    auto *evalChunk = vmState.rt.create<vm::Chunk>(std::move(*chunk)).get();

    u16 retReg{ 0 };
    vm::Chunk tmpChunk{};
    tmpChunk.setRegCount(1); // accept ret val

    vmState.allocCallFrame(&tmpChunk);
    vmState.makeClosure();
    vmState.allocCallFrame(evalChunk, retReg);
    vmState.makeClosure();

    u32 stackTop = vmState.context.stackTop();
    BENCHMARK("fib 15") { vmState.run0(); };
    Value ret = vmState.reg(retReg);

    REQUIRE(ret.asInteger().value() == correct);

    if(stackTop - 1 != vmState.context.stackTop()) {
        vmState.freeCallFrame();
    }
    vmState.freeCallFrame();
}