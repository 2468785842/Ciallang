#include "Compilation.hpp"

#include <algorithm>

#include "common/Defer.hpp"
#include "gen/ControlFlowGraph.hpp"
#include "gen/IRGenerator.hpp"
#include "gen/SSA.hpp"
#include "parser/Parser.hpp"
#include "parser/ast/ExprNode.hpp"
#include "types/Class.hpp"

namespace cial::cli {
    Compilation compile(Common::Result &diagnostics, Context &context, Common::SourceFile &source,
                        const bool expression) {
        Compilation compilation;
        if(expression)
            compilation.result = inter::Register{ 0xFFFF }; // request a value, including postfix ++/--
        if(diagnostics.isFailed())
            return compilation;
        syntax::Parser parser{ source, context.pp() };
        const syntax::AstNode *node = expression
            ? static_cast<syntax::AstNode *>(parser.parseExpression(diagnostics, true))
            : parser.parse(diagnostics);
        if(expression) {
            if(parser.peek(diagnostics, syntax::TokenType::SemiColon))
                parser.consume(diagnostics);
            syntax::Token *remaining{};
            if(parser.current(diagnostics, remaining) && remaining->type() != syntax::TokenType::EndOfFile)
                diagnostics.error("unexpected token after expression", remaining->location);
        }
        if(!node && !diagnostics.isFailed())
            diagnostics.error("cannot parse input");
        if(diagnostics.isFailed())
            return compilation;
        inter::IRGenerator generator{ diagnostics, context.rt(), source };
        compilation.chunk = generator.parseAst(node, compilation.result);
        if(!compilation.chunk && !diagnostics.isFailed())
            diagnostics.error("cannot compile input");
        return compilation;
    }

    Value execute(vm::VMState &state, vm::Chunk chunk, const OptReg result) {
        if(chunk.getInstVec().empty())
            return {};
        auto rooted = std::move(chunk).toHeap(state.rt);
        const auto depth = state.context.stackTop();
        DEFER {
            while(state.context.stackTop() > depth)
                state.freeCallFrame();
        };
        state.allocCallFrame(rooted.get());
        state.makeClosure();
        state.curFrame()->setThisObj(Value{ static_cast<Object *>(state.context.global()) });
        try {
            state.run();
            return result ? state.reg(result->index()) : Value{};
        } catch(...) {
            state.clearException();
            throw;
        }
    }

    void dumpGraphs(Runtime &runtime, vm::Chunk &top, const bool ssa) {
        Vec<std::pair<std::string, vm::Chunk *>> pending{ { "top-level", &top } };
        Vec<vm::Chunk *> visited;
        while(!pending.empty()) {
            auto [name, chunk] = pending.back();
            pending.pop_back();
            if(std::ranges::find(visited, chunk) != visited.end())
                continue;
            visited.push_back(chunk);
            for(const auto &constant : chunk->getConstants()) {
                if(constant.type() != ConstantType::FuncMeta)
                    continue;
                const auto *meta = constant.value<FuncMeta *>();
                if(!meta || !meta->chunk)
                    continue;
                const auto *entry = runtime.atomTable.get(meta->name);
                const auto functionName = entry && entry->str && entry->str->length() ? entry->str->toStdStr() : "func";
                pending.emplace_back(functionName, meta->chunk);
            }
            inter::ControlFlowGraph cfg;
            cfg.buildCFG(*chunk);
            fmt::println("// ===== {} =====", name);
            if(ssa) {
                const auto form = inter::SsaBuilder::buildChunkSsa(*chunk, cfg.getBasicBlocks());
                fmt::println("{}", inter::SsaBuilder::dumpSsaText(*chunk, form));
            } else {
                fmt::println("{}", cfg.dumpDot(*chunk));
            }
        }
    }
} // namespace cial::cli
