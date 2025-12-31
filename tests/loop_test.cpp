//
// Created by LiDong on 2025/12/6.
//

#include <catch.hpp>

#include "common/SourceFile.hpp"
#include "gen/IRGenerator.hpp"
#include "parser/Parser.hpp"
#include "parser/ast/AstNode.hpp"
#include "parser/ast/DeclNode.hpp"
#include "parser/ast/StmtNode.hpp"
#include "vm/Instruction.hpp"
#include "vm/VMState.hpp"

using namespace Cial;

TEST_CASE("循环 - while语法解析") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    sourceFile.load(r, "while (i < 10) i = i + 1;");
    Syntax::Parser parser{ sourceFile, astBuilder };

    auto *globalNode = parser.parse(r);
    REQUIRE(globalNode != nullptr);
    REQUIRE_FALSE(r.isFailed());

    auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
    REQUIRE(blockNode != nullptr);
    REQUIRE(blockNode->childrens.size() == 1);
    auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockNode->childrens[0]);
    REQUIRE(stmtDecl != nullptr);
    auto *whileStmt = dynamic_cast<const Syntax::WhileStmtNode *>(stmtDecl->statement);
    REQUIRE(whileStmt != nullptr);
    REQUIRE(whileStmt->test != nullptr);
    REQUIRE(whileStmt->body != nullptr);
}

TEST_CASE("循环 - while字节码结构") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};
    Syntax::Parser parser{ sourceFile, astBuilder };

    sourceFile.load(r, "var i = 0; while (i < 3) i = i + 1;");

    auto *globalNode = parser.parse(r);
    REQUIRE(globalNode != nullptr);
    REQUIRE_FALSE(r.isFailed());

    Inter::SymbolTable globalTable{};
    Inter::IRGenerator codeGen{ sourceFile, globalTable };
    auto chunk = codeGen.parseAst(r, globalNode);
    REQUIRE(chunk != nullptr);

    const auto &instructions = chunk->getInstVec();
    bool hasTest = false;
    bool hasJmpNE = false;
    bool hasJmp = false;
    for(const auto *inst : instructions) {
        using enum Bytecode::Op::OpCode;
        if(inst->opcode == Test)
            hasTest = true;
        if(inst->opcode == JmpNE)
            hasJmpNE = true;
        if(inst->opcode == Jmp)
            hasJmp = true;
    }
    REQUIRE(hasTest);
    REQUIRE(hasJmpNE);
    REQUIRE(hasJmp);
}

TEST_CASE("循环 - do while与for执行") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};
    Syntax::Parser parser{ sourceFile, astBuilder };

    SECTION("do-while执行") {
        sourceFile.load(r, "var j = 0; do { j = j + 1; } while (j < 5);");
        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        Inter::SymbolTable globalTable{};
        Inter::IRGenerator codeGen{ sourceFile, globalTable };
        auto chunk = codeGen.parseAst(r, globalNode);
        Runtime rt;
        Bytecode::VMState vm{ globalTable, rt };
        vm.allocCallFrame(chunk.get());
        vm.run();
        REQUIRE(vm.global("j").toInteger() == 5);
    }

    SECTION("for执行") {
        sourceFile.load(r, "var k = 0; var i; for (i = 0; i < 3; i = i + 1) { k = k + 1; }");
        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        Inter::SymbolTable globalTable{};
        Inter::IRGenerator codeGen{ sourceFile, globalTable };
        auto chunk = codeGen.parseAst(r, globalNode);

        Runtime rt;
        Bytecode::VMState vm{ globalTable, rt };
        vm.allocCallFrame(chunk.get());
        vm.run();
        REQUIRE(vm.global("k").toInteger() == 3);
    }
}

TEST_CASE("循环 - while执行结果") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};
    Syntax::Parser parser{ sourceFile, astBuilder };

    sourceFile.load(r, "var i = 0; while (i < 10) i = i + 1;");

    auto *globalNode = parser.parse(r);
    REQUIRE(globalNode != nullptr);
    REQUIRE_FALSE(r.isFailed());

    Inter::SymbolTable globalTable{};
    Inter::IRGenerator codeGen{ sourceFile, globalTable };
    auto chunk = codeGen.parseAst(r, globalNode);
    REQUIRE(chunk != nullptr);


    Runtime rt;
    Bytecode::VMState vm{ globalTable, rt };
    vm.allocCallFrame(chunk.get());
    vm.run();

    const auto idx = globalTable.getSymbolIndex("i");
    REQUIRE(idx);
    REQUIRE(vm.global(*idx).toInteger() == 10);
}
