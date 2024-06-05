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

#pragma once

#include "Ast.hpp"

namespace Ciallang::Syntax {
    struct GraphvizFormatter {
        static std::string escapeChars(const std::string& value) noexcept {
            std::string buffer;
            for(const auto& c : value) {
                if(c == '\"') {
                    buffer += "\\\"";
                } else if(c == '{') {
                    buffer += "\\{";
                } else if(c == '}') {
                    buffer += "\\}";
                } else if(c == '.') {
                    buffer += "\\.";
                } else if(c == '|') {
                    buffer += "\\|";
                } else if(c == '<') {
                    buffer += "\\<";
                } else if(c == '>') {
                    buffer += "\\>";
                } else if(c == '=') {
                    buffer += "\\=";
                } else if(c == '\\') {
                    buffer += "\\\\";
                } else {
                    buffer += c;
                }
            }
            return buffer;
        }
    };

    class AstFormatter : AstNode::Visitor {
        FILE* _file = nullptr;
        AstNode* _root = nullptr;

    public:
        AstFormatter(AstNode* root, FILE* file);

        void format(const std::string& title);

        void visit(const ValueExprNode*) override;

        void visit(const SymbolExprNode*) override;

        void visit(const BinaryExprNode*) override;

        void visit(const UnaryExprNode*) override;

        void visit(const AssignExprNode*) override;

        void visit(const BlockStmtNode*) override;

        void visit(const ExprStmtNode*) override;

        void visit(const IfStmtNode*) override;

        void visit(const VarDeclNode*) override;

        void visit(const StmtDeclNode*) override;

    private:
        static std::string getVertexName(const AstNode* node);
    };
}
