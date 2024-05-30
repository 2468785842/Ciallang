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

#include "../common/SourceFile.hpp"
#include "../ast/Ast.hpp"
#include "../common/Result.hpp"

namespace Ciallang::Inter {
    struct CompilerOptions {
        bool verbose = false;
        size_t heapSize = 0;
        bool debugger = false;
        size_t stackSize = 0;
        size_t ffiHeapSize = 4096;
        bool outputAstGraphs = true;
    };

    class Compiler final {
        bool _run = false;
        Common::Result _result;
        Syntax::AstBuilder *_astBuilder;
        CompilerOptions _options{};
        Common::SourceFile _sourceFile;
        // ByteCodeEmitter* _emitter = nullptr;

    public:
        Compiler(
                const CompilerOptions &options,
                const std::filesystem::path &sourceFile) :
                _astBuilder(new Syntax::AstBuilder{}),
                _options(options),
                _sourceFile(sourceFile) {
        }

        ~Compiler() {
            delete _astBuilder;
        }

        void error(
                const std::string &message,
                const Common::SourceLocation &location);

        bool compile();

        void initializeCoreTypes() const;

        void finalize();

        void enableRun();

        Common::Result &result() { return _result; }

        void disassemble(FILE *file);

        [[nodiscard]] Syntax::AstBuilder &astBuilder() const;

        Common::SourceFile *sourceFile(int32_t id);

        Syntax::AstNode *parse(Common::SourceFile &sourceFile);

        void writeCodeDomGraph(const std::filesystem::path &path) const;
    };
}
