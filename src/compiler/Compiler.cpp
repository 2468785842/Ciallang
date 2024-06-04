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

#include "Compiler.hpp"

#include "../parser/Parser.hpp"

namespace Ciallang::Inter {
    void Compiler::error(
            const std::string &message,
            const Common::SourceLocation &location) {
        _result.error(message, location);
    }

    bool Compiler::compile() {
        const auto globalNode = parse(_sourceFile);
        if (globalNode == nullptr || _result.isFailed()) {
            return false;
        }
        return true;
    }

    /**
     * 初始化基本数据类型
     */
    void Compiler::initializeCoreTypes() const {

    }

    Syntax::AstNode *Compiler::parse(Common::SourceFile &sourceFile) {
        if (sourceFile.empty()) {
            if (!sourceFile.load(_result))
                return nullptr;
        }

        _astBuilder->reset();
        Syntax::Parser parser(sourceFile, *_astBuilder);

        const auto globalNode = parser.parse(_result);
        if (globalNode != nullptr && !_result.isFailed()) {
            if (_options.outputAstGraphs) {
                std::filesystem::path astFilePath{sourceFile.path().parent_path()};

                auto filename = sourceFile.path()
                        .filename()
                        .replace_extension("")
                        .string();
                filename += "-ast";
                astFilePath.append(filename);
                astFilePath.replace_extension(".dot");
                Syntax::Parser::writeAstGraph(astFilePath, globalNode);
            }
        }

        return globalNode;
    }

    Syntax::AstBuilder &Compiler::astBuilder() const {
        return *_astBuilder;
    }

}
