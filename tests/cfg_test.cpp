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

#include "gen/ControlFlowGraph.hpp"
#include "gen/Optimizer.hpp"
#include "parser/ast/DeclNode.hpp"
#include "parser/ast/StmtNode.hpp"

#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace cial;

TEST_CASE("cfg - test") {

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
    )");

    assert(!r.isFailed());

    syntax::Parser parser{ sourceFile, vmState.context.pp() };

    auto *node = dynamic_cast<syntax::BlockStmtNode *>(parser.parse(r));
    assert(!r.isFailed());

    inter::IRGenerator codeGen{ r, rt, sourceFile };

    OptReg ignoreReg{};
    Opt<inter::TacChunk> tacChunk =
        codeGen.parseAst(dynamic_cast<syntax::FunctionDeclNode *>(node->childrens[0])->body, ignoreReg);

    inter::OptimizerManager optimizerManager(*tacChunk);
    optimizerManager.addOptimizer(std::make_unique<inter::RegisterAllocator>());
    optimizerManager.applyOptimizations();

    fmt::print("{}", tacChunk->dumpInstructions());

    assert(!r.isFailed());
    inter::ControlFlowGraph cfg{};
    cfg.buildCFG(*tacChunk);
    inter::CFGVisualizer::exportToDot("debug_cfg.dot", cfg.getBasicBlocks());
}