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

#include <limits>

#include "Label.hpp"
#include "Register.hpp"
#include "common/Result.hpp"
#include "common/SourceFile.hpp"
#include "parser/ast/AstNode.hpp"
#include "vm/Chunk.hpp"

namespace cial::inter {

    // 每个生成的 Chunk（含函数/构造器/访问器）使用同一外部优化策略。
    using ChunkOptimizer = void (*)(vm::Chunk &, OptReg);
    void optimizeDefaultChunk(vm::Chunk &chunk, OptReg externallyObserved);

    class IRGenerator {

    public:
        explicit IRGenerator(Common::Result &r, Runtime &rt, Common::SourceFile &sourceFile,
                             ChunkOptimizer optimize = optimizeDefaultChunk);
        ~IRGenerator();
        IRGenerator(const IRGenerator &) = delete;
        IRGenerator &operator=(const IRGenerator &) = delete;

        Opt<vm::Chunk> parseAst(const syntax::AstNode *node, OptReg &optReg);

        void addLocalVar(LocalVariable variable) {
            _activeLocals.push_back(_localVars.size());
            _localVars.emplace_back(variable);
        }

        // 当前函数的未命名实参基（旧版 FuncDeclUnnamedArgArrayBase）：
        // `*` 展开实参转发本帧 %base.. 的实际实参；无 collapse 声明时 = 具名参数个数
        u32 unnamedArgBase{};

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
            if(reg.index() >= std::numeric_limits<u16>::max()) {
                throw std::runtime_error("too many registers");
            }
            return reg;
        }

        void freeRegister(const Register reg) {
            // loadVoidReg() 的常驻槽不能被临时值回收或由局部声明接管。
            if(!_empty || reg != *_empty)
                _freeRegisters.push_back(reg);
        }

        /**
         * create a global scope
         * ensue we don't get variable from the global scope
         */
        void makeVirtualGlobalScope() { beginScope(); }

#define DECLARE_AST_NODE_VISIT(name) void generate(const syntax::name *node, OptReg &optReg);
        CIAL_AST_NODE_ENUMS(DECLARE_AST_NODE_VISIT)
#undef DECLARE_AST_NODE_VISIT

    private:
        Box<vm::Chunk> _chunk = std::make_unique<vm::Chunk>();

        Runtime &_rt;
        Common::SourceFile &_sourceFile;
        Common::Result &_r;
        ChunkOptimizer _optimize;

        OptReg _empty{};

        Vec<Register> _freeRegisters{};

        using BreakContext = Vec<size_t>;
        Vec<BreakContext> _breakStack{};

        struct ContinueContext {
            Opt<Label> continueLabel;
            Vec<size_t> continues;
        };

        Vec<ContinueContext> _continueStack{};

        // 按绑定栈边界判定作用域；空块/参数/catch 可共享同一 PC，不能比较源码 PC。
        Vec<size_t> _scopeLocalStarts{};
        Vec<size_t> _activeLocals{};
        Vec<LocalVariable> _localVars{};

        u16 _regNextIndex{};

        [[nodiscard]] u32 getNextInstPos() const { return static_cast<u32>(this->_chunk->getInstVec().size()); }

        Label makeLabel() const { return Label{ getNextInstPos() }; }

        [[nodiscard]] bool isTopScope() const noexcept { return _scopeLocalStarts.size() == 1; }

        void beginScope() { _scopeLocalStarts.push_back(_activeLocals.size()); }

        void endScope() {
            while(_activeLocals.size() > _scopeLocalStarts.back()) {
                auto &localVar = _localVars[_activeLocals.back()];
                freeRegister(localVar.reg);
                localVar.endPC = makeLabel();
                _activeLocals.pop_back();
            }
            _scopeLocalStarts.pop_back();
        }

        LocalVariable *resolveLocalVariable(Atom identifier);
        LocalVariable *resolveCurrentLocalVariable(Atom identifier);

        // TJS 函数提升:把块内(顶层/函数体)的 FunctionDeclNode 声明注册
        // (Load + DGlobal)前置到块代码之前,调用先于声明文本位置也能命中
        void emitFunctionHoisting(const syntax::BlockStmtNode *node);

        Register loadVoidReg();

        // 补"返回 void"的 ret 到 parseAst 产出的 chunk（void Load 必须写入该 chunk 本体）
        void patchVoidRet(vm::Chunk &funChunk);

        FuncMeta *generateFuncMeta(const syntax::Parameters &parameters, const syntax::BlockStmtNode *body) const;

        bool expectValue(const syntax::ExprNode *node, Register &ret);

        // ++/-- 公共生成（前缀与后缀共用）：identifier 解析为局部/全局/this 路径。
        // 前缀表达式值为新值；postfix=true 时为旧值（TJS2 后缀语义）
        void generateIncDec(const Atom identifier, bool isIncrement, OptReg &optReg, bool postfix = false);

        void error(const std::string &message, const Common::SourceLocation &location) const {
            _sourceFile.error(_r, message, location);
        }
        [[nodiscard]] Atom getAtomFromToken(const syntax::Token &token) const;
        void genTokenValueLoadInst(Register reg, const syntax::Token &token) const;
    };
} // namespace cial::inter
