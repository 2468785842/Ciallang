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

#include <cassert>
#include <sstream>
#include <set>
#include <fmt/printf.h>

#include "../types/TjsString.hpp"

namespace Ciallang::Syntax {
    AstFormatter::AstFormatter(AstNode* root, FILE* file)
        : _file(file), _root(root) {
    }

    void AstFormatter::format(const std::string& title) {
        fmt::print(_file, "digraph G {{\n");
        fmt::print(_file, "graph [ fontsize=22 ];\n");
        fmt::print(_file, "labelloc=\"t\";\n");
        fmt::print(_file, "label=\"{}\";\n", title);
        _root->accept(this);
        fmt::print(_file, "}}\n");
    }

    void AstFormatter::visit(const ValueExprNode* node) const {
        assert(node->token != nullptr);

        std::string shape = "record";
        std::string details;
        auto nodeVertexName = getVertexName(node);
        std::string style = ", fillcolor=goldenrod1, style=\"filled\"";
        const auto strPtr = node->token->value();
        std::stringstream ss{ "" };

        assert(strPtr != nullptr);

        switch(strPtr->type()) {
            case TjsValueType::Integer:
                ss << strPtr->asInteger();
                break;
            case TjsValueType::Real:
                ss << strPtr->asReal();
                break;
            default:
                assert(false);
        }

        details = fmt::format(
                              "|{{ {} }}",
                              GraphvizFormatter::escapeChars(ss.str()));

        fmt::print(
                   _file,
                   "\t{}[shape={},label=\"<f1> {}{}\"{}];\n",
                   nodeVertexName,
                   shape,
                   node->name(),
                   details,
                   style);
    }

    void AstFormatter::visit(const SymbolExprNode* node) const {
        assert(node->token != nullptr);

        std::string shape = "record";
        auto nodeVertexName = getVertexName(node);
        std::string style = ", fillcolor=goldenrod1, style=\"filled\"";

        fmt::print(
                   _file,
                   "\t{}[shape={},label=\"<f1> {}|{{ {} }}\"{}];\n",
                   nodeVertexName,
                   shape,
                   node->name(),
                   node->token->value()->asString()->c_str(),
                   style);
    }

    void AstFormatter::visit(const BinaryExprNode* node) const {
        assert(node->token != nullptr);
        assert(node->lhs != nullptr);
        assert(node->rhs != nullptr);

        auto nodeVertexName = getVertexName(node);
        std::string style = ", fillcolor=goldenrod1, style=\"filled\"";
        std::string value = node->token->name();

        auto details = fmt::format(
                                   "|{{ operator: '{}' }}",
                                   GraphvizFormatter::escapeChars(value));

        fmt::print(
                   _file,
                   "\t{}[shape=record,label=\"<f0> lhs|<f1> {}{}|<f2> rhs\"{}];\n",
                   nodeVertexName,
                   node->name(),
                   details,
                   style);

        node->lhs->accept(this);
        fmt::print(
                   _file,
                   "\t{}:f0 -> {}:f1;\n",
                   nodeVertexName,
                   getVertexName(node->lhs));
        node->rhs->accept(this);
        fmt::print(
                   _file,
                   "\t{}:f2 -> {}:f1;\n",
                   nodeVertexName,
                   getVertexName(node->rhs));
    }

    void AstFormatter::visit(const UnaryExprNode* node) const {
        assert(node->token != nullptr);
        assert(node->rhs != nullptr);

        auto nodeVertexName = getVertexName(node);
        std::string style = ", fillcolor=goldenrod1, style=\"filled\"";
        std::string value = node->token->name();

        auto details = fmt::format(
                                   "|{{ operator: '{}' }}",
                                   GraphvizFormatter::escapeChars(value));

        fmt::print(
                   _file,
                   "\t{}[shape=record,label=\"<f0> {}{}|<f2> rhs\"{}];\n",
                   nodeVertexName,
                   node->name(),
                   details,
                   style);

        node->rhs->accept(this);
        fmt::print(
                   _file,
                   "\t{}:f2 -> {}:f1;\n",
                   nodeVertexName,
                   getVertexName(node->rhs));
    }

    void AstFormatter::visit(const AssignExprNode* node) const {
        assert(node->lhs != nullptr);
        assert(node->rhs != nullptr);

        auto nodeVertexName = getVertexName(node);
        std::string style = ", fillcolor=goldenrod1, style=\"filled\"";

        fmt::print(
                   _file,
                   "\t{}[shape=record,label=\"<f0> lhs|<f1> =|<f2> rhs\"{}];\n",
                   nodeVertexName,
                   style);

        node->lhs->accept(this);
        fmt::print(
                   _file,
                   "\t{}:f0 -> {}:f1;\n",
                   nodeVertexName,
                   getVertexName(node->lhs));
        node->rhs->accept(this);
        fmt::print(
                   _file,
                   "\t{}:f2 -> {}:f1;\n",
                   nodeVertexName,
                   getVertexName(node->rhs));
    }

    void AstFormatter::visit(const BlockStmtNode* node) const {
        auto index = 0;
        set<std::string> edges{};

        auto nodeVertexName = getVertexName(node);

        for(const auto* child : node->children) {
            if(child == nullptr) break;
            child->accept(this);
            edges.insert(getVertexName(child));
            index++;
        }

        if(!edges.empty()) {
            index = 0;
            for(const auto& edge : edges)
                fmt::print(
                           _file,
                           "\t{}:f1 -> {}:f1 [label=\"[{:02}]\"];\n",
                           nodeVertexName,
                           edge,
                           index++);
            fmt::print(_file, "\n");
        }
    }

    void AstFormatter::visit(const ExprStmtNode* node) const {
        auto index = 0;
        set<std::string> edges{};

        auto nodeVertexName = getVertexName(node);

        for(const auto child : node->expressions) {
            if(child == nullptr) break;
            child->accept(this);
            edges.insert(getVertexName(child));
            index++;
        }

        if(!edges.empty()) {
            index = 0;
            for(const auto& edge : edges)
                fmt::print(
                           _file,
                           "\t{}:f1 -> {}:f1 [label=\"[{:02}]\"];\n",
                           nodeVertexName,
                           edge,
                           index++);
            fmt::print(_file, "\n");
        }
    }

    void AstFormatter::visit(const IfStmtNode* node) const {
        assert(node->test != nullptr);
        assert(node->body != nullptr);

        auto nodeVertexName = getVertexName(node);
        std::string style = ", fillcolor=goldenrod1, style=\"filled\"";
        std::string value = node->test->name();

        fmt::print(
                   _file,
                   "\t{}[shape=record,label=\"<f0> test|<f1> {}|<f2> body|<f3> else\"{}];\n",
                   nodeVertexName,
                   node->name(),
                   style);

        node->test->accept(this);
        fmt::print(
                   _file,
                   "\t{}:f0 -> {}:f1;\n",
                   nodeVertexName,
                   getVertexName(node->test));

        node->body->accept(this);
        fmt::print(
                   _file,
                   "\t{}:f2 -> {}:f1;\n",
                   nodeVertexName,
                   getVertexName(node->body));

        if(node->elseBody) {
            node->elseBody->accept(this);
            fmt::print(
                       _file,
                       "\t{}:f3 -> {}:f1;\n",
                       nodeVertexName,
                       getVertexName(node->elseBody));
        }
    }

    std::string AstFormatter::getVertexName(const AstNode* node) {
        return fmt::format("{}{}", node->name(), node->id);
    }
}
