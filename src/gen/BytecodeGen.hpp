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

#include "pch.h"

#include "ast/Ast.hpp"
#include "vm/Chunk.hpp"
#include "vm/Register.hpp"
#include "common/Result.hpp"
#include "common/SourceFile.hpp"
#include "vm/Label.hpp"

namespace Ciallang::Inter {
    struct LocalVariable {
        std::string identifier;
        Bytecode::Register reg;
        size_t scopeDepth;
        bool init;
    };

    class BytecodeGen {
    public:
        explicit BytecodeGen(
            Common::SourceFile& sourceFile
        ) : _sourceFile(sourceFile) {
        }

        std::unique_ptr<Bytecode::Chunk> parseAst(const Common::Result& r,
                                                  const Syntax::AstNode* node);

        void error(Common::Result& r,
                   const std::string& message,
                   const Common::SourceLocation& location) const {
            _sourceFile.error(r, message, location);
        }

        void addVariable(LocalVariable&& variable) {
            _variables.push_back(std::move(variable));
        }

        ~BytecodeGen() noexcept {
            delete _chunk;
        }

        std::optional<Bytecode::Register> generate(const Syntax::ValueExprNode*);

        std::optional<Bytecode::Register> generate(const Syntax::IdentifierExprNode*);

        std::optional<Bytecode::Register> generate(const Syntax::BinaryExprNode*);

        std::optional<Bytecode::Register> generate(const Syntax::UnaryExprNode*);

        std::optional<Bytecode::Register> generate(const Syntax::ProcCallExprNode*);

        std::optional<Bytecode::Register> generate(const Syntax::AssignExprNode*);

        std::optional<Bytecode::Register> generate(const Syntax::BlockStmtNode*);

        std::optional<Bytecode::Register> generate(const Syntax::ExprStmtNode*);

        std::optional<Bytecode::Register> generate(const Syntax::IfStmtNode*);

        std::optional<Bytecode::Register> generate(const Syntax::VarDeclNode*);

        std::optional<Bytecode::Register> generate(const Syntax::FunctionDeclNode*);

        std::optional<Bytecode::Register> generate(const Syntax::StmtDeclNode*);

        std::optional<Bytecode::Register> generate(const Syntax::WhileStmtNode*);

        std::optional<Bytecode::Register> generate(const Syntax::BreakStmtNode*);

        std::optional<Bytecode::Register> generate(const Syntax::ContinueStmtNode*);

        std::optional<Bytecode::Register> generate(const Syntax::ReturnStmtNode*);

    private:
        Bytecode::Chunk* _chunk{ nullptr };

        Common::SourceFile& _sourceFile;
        Common::Result _r{};

        size_t _scopeDepth{ 0 };

        std::vector<LocalVariable> _variables{};

        std::vector<Bytecode::Register> _freeRegisters{};
        std::uint32_t _nextIndex{ 0 };

        std::optional<Bytecode::Register> _empty{};

        Bytecode::Register allocateRegister() {
            if(!_freeRegisters.empty()) {
                const Bytecode::Register reg = _freeRegisters.back();
                _freeRegisters.pop_back();
                return reg;
            }

            return Bytecode::Register{ _nextIndex++ };
        }

        void freeRegister(const Bytecode::Register reg) {
            _freeRegisters.push_back(reg);
        }

        Bytecode::Label makeLabel() const {
            return Bytecode::Label{ _chunk->instructions().size() };
        }

        void beginScope() { _scopeDepth++; }

        void endScope() {
            auto new_end = std::ranges::remove_if(
                _variables, [&](const LocalVariable& variable) {
                    if(variable.scopeDepth == _scopeDepth) {
                        freeRegister(variable.reg);
                        return true;
                    }
                    return false;
                }).begin();

            _variables.erase(new_end, _variables.end());
            _scopeDepth--;
        }

        Bytecode::Register getEmpty(Bytecode::Chunk& chunk) {
            if(!_empty.has_value()) {
                _empty = allocateRegister();
                chunk.emit<Bytecode::Op::Load>(_empty.value(), TjsValue{});
            }
            return _empty.value();
        }

        std::optional<LocalVariable*> resolveLocalVariable(const std::string& identifier);
    };
}
