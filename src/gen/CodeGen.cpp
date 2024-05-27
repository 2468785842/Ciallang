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

#include "CodeGen.hpp"
#include "Token.hpp"

namespace Ciallang::Inter {

    static const inline std::unordered_map<AstNodeType, NodeGenCallable> S_NodeGens{
            {AstNodeType::global_list,     std::bind_front(&CodeGen::global)},
            {AstNodeType::statement,       std::bind_front(&CodeGen::statement)},
            {AstNodeType::binary_operator, std::bind_front(&CodeGen::binaryOperator)}
    };

    std::shared_ptr<Prototype*> CodeGen::load(AstNode *programNode) {
        if (programNode->children.empty()) {
            return nullptr;
        }

        auto pPrototype = std::make_shared<Prototype *>(new Prototype{});

        if (!parse(programNode->children[0], pPrototype)) {
            return nullptr;
        }

        return pPrototype;
    }

    bool CodeGen::parse(AstNode *node, std::shared_ptr<Prototype *> prototype) {
        auto it = S_NodeGens.find(node->type);
        if (it != S_NodeGens.end()) {
            return it->second(this, node, prototype);
        }
        return false;
    }

    bool CodeGen::global(AstNode *node, std::shared_ptr<Prototype *> prototype) {
        if (node->children.empty()) {
            return false;
        }
        return parse(node, prototype);
    }

    bool CodeGen::statement(AstNode *node, std::shared_ptr<Prototype *> prototype) {
        if (!node->rhs) {
            return false;
        }
        return parse(node->rhs, prototype);
    }

    bool CodeGen::binaryOperator(AstNode *node, std::shared_ptr<Prototype *> prototype) {
        switch (node->token.type) {
            case TokenType::Plus: {

                //TODO emit opcode;
//                prototype.
                return true;
            }
            case TokenType::Asterisk: {
                return true;
            }
        }
        return false;
    }

}