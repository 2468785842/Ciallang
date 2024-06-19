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

#include "Ast.hpp"

#include "ExprNode.hpp"
#include "StmtNode.hpp"
#include "DeclNode.hpp"

namespace Ciallang::Syntax {


    AstBuilder::~AstBuilder() {
        for(const auto& val : _nodes)
            delete val;
        _nodes.clear();
    }

    ExprStmtNode* AstBuilder::makeExprStmtNode(const ExprNode* rhs) {
        return makeNode<ExprStmtNode>(rhs);
    }

    IfStmtNode* AstBuilder::makeIfStmtNode(const ExprNode* test, const BlockStmtNode* body) {
        return makeNode<IfStmtNode>(test, body);
    }

    WhileStmtNode* AstBuilder::makeWhileStmtNode(const ExprNode* test, const BlockStmtNode* body) {
        return makeNode<WhileStmtNode>(test, body);
    }

    ValueExprNode* AstBuilder::makeValueExprNode(Token&& op) {
        return makeNode<ValueExprNode>(op);
    }

    /**
     * 创建一个二目运算节点
     *
     * @param op 根节点
     * @param lhs 左叶子节点
     * @param rhs 右叶子节点
     * @return 二目运算符AstNode
     */
    BinaryExprNode* AstBuilder::makeBinaryExprNode(
        Token&& op, const ExprNode* lhs, const ExprNode* rhs
    ) {
        auto* node = makeNode<BinaryExprNode>(op, lhs, rhs);
        node->location.start(lhs->location.start());
        node->location.end(rhs->location.end());
        return node;
    }

    UnaryExprNode* AstBuilder::makeUnaryExprNode(Token&& op, const ExprNode* rhs) {
        return makeNode<UnaryExprNode>(op, rhs);
    }

    ProcCallExprNode* AstBuilder::makeProcCallExprNode(const ExprNode* lhs) {
        return makeNode<ProcCallExprNode>(lhs);
    }

    AssignExprNode* AstBuilder::makeAssignExprNode(const IdentifierExprNode* lhs, const ExprNode* rhs) {
        return makeNode<AssignExprNode>(lhs, rhs);
    }

    IdentifierExprNode* AstBuilder::makeSymbolExprNode(Token&& token) {
        return makeNode<IdentifierExprNode>(token);
    }

    FunctionDeclNode* AstBuilder::makeFunctionDeclNode(Token&& token) {
        return makeNode<FunctionDeclNode>(token);
    }

    StmtDeclNode* AstBuilder::makeStmtDeclNode(const StmtNode* stmtNode) {
        return makeNode<StmtDeclNode>(stmtNode);
    }

    BreakStmtNode* AstBuilder::makeBreakStmtNode() {
        return makeNode<BreakStmtNode>();
    }

    ContinueStmtNode* AstBuilder::makeContinueStmtNode() {
        return makeNode<ContinueStmtNode>();
    }

    ReturnStmtNode* AstBuilder::makeReturnStmtNode(const ExprNode* node) {
        return makeNode<ReturnStmtNode>(node);
    }

}
