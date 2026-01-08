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

#include "common/Result.hpp"
#include "common/SourceFile.hpp"
#include "parser/ast/AstNode.hpp"
#include "types/String.hpp"
#include "vm/Chunk.hpp"
#include "vm/Label.hpp"
#include "vm/Register.hpp"

namespace cial::Inter {

    class IRGenerator {

    public:
        explicit IRGenerator(Runtime &rt, Common::SourceFile &sourceFile) : _rt(rt), _sourceFile(sourceFile) {}

        Opt<Bytecode::Chunk> parseAst(const Common::Result &r, const Syntax::AstNode *node, OptReg &retReg);

        void addLocalVar(LocalVariable &&variable) { _localVars.emplace_back(variable); }

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
            return reg;
        }

        void freeRegister(const Bytecode::Register reg) { _freeRegisters.push_back(reg); }

        /**
         * create a global scope
         * ensue we don't get variable from the global scope
         */
        void makeVirtualGlobalScope() { _scopeStartPC.emplace_back(0); }

        void generate(const Syntax::ValueExprNode *, OptReg &);

        void generate(const Syntax::IdentifierExprNode *, OptReg &);

        void generate(const Syntax::BinaryExprNode *, OptReg &);

        void generate(const Syntax::UnaryExprNode *, OptReg &);

        void generate(const Syntax::ProcCallExprNode *, OptReg &);

        void generate(const Syntax::AssignExprNode *, OptReg &);

        void generate(const Syntax::FunctionExprNode *, OptReg &);

        void generate(const Syntax::BlockStmtNode *, OptReg &);

        void generate(const Syntax::ExprStmtNode *, OptReg &);

        void generate(const Syntax::IfStmtNode *, OptReg &);

        void generate(const Syntax::SwitchStmtNode *, OptReg &);

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

        void generate(const Syntax::ConditionalTernaryExprNode *, OptReg &);

    private:
        Box<Bytecode::Chunk> _chunk = std::make_unique<Bytecode::Chunk>();

        Runtime &_rt;
        Common::SourceFile &_sourceFile;
        Common::Result _r{};

        OptReg _empty{};

        Vec<Bytecode::Register> _freeRegisters{};

        struct LoopContext {
            Opt<Bytecode::Label> continueLabel;
            Opt<Bytecode::Label> breakLabel;
            Vec<Bytecode::Op::Instruction *> continues;
            Vec<Bytecode::Op::Instruction *> breaks;
        };

        Vec<LoopContext> _loopStack{};

        Vec<std::uint32_t> _scopeStartPC{};
        Vec<LocalVariable> _localVars{};

        std::uint64_t _regNextIndex{ 0 };

        [[nodiscard]] std::uint32_t getNextInstPos() const {
            return static_cast<std::uint32_t>(this->_chunk->getInstVec().size());
        }

        Bytecode::Label makeLabel() const { return Bytecode::Label{ getNextInstPos() }; }

        [[nodiscard]] bool isTopScope() const noexcept { return _scopeStartPC.size() == 1; }

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

        LocalVariable *resolveLocalVariable(Atom identifier);

        Bytecode::Register loadVoidReg(Bytecode::Chunk &chunk);

        FuncMeta *generateFuncMeta(const Syntax::Parameters &parameters, Syntax::BlockStmtNode *body) const;

        bool expectValue(const Syntax::ExprNode *node, Bytecode::Register &ret);

        void error(const std::string &message, const Common::SourceLocation &location) {
            _sourceFile.error(_r, message, location);
        }
    };
} // namespace cial::Inter
