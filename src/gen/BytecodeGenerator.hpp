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

#include "ast/Ast.hpp"
#include "common/Result.hpp"
#include "common/SourceFile.hpp"
#include "types/TjsString.hpp"
#include "vm/Chunk.hpp"
#include "vm/Label.hpp"
#include "vm/Register.hpp"

namespace Ciallang::Inter {
    struct LocalVariable {
        std::string identifier;
        Bytecode::Register reg;
        size_t scopeDepth;
        bool init;
    };

    class SymbolTable {
    public:
        [[nodiscard]] std::optional<size_t> getSymbolIndex(const std::string &identifier) const {
            const auto symbol = _table.find(identifier);
            if(symbol == _table.end()) {
                return {};
            }
            return symbol->second;
        }

        size_t addSymbol(const std::string &identifier) {
            _table[identifier] = _symbolNextIndex++;
            return _symbolNextIndex - 1;
        }

        size_t getOrAddSymbol(const std::string &identifier) {
            if(const auto s = getSymbolIndex(identifier)) {
                return *s;
            }
            return addSymbol(identifier);
        }

        [[nodiscard]] const char *getSymbol(const size_t index) const {
            for(const auto &[k, v] : _table) {
                if(v == index) {
                    return k.c_str();
                }
            }
            return "";
        }

    private:
        std::unordered_map<std::string, size_t> _table{};
        std::uint32_t _symbolNextIndex{ 0 };
    };

    class BytecodeGenerator {
    public:
        explicit BytecodeGenerator(Common::SourceFile &sourceFile, SymbolTable &symbolTable) :
            _sourceFile(sourceFile), _symbolTable(symbolTable) {}

        std::unique_ptr<Bytecode::Chunk> parseAst(const Common::Result &r, const Syntax::AstNode *node);

        void error(Common::Result &r, const std::string &message, const Common::SourceLocation &location) const {
            _sourceFile.error(r, message, location);
        }

        void addVariable(LocalVariable &&variable) { _variables.push_back(std::move(variable)); }

        std::optional<Bytecode::Register> generate(const Syntax::ValueExprNode *);

        std::optional<Bytecode::Register> generate(const Syntax::IdentifierExprNode *);

        std::optional<Bytecode::Register> generate(const Syntax::BinaryExprNode *);

        std::optional<Bytecode::Register> generate(const Syntax::UnaryExprNode *);

        std::optional<Bytecode::Register> generate(const Syntax::ProcCallExprNode *);

        std::optional<Bytecode::Register> generate(const Syntax::AssignExprNode *);

        std::optional<Bytecode::Register> generate(const Syntax::BlockStmtNode *);

        std::optional<Bytecode::Register> generate(const Syntax::ExprStmtNode *);

        std::optional<Bytecode::Register> generate(const Syntax::IfStmtNode *);

        std::optional<Bytecode::Register> generate(const Syntax::VarDeclNode *);

        std::optional<Bytecode::Register> generate(const Syntax::FunctionDeclNode *);

        std::optional<Bytecode::Register> generate(const Syntax::StmtDeclNode *);

        std::optional<Bytecode::Register> generate(const Syntax::WhileStmtNode *);

        std::optional<Bytecode::Register> generate(const Syntax::BreakStmtNode *);

        std::optional<Bytecode::Register> generate(const Syntax::ContinueStmtNode *);

        std::optional<Bytecode::Register> generate(const Syntax::ReturnStmtNode *);

    private:
        std::unique_ptr<Bytecode::Chunk> _chunk = std::make_unique<Bytecode::Chunk>();

        Common::SourceFile &_sourceFile;
        Common::Result _r{};

        size_t _scopeDepth{ 0 };

        std::vector<LocalVariable> _variables{};

        std::optional<Bytecode::Register> _empty{};

        std::vector<Bytecode::Register> _freeRegisters{};
        std::uint32_t _regNextIndex{ 0 };

        SymbolTable &_symbolTable;

        Bytecode::Register allocateRegister() {
            if(!_freeRegisters.empty()) {
                const Bytecode::Register reg = _freeRegisters.back();
                _freeRegisters.pop_back();
                return reg;
            }
            const Bytecode::Register reg{ _regNextIndex++ };
            _chunk->setRegisterCount(std::max(_chunk->getRegisterCount(), _regNextIndex));
            return reg;
        }

        void freeRegister(const Bytecode::Register reg) { _freeRegisters.push_back(reg); }

        Bytecode::Label makeLabel() const { return Bytecode::Label{ _chunk->instructions().size() }; }

        void beginScope() { _scopeDepth++; }

        void endScope() {
            const auto new_end = std::ranges::remove_if(_variables, [&](const LocalVariable &variable) {
                                     if(variable.scopeDepth == _scopeDepth) {
                                         freeRegister(variable.reg);
                                         return true;
                                     }
                                     return false;
                                 }).begin();

            _variables.erase(new_end, _variables.end());
            _scopeDepth--;
        }

        Bytecode::Register getEmpty(Bytecode::Chunk &chunk) {
            if(!_empty.has_value()) {
                _empty = allocateRegister();
                chunk.emit<Bytecode::Op::OpCode::Load>(_empty.value(), TjsValue{});
            }
            return _empty.value();
        }

        std::optional<LocalVariable *> resolveLocalVariable(const TjsString &identifier);
    };
} // namespace Ciallang::Inter
