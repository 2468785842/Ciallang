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

namespace Ciallang::Syntax {
    BlockStmtNode* AstBuilder::popScope() {
        if(_scopeStack.empty())
            return nullptr;
        const auto top = _scopeStack.top();
        _scopeStack.pop();
        return top;
    }

    BlockStmtNode* AstBuilder::endScope() {
        return popScope();
    }

    BlockStmtNode* AstBuilder::beginScope() {
        if(_scopeStack.empty()) {
            return globalNode();
        }
        return basicBlockNode();
    }

    BlockStmtNode* AstBuilder::globalNode() {
        auto* scope = makeNode<BlockStmtNode>("global_scope");
        _scopeStack.push(scope);
        return scope;
    }

    BlockStmtNode* AstBuilder::basicBlockNode() {
        auto* scope = makeNode<BlockStmtNode>("basic_scope");
        _scopeStack.push(scope);
        return scope;
    }

    void AstBuilder::pushScope(BlockStmtNode* node) {
        _scopeStack.push(node);
    }

    AstBuilder::~AstBuilder() {
        reset();
        for(const auto& val : _nodes)
            delete val;
        _nodes.clear();
    }

    void AstBuilder::reset() {
        while(!_scopeStack.empty())
            _scopeStack.pop();
    }

    ExprStmtNode* AstBuilder::makeExprStmtNode(ExprNode* rhs) {
        return makeNode<ExprStmtNode>(rhs);
    }

    IfStmtNode* AstBuilder::makeIfStmtNode(const ExprNode* test, const StmtNode* body) {
        return makeNode<IfStmtNode>(test, body);
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

    AssignExprNode* AstBuilder::makeAssignExprNode(const SymbolExprNode* lhs, const ExprNode* rhs) {
        return makeNode<AssignExprNode>(lhs, rhs);
    }

    SymbolExprNode* AstBuilder::makeSymbolExprNode(Token&& token) {
        return makeNode<SymbolExprNode>(token);
    }

    VarDeclNode* AstBuilder::makeVarDeclNode(const ExprStmtNode* rhs) {
        return makeNode<VarDeclNode>(rhs);
    }

    template <typename R, typename... Args>
    R* AstBuilder::makeNode(Args&&... args) {
        static_assert(
            std::is_base_of_v<AstNode, R>,
            "Error: R must be a derived class of AstNode."
        );

        R* node = new R{ std::forward<Args>(args)... };
        _nodes.push_front(node);
        return node;
    }
}
