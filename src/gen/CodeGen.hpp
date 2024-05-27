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

#include <functional>
#include "Ast.hpp"
#include "Prototype.hpp"

namespace Ciallang::Inter {
    using namespace Syntax;
    using namespace VM;

    class CodeGen;

    using NodeGenCallable = std::function<
            bool(CodeGen *, AstNode *, std::shared_ptr<Prototype *>)
    >;

    class CodeGen {
    public:
        CodeGen() {}

        std::shared_ptr<Prototype *> load(AstNode *programNode);

        bool parse(AstNode *node, std::shared_ptr<Prototype *> prototype);

        bool global(AstNode *node, std::shared_ptr<Prototype *> prototype);

        bool statement(AstNode *node, std::shared_ptr<Prototype *> prototype);

        bool binaryOperator(AstNode *node, std::shared_ptr<Prototype *> prototype);
    };

}
