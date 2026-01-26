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

#include "Label.hpp"
#include "Register.hpp"
#include "common/Result.hpp"
#include "common/SourceFile.hpp"
#include "parser/ast/AstNode.hpp"
#include "vm/Chunk.hpp"

namespace cial::Inter {

    class IRGenerator {

    public:
        explicit IRGenerator(Common::Result &r, Runtime &rt, Common::SourceFile &sourceFile) :
            _rt(rt), _sourceFile(sourceFile), _r(r) {}

        Opt<Bytecode::Chunk> parseAst(const Syntax::AstNode *node, OptReg &retReg);

        void addLocalVar(LocalVariable &&variable) { _localVars.emplace_back(variable); }

        /**
         * allocate a temp register in this chunk
         * @return register
         */
        Register allocateRegister() {
            if(!_freeRegisters.empty()) {
                const Register reg = _freeRegisters.back();
                _freeRegisters.pop_back();
                return reg;
            }
            const Register reg{ _regNextIndex++ };
            return reg;
        }

        void freeRegister(const Register reg) { _freeRegisters.push_back(reg); }

        /**
         * create a global scope
         * ensue we don't get variable from the global scope
         */
        void makeVirtualGlobalScope() { _scopeStartPC.emplace_back(0); }

        void generate(const Syntax::ValueExprNode *, OptReg &);

        void generate(const Syntax::IdentifierExprNode *, OptReg &);

        void generate(const Syntax::InternalIdentifierExprNode *, OptReg &);

        void generate(const Syntax::BinaryExprNode *, OptReg &);

        void generate(const Syntax::PrefixUnaryExprNode *, OptReg &);

        void generate(const Syntax::SuffixUnaryExprNode *, OptReg &);

        void generate(const Syntax::ProcCallExprNode *, OptReg &);

        void generate(const Syntax::AssignExprNode *, OptReg &);

        void generate(const Syntax::FunctionExprNode *, OptReg &);

        void generate(const Syntax::BlockStmtNode *, OptReg &);

        void generate(const Syntax::ExprStmtNode *, OptReg &);

        void generate(const Syntax::TryStmtNode *, OptReg &);

        void generate(const Syntax::IfStmtNode *, OptReg &);

        void generate(const Syntax::SwitchStmtNode *, OptReg &);

        void generate(const Syntax::PropertyDeclNode *, OptReg &);

        void generate(const Syntax::VarDeclNode *, OptReg &);

        void generate(const Syntax::FunctionDeclNode *, OptReg &);

        void generate(const Syntax::ClassDeclNode *, OptReg &);

        void generate(const Syntax::StmtDeclNode *, OptReg &);

        void generate(const Syntax::DoWhileStmtNode *, OptReg &);

        void generate(const Syntax::ForStmtNode *, OptReg &);

        void generate(const Syntax::WhileStmtNode *, OptReg &);

        void generate(const Syntax::BreakStmtNode *, OptReg &);

        void generate(const Syntax::ContinueStmtNode *, OptReg &);

        void generate(const Syntax::ReturnStmtNode *, OptReg &);

        void generate(const Syntax::DebuggerStmtNode *, OptReg &) const;

        void generate(const Syntax::TernaryExprNode *, OptReg &);

    private:
        Box<Bytecode::Chunk> _chunk = std::make_unique<Bytecode::Chunk>();

        Runtime &_rt;
        Common::SourceFile &_sourceFile;
        Common::Result &_r;

        OptReg _empty{};

        Vec<Register> _freeRegisters{};

        using BreakContext = Vec<size_t>;
        Vec<BreakContext> _breakStack{};

        struct ContinueContext {
            Opt<Label> continueLabel;
            Vec<size_t> continues;
        };

        Vec<ContinueContext> _continueStack{};

        Vec<u32> _scopeStartPC{};
        Vec<LocalVariable> _localVars{};

        u64 _regNextIndex{ 0 };

        [[nodiscard]] u32 getNextInstPos() const { return static_cast<u32>(this->_chunk->getInstVec().size()); }

        Label makeLabel() const { return Label{ getNextInstPos() }; }

        [[nodiscard]] bool isTopScope() const noexcept { return _scopeStartPC.size() == 1; }

        void beginScope() { _scopeStartPC.emplace_back(getNextInstPos()); }

        void endScope() {
            for(auto &localVar : _localVars) {
                if(localVar.startPC.address() > _scopeStartPC.back()) {
                    freeRegister(Register{ localVar.reg });
                    localVar.endPC = Label{ getNextInstPos() };
                }
            }
            _scopeStartPC.pop_back();
        }

        LocalVariable *resolveLocalVariable(Atom identifier);

        Register loadVoidReg();

        FuncMeta *generateFuncMeta(const Syntax::Parameters &parameters, const Syntax::BlockStmtNode *body) const;

        bool expectValue(const Syntax::ExprNode *node, Register &ret);

        void error(const std::string &message, const Common::SourceLocation &location) const {
            _sourceFile.error(_r, message, location);
        }
        [[nodiscard]] Atom getAtomFromToken(const Syntax::Token &token) const;
        void genTokenValueLoadInst(Register reg, const Syntax::Token &token) const;
    };
} // namespace cial::Inter
