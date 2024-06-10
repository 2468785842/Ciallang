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

#include "AstFormatter.hpp"

#include "../types/TjsString.hpp"

#include "../ast/DeclNode.hpp"
#include "../ast/ExprNode.hpp"
#include "../ast/StmtNode.hpp"

namespace Ciallang::Syntax {
    AstFormatter::AstFormatter(AstNode* root, FILE* file)
        : _file(file), _root(root) {
    }

    void AstFormatter::format(const std::string& title) {
        fmt::print(_file, "digraph G {{\n");
        fmt::print(_file, "graph [fontsize=22, fontname=\"Arial\"];\n");
        fmt::print(_file, "node [shape=record, style=filled, fontname=\"Arial\"];\n");
        fmt::print(_file, "edge [fontname=\"Arial\"];\n");
        fmt::print(_file, "labelloc=\"t\";\n");
        fmt::print(_file, "label=\"{}\";\n", title);
        _root->accept(this);
        fmt::print(_file, "}}\n");
    }

    void AstFormatter::printNode(const std::string& vertexName,
                                 const std::string& label,
                                 const std::string& details,
                                 const std::string& style) {
        fmt::print(_file, "\t{}[label=\"{}{}\"{}];\n", vertexName, label, details, style);
    }

    void AstFormatter::printEdge(const std::string& from,
                                 const std::string& fromPort,
                                 const std::string& to,
                                 const std::string& toPort,
                                 const std::string& style = "") {
        fmt::print(_file, "\t{}:{} -> {}:{}{};\n", from, fromPort, to, toPort, style);
    }

    std::string AstFormatter::formatStyle(const std::string& fillColor,
                                          const std::string& fontColor = "black",
                                          const std::string& shape = "record") {
        return fmt::format(", fillcolor={}, fontcolor={}, shape={}", fillColor, fontColor, shape);
    }

    void AstFormatter::visit(const ValueExprNode* node) {
        assert(node->token != nullptr && node->token->value() != nullptr);

        std::stringstream ss;
        switch(node->token->value()->type()) {
            case TjsValueType::Integer:
                ss << node->token->value()->asInteger();
                break;
            case TjsValueType::Real:
                ss << node->token->value()->asReal();
                break;
            default:
                assert(false);
        }

        std::string details = fmt::format(": {}",
            GraphvizFormatter::escapeChars(ss.str())
        );
        std::string style = formatStyle(
            "lightblue", "black", "ellipse"
        );
        printNode(getVertexName(node), fmt::format("{}", node->name()), details, style);
    }

    void AstFormatter::visit(const SymbolExprNode* node) {
        assert(node->token != nullptr);

        std::string details = fmt::format(": {}", *node->token->value());
        std::string style = formatStyle(""
            "lightgreen", "black", "box"
        );
        printNode(getVertexName(node), fmt::format("{}", node->name()), details, style);
    }

    void AstFormatter::visitBinaryUnaryNode(const std::string& vertexName, const std::string& nodeName,
                                            const std::string& details, const std::string& lhsPort, const AstNode* lhs,
                                            const std::string& rhsPort, const AstNode* rhs) {
        std::string style = formatStyle(
            "goldenrod1", "black", "record"
        );
        printNode(vertexName, nodeName, details, style);
        if(lhs) {
            lhs->accept(this);
            printEdge(vertexName, lhsPort, getVertexName(lhs), "f1");
        }
        if(rhs) {
            rhs->accept(this);
            printEdge(vertexName, rhsPort, getVertexName(rhs), "f1");
        }
    }

    void AstFormatter::visit(const BinaryExprNode* node) {
        assert(node->token != nullptr && node->lhs != nullptr && node->rhs != nullptr);

        std::string details = fmt::format("|{{ operator: '{}' }}", GraphvizFormatter::escapeChars(node->token->name()));
        visitBinaryUnaryNode(getVertexName(node), fmt::format("<f0> lhs|<f1> {}{}|<f2> rhs", node->name(), details), "",
            "f0", node->lhs, "f2", node->rhs);
    }

    void AstFormatter::visit(const UnaryExprNode* node) {
        assert(node->token != nullptr && node->rhs != nullptr);

        std::string details = fmt::format("|{{ operator: '{}' }}", GraphvizFormatter::escapeChars(node->token->name()));
        visitBinaryUnaryNode(getVertexName(node),
            fmt::format("<f0> {}{}|<f2> rhs", node->name(), details),
            "", "",
            nullptr, "f2", node->rhs
        );
    }

    void AstFormatter::visit(const ProcCallExprNode* node) {
        auto nodeVertexName = getVertexName(node);
        std::set<std::string> edges;

        std::string style = formatStyle("pink", "black", "record");
        printNode(nodeVertexName,
            fmt::format("<f0> lhs | <f1> {} | <f2> rhs", node->name()),
            "", style
        );
        node->memberAccess->accept(this);
        printEdge(nodeVertexName,
            "f0", getVertexName(node->memberAccess), "f1"
        );
        for(auto argument : node->arguments) {
            argument->accept(this);
            edges.insert(getVertexName(argument));
        }
        int index = 0;
        for(const auto& edge : edges)
            fmt::print(_file,
                "\t{}:f2 -> {}:f1 [label=\"[{:02}]\"];\n",
                nodeVertexName, edge, index++
            );
        fmt::print(_file, "\n");
    }


