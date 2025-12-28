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

        Vec<Bytecode::Register> _freeRegisters{};

        struct LoopContext {
            Opt<Bytecode::Label> continueLabel;
            Opt<Bytecode::Label> breakLabel;
            Vec<Bytecode::Op::Instruction *> continues;
            Vec<Bytecode::Op::Instruction *> breaks;
        };

        Vec<LoopContext> _loopStack{};

        Vec<std::uint32_t> _scopeStartPC{};
        Vec<FuncMeta::LocalVariable> _localVars{};

        std::uint32_t _regNextIndex{ 0 };

        /**
         * allocate a temp register in this chunk
         * @return register
         */
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

        [[nodiscard]] std::uint32_t getNextInstPos() const {
            return static_cast<std::uint32_t>(this->_chunk->getInstVec().size());
        }

        Bytecode::Label makeLabel() const { return Bytecode::Label{ getNextInstPos() }; }

        std::uint32_t &getScopeInstPos() noexcept { return _scopeStartPC.back(); }

        bool isTopScope() const noexcept { return _scopeStartPC.size() == 1; }

        void beginScope() { _scopeStartPC.emplace_back(getNextInstPos()); }

        void endScope() {
            for(auto &localVar : _localVars) {
                if(localVar.startPC > _scopeStartPC.back()) {
                    freeRegister(localVar.reg);
                    localVar.endPC = getNextInstPos();
                }
            }
            _scopeStartPC.pop_back();
        }

        void addLocalVariable(FuncMeta::LocalVariable &&variable) { _localVars.emplace_back(variable); }

        std::optional<FuncMeta::LocalVariable *> resolveLocalVariable(Atom identifier);

        Bytecode::Register loadVoidReg(Bytecode::Chunk &chunk);

        FuncMeta generateChunk(const Syntax::FunctionDeclNode *node) const;

        void error(Common::Result &r, const std::string &message, const Common::SourceLocation &location) const {
            _sourceFile.error(r, message, location);
        }
    };
} // namespace Cial::Inter
