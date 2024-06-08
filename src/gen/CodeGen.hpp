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
            Common::SourceFile& sourceFile
        ) : _sourceFile(sourceFile) {
        }

        std::unique_ptr<VM::VMChunk> parseAst(Common::Result& r,
                                              const Syntax::AstNode* node);

        void error(
            Common::Result& r,
            const std::string& message,
            const Common::SourceLocation& location) const {
            _sourceFile.error(r, message, location);
        }

        ~CodeGen() noexcept override {
            for(auto local : _locals) {
                delete local;
            }
            for(auto chunk : _chunks) {
                delete chunk;
            }
        }

    private:
        std::vector<VM::VMChunk*> _chunks{};

        Common::SourceFile& _sourceFile;
        Common::Result* _r{ nullptr };

        struct Local {
            const Syntax::Token* token;
            const size_t depth;
            bool init;
        };

        std::vector<Local*> _locals{};

        struct Loop {
            const int16_t addr;
            const size_t depth;
            std::vector<int16_t> controls{}; // break or continue for this loop
        };

        std::vector<Loop> _loops{};

        // 1 is global scope
        size_t _scopeDepth{ 0 };

        void visit(const Syntax::ValueExprNode*) override;

        void visit(const Syntax::SymbolExprNode*) override;

        void visit(const Syntax::BinaryExprNode*) override;

        void visit(const Syntax::UnaryExprNode*) override;

        void visit(const Syntax::AssignExprNode*) override;

        void visit(const Syntax::BlockStmtNode*) override;

        void visit(const Syntax::ExprStmtNode*) override;

        void visit(const Syntax::IfStmtNode*) override;

        void visit(const Syntax::VarDeclNode*) override;

        void visit(const Syntax::FunctionDeclNode*) override;

        void visit(const Syntax::StmtDeclNode*) override;

        void visit(const Syntax::WhileStmtNode*) override;

        void visit(const Syntax::BreakStmtNode*) override;

        void visit(const Syntax::ContinueStmtNode*) override;

        void beginScope();

        void endScope(const Syntax::BlockStmtNode* node);

        std::pair<size_t, Local*> resolveLocal(const Syntax::Token* token) const;
    };
}
