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

#include "../ast/Ast.hpp"
#include "../vm/Interpreter.hpp"
#include "../common/Result.hpp"
#include "../common/SourceFile.hpp"

namespace Ciallang::Inter {
    class CodeGen;

    class CodeGen : Syntax::AstNode::Visitor {
    public:
        friend class Syntax::AstNode;

        explicit CodeGen(
            Common::SourceFile& sourceFile, VM::VMChunk* vmChunk
        ) : _vmChunk(vmChunk), _sourceFile(sourceFile) {
        }

        void loadAst(Common::Result& r, const Syntax::AstNode* node);

        void error(
            Common::Result& r,
            const std::string& message,
            const Common::SourceLocation& location) const {
            _sourceFile.error(r, message, location);
        }

        ~CodeGen() noexcept override {
            for(auto local : locals) {
                delete local;
            }
        }

    private:
        VM::VMChunk* _vmChunk;
        Common::SourceFile& _sourceFile;
        Common::Result* _r{ nullptr };

        struct Local {
            const Syntax::Token* token;
            size_t depth;
            bool init;
        };

        std::vector<Local*> locals{};

        // 1 is global scope
        size_t scopeDepth{ 0 };

        void visit(const Syntax::ValueExprNode*) override;

        void visit(const Syntax::SymbolExprNode*) override;

        void visit(const Syntax::BinaryExprNode*) override;

        void visit(const Syntax::UnaryExprNode*) override;

        void visit(const Syntax::AssignExprNode*) override;

        void visit(const Syntax::BlockStmtNode*) override;

        void visit(const Syntax::ExprStmtNode*) override;

        void visit(const Syntax::IfStmtNode*) override;

        void visit(const Syntax::VarDeclNode*) override;

        void visit(const Syntax::StmtDeclNode*) override;

        void beginScope();
        void endScope(const Syntax::BlockStmtNode* node);

        std::pair<size_t, Local*> resolveLocal(const Syntax::Token* token) const;
    };
}