    void AstFormatter::visit(const AssignExprNode* node) {
        assert(node->lhs != nullptr && node->rhs != nullptr);

        visitBinaryUnaryNode(
            getVertexName(node),
            "<f0> lhs|<f1> {}|<f2> rhs", std::string{ node->name() },
            "f0", node->lhs, "f2", node->rhs
        );
    }

    void AstFormatter::visit(const BlockStmtNode* node) {
        auto nodeVertexName = getVertexName(node);
        std::set<std::string> edges;

        for(const auto* child : node->childrens) {
            if(child == nullptr) break;
            child->accept(this);
            edges.insert(getVertexName(child));
        }

        int index = 0;
        for(const auto& edge : edges)
            fmt::print(_file,
                "\t{}:f1 -> {}:f1 [label=\"[{:02}]\"];\n",
                nodeVertexName, edge, index++
            );
        fmt::print(_file, "\n");
    }

    void AstFormatter::visit(const ExprStmtNode* node) {
        auto nodeVertexName = getVertexName(node);
        std::string style = formatStyle("pink", "black", "record");
        printNode(nodeVertexName,
            fmt::format("<f0> {}", node->name()),
            "", style
        );
        node->expression->accept(this);
        printEdge(nodeVertexName,
            "f0", getVertexName(node->expression), "f1"
        );
    }

    void AstFormatter::visit(const IfStmtNode* node) {
        assert(node->test != nullptr && node->body != nullptr);

        auto nodeVertexName = getVertexName(node);
        std::string details = "|<f0> test|<f2> body|<f3> else";
        std::string style = formatStyle(""
            "lightyellow", "black", "record"
        );
        printNode(nodeVertexName, fmt::format("<f1> {}", node->name()), details, style);

        node->test->accept(this);
        printEdge(nodeVertexName,
            "f0", getVertexName(node->test), "f1"
        );
        node->body->accept(this);
        printEdge(nodeVertexName,
            "f2", getVertexName(node->body), "f1"
        );

        if(node->elseBody) {
            node->elseBody->accept(this);
            printEdge(nodeVertexName, ""
                "f3", getVertexName(node->elseBody), "f1"
            );
        }
    }

    void AstFormatter::visit(const VarDeclNode* node) {
        auto nodeVertexName = getVertexName(node);
        std::string details = fmt::format("|<f0> lhs |<f1> {} |<f2> rhs", node->name());
        std::string style = formatStyle(""
            "lightcyan", "black", "record"
        );
        printNode(nodeVertexName, "", details, style);

        fmt::print(_file, "\t{}:f0 -> {}:f1",
            nodeVertexName,
            *node->token->value()
        );

        if(node->rhs) {
            node->rhs->accept(this);
            printEdge(nodeVertexName,
                "f2", getVertexName(node->rhs), "f1"
            );
        }
    }

    void AstFormatter::visit(const FunctionDeclNode* node) {
        auto nodeVertexName = getVertexName(node);
        std::string details = fmt::format("<f0> lhs |<f1> {} |<f2> rhs", node->name());
        std::string style = formatStyle(""
            "lightcyan", "black", "record"
        );
        printNode(nodeVertexName, "", details, style);

        fmt::print(_file, "\t{}:f0 -> {}:f1",
            nodeVertexName,
            *node->token->value()
        );

        // XXX: print parameters
        node->body->accept(this);
        printEdge(nodeVertexName,
            "f2", getVertexName(node->body), "f1"
        );
    }


    void AstFormatter::visit(const StmtDeclNode* node) {
        auto nodeVertexName = getVertexName(node);
        std::string style = formatStyle(
            "lightcoral", "black", "record"
        );
        printNode(nodeVertexName,
            fmt::format("<f0> {}", node->name()),
            "", style
        );

        node->statement->accept(this);
        printEdge(nodeVertexName, "f0",
            getVertexName(node->statement),
            "f1"
        );
    }

    void AstFormatter::visit(const WhileStmtNode* node) {
        assert(node->test != nullptr);
        assert(node->body != nullptr);

        auto nodeVertexName = getVertexName(node);
        std::string details = "|<f0> test|<f2> body";
        std::string style = formatStyle(
            "lightgrey", "black", "record");
        printNode(nodeVertexName, fmt::format("<f1> {}", node->name()), details, style);

        node->test->accept(this);
        printEdge(nodeVertexName,
            "f0", getVertexName(node->test), "f1"
        );

        node->body->accept(this);
        printEdge(nodeVertexName,
            "f2", getVertexName(node->body), "f1"
        );
    }

    void AstFormatter::visit(const BreakStmtNode* node) {
        std::string details = fmt::format("{}", node->name());
        std::string style = formatStyle(""
            "lightcyan", "black", "record"
        );
        printNode(getVertexName(node), "", details, style);
    }

    void AstFormatter::visit(const ContinueStmtNode* node) {
        std::string details = fmt::format("{}", node->name());
        std::string style = formatStyle(""
            "lightcyan", "black", "record"
        );
        printNode(getVertexName(node), "", details, style);
    }


    std::string AstFormatter::getVertexName(const AstNode* node) {
        return fmt::format("{}{}", node->name(), node->id);
    }
}
