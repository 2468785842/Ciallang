// Copyright (c) 2024/5/21 下午8:33
//
// /\  _` \   __          /\_ \  /\_ \
// \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
//  \ \ \/_/_\/\ \  /'__`\  \ \ \  \ \ \   /'__`\  /' _ `\  /'_ `\
//   \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
//    \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
//     \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
//                                                            /\____
//                                                            \_/__/
//

#pragma once

#include "Label.hpp"
#include "Register.hpp"
#include "TacChunk.hpp"

#include "common/Result.hpp"
#include "common/SourceFile.hpp"
#include "parser/ast/AstNode.hpp"

#include "vm/Constant.hpp"

namespace cial::inter {

    class IRGenerator {

    public:
        explicit IRGenerator(Common::Result &r, Runtime &rt, Common::SourceFile &sourceFile) :
            _rt(rt), _sourceFile(sourceFile), _r(r) {}

        Opt<TacChunk> parseAst(const syntax::AstNode *node, OptReg &optReg);

        /**
         * allocate a temp register in this chunk
         * @return register
         */
        Register allocateRegister() { return Register{ _regNextIndex++ }; }

        /**
         * create a global scope
         * ensure we don't get variable from the global scope
         */
        void makeVirtualGlobalScope() { _scopeStartPC.emplace_back(0); }

#define DECLARE_AST_NODE_VISIT(name) void generate(const syntax::name *node, OptReg &optReg);
        CIAL_AST_NODE_ENUMS(DECLARE_AST_NODE_VISIT)
#undef DECLARE_AST_NODE_VISIT

    private:
        Box<TacChunk> _chunk = std::make_unique<TacChunk>();

        Runtime &_rt;
        Common::SourceFile &_sourceFile;
        Common::Result &_r;

        OptReg _empty{};

        using BreakContext = Vec<size_t>;
        Vec<BreakContext> _breakStack{};

        struct ContinueContext {
            Opt<Label> continueLabel;
            Vec<size_t> continues;
        };

        Vec<ContinueContext> _continueStack{};

        Vec<u32> _scopeStartPC{};

        i64 _regNextIndex{};

        [[nodiscard]] u32 getNextInstPos() const { return static_cast<u32>(this->_chunk->getInstVec().size()); }

        Label makeLabel() const { return Label{ getNextInstPos() }; }

        [[nodiscard]] bool isTopScope() const noexcept { return _scopeStartPC.size() == 1; }

        void beginScope() { _scopeStartPC.emplace_back(getNextInstPos()); }

        void endScope() {
            for(auto &localVar : _chunk->getLocalVars()) {
                if(localVar.startPC.address() > _scopeStartPC.back()) {
                    localVar.endPC = Label{ getNextInstPos() };
                }
            }
            _scopeStartPC.pop_back();
        }

        Register loadVoidReg();

        FuncMeta *generateFuncMeta(const syntax::Parameters &parameters, const syntax::BlockStmtNode *body) const;

        bool expectValue(const syntax::ExprNode *node);
        bool expectValue(const syntax::ExprNode *node, Register &ret);

        void error(const std::string &message, const Common::SourceLocation &location) const {
            _sourceFile.error(_r, message, location);
        }

        [[nodiscard]] Atom getAtomFromToken(const syntax::Token &token) const;

        void genTokenValueLoadInst(Register reg, const syntax::Token &token);
    };
} // namespace cial::inter
