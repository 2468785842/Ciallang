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

#include "ast/AstNode.hpp"
#include "common/Result.hpp"
#include "common/SourceFile.hpp"
#include "types/String.hpp"
#include "vm/Chunk.hpp"
#include "vm/Label.hpp"
#include "vm/Register.hpp"

namespace Cial::Inter {

    class IRGenerator {

    public:
        explicit IRGenerator(Common::SourceFile &sourceFile) : _sourceFile(sourceFile) {}

        std::unique_ptr<Bytecode::Chunk> parseAst(const Common::Result &r, const Syntax::AstNode *node);

        Syntax::OptReg generate(const Syntax::ValueExprNode *);

        Syntax::OptReg generate(const Syntax::IdentifierExprNode *);

        Syntax::OptReg generate(const Syntax::BinaryExprNode *);

        Syntax::OptReg generate(const Syntax::UnaryExprNode *);

        Syntax::OptReg generate(const Syntax::ProcCallExprNode *);

        Syntax::OptReg generate(const Syntax::AssignExprNode *);

        Syntax::OptReg generate(const Syntax::BlockStmtNode *);

        Syntax::OptReg generate(const Syntax::ExprStmtNode *);

        Syntax::OptReg generate(const Syntax::IfStmtNode *);

        Syntax::OptReg generate(const Syntax::VarDeclNode *);

        Syntax::OptReg generate(const Syntax::FunctionDeclNode *);

        Syntax::OptReg generate(const Syntax::ClassDeclNode *);

        Syntax::OptReg generate(const Syntax::StmtDeclNode *);

        Syntax::OptReg generate(const Syntax::DoWhileStmtNode *);

        Syntax::OptReg generate(const Syntax::ForStmtNode *);

        Syntax::OptReg generate(const Syntax::WhileStmtNode *);

        Syntax::OptReg generate(const Syntax::BreakStmtNode *);

        Syntax::OptReg generate(const Syntax::ContinueStmtNode *);

        Syntax::OptReg generate(const Syntax::ReturnStmtNode *);

    private:
        std::unique_ptr<Bytecode::Chunk> _chunk = std::make_unique<Bytecode::Chunk>();

        Common::SourceFile &_sourceFile;
        Common::Result _r{};

        Syntax::OptReg _empty{};

        std::vector<Bytecode::Register> _freeRegisters{};
        std::uint32_t _regNextIndex{ 0 };

        struct LoopContext {
            std::optional<Bytecode::Label> continueLabel;
            std::optional<Bytecode::Label> breakLabel;
            std::vector<Bytecode::Op::Instruction *> continues;
            std::vector<Bytecode::Op::Instruction *> breaks;
        };

        std::vector<LoopContext> _loopStack{};

        struct LocalVariable {
            Bytecode::Register reg;
            bool init;
        };

        struct Variable {
            Atom identifier;
            std::optional<LocalVariable> localVar;
        };

        struct ScopeContext {
            std::vector<Variable> variables{};
        };

        std::vector<ScopeContext> _scopeChain{};

        Bytecode::Register allocateRegister() {
            if(!_freeRegisters.empty()) {
                const Bytecode::Register reg = _freeRegisters.back();
                _freeRegisters.pop_back();
                return reg;
            }
            const Bytecode::Register reg{ _regNextIndex++ };
            _chunk->setRegisterCount(_regNextIndex);
            return reg;
        }

        void freeRegister(const Bytecode::Register reg) { _freeRegisters.push_back(reg); }

        Bytecode::Label makeLabel() const { return Bytecode::Label{ _chunk->getInstVec().size() }; }

        void beginScope() { _scopeChain.emplace_back(); }

        void endScope() {
            const auto &scope = _scopeChain.back();
            for(const auto &var : scope.variables) {
                if(var.localVar.has_value()) {
                    freeRegister(var.localVar->reg);
                }
            }
            _scopeChain.pop_back();
        }

        void addVariable(Variable &&variable) { _scopeChain.back().variables.emplace_back(variable); }

        Bytecode::Register loadVoidReg(Bytecode::Chunk &chunk);

        std::optional<Variable *> resolveLocalVariable(Atom identifier);

        std::unique_ptr<Bytecode::Chunk> generateChunk(const Syntax::FunctionDeclNode *node) const;

        void error(Common::Result &r, const std::string &message, const Common::SourceLocation &location) const {
            _sourceFile.error(r, message, location);
        }
    };
} // namespace Cial::Inter
