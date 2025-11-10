/*
 * Copyright (c) 2024/12/18
 *
 * 虚拟机指令单元测试
 */

#include <catch.hpp>
#include <memory>

#include "types/TjsTypes.hpp"
#include "types/TjsValue.hpp"
#include "vm/Instruction.hpp"
#include "vm/Label.hpp"
#include "vm/Register.hpp"

using namespace Ciallang;

TEST_CASE("虚拟机指令 - 基本操作码") {
    SECTION("Load 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::Load };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::Load);
        REQUIRE(instr.operandCount() == 0);
    }

    SECTION("PushReg 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::PushReg };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::PushReg);
        REQUIRE(instr.operandCount() == 0);
    }

    SECTION("PopN 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::PopN };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::PopN);
        REQUIRE(instr.operandCount() == 0);
    }

    SECTION("Add 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::Add };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::Add);
        REQUIRE(instr.operandCount() == 0);
    }

    SECTION("Sub 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::Sub };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::Sub);
        REQUIRE(instr.operandCount() == 0);
    }

    SECTION("Mul 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::Mul };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::Mul);
        REQUIRE(instr.operandCount() == 0);
    }

    SECTION("Div 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::Div };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::Div);
        REQUIRE(instr.operandCount() == 0);
    }

    SECTION("Mov 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::Mov };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::Mov);
        REQUIRE(instr.operandCount() == 0);
    }

    SECTION("Jmp 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::Jmp };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::Jmp);
        REQUIRE(instr.operandCount() == 0);
    }

    SECTION("Call 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::Call };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::Call);
        REQUIRE(instr.operandCount() == 0);
    }

    SECTION("Ret 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::Ret };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::Ret);
        REQUIRE(instr.operandCount() == 0);
    }
}

TEST_CASE("虚拟机指令 - 操作数类型") {
    SECTION("Value 操作数") {
        Ciallang::TjsValue value{ static_cast<Ciallang::TjsInteger>(42) };
        Ciallang::Bytecode::Op::Operand operand{ std::move(value) };
        REQUIRE(operand.type == Ciallang::Bytecode::Op::Operand::Type::Value);
        REQUIRE(operand.operand.value != nullptr);
        REQUIRE(operand.operand.value->toInteger() == 42);
    }

    SECTION("Register 操作数") {
        Ciallang::Bytecode::Register reg{ 5 };
        Ciallang::Bytecode::Op::Operand operand{ reg };
        REQUIRE(operand.type == Ciallang::Bytecode::Op::Operand::Type::Register);
        REQUIRE(operand.operand.reg.index() == 5);
    }

    SECTION("Label 操作数") {
        Ciallang::Bytecode::Label label{ 0 };
        Ciallang::Bytecode::Op::Operand operand{ label };
        REQUIRE(operand.type == Ciallang::Bytecode::Op::Operand::Type::Label);
        REQUIRE(operand.operand.label.address() == 0);
    }

    SECTION("SymbolIndex 操作数") {
        size_t sym{ 10 };
        Ciallang::Bytecode::Op::Operand operand{ sym };
        REQUIRE(operand.type == Ciallang::Bytecode::Op::Operand::Type::SymbolIndex);
        REQUIRE(operand.operand.i == 10);
    }
}

TEST_CASE("虚拟机指令 - 带操作数的指令") {
    SECTION("带 Value 操作数的指令") {
        Ciallang::TjsValue value{ static_cast<Ciallang::TjsInteger>(100) };
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::Load,
                                                   Ciallang::Bytecode::Op::Operand{ std::move(value) } };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::Load);
        REQUIRE(instr.operandCount() == 1);
        REQUIRE(instr.getOperand1<Ciallang::TjsValue>().toInteger() == 100);
    }

    SECTION("带 Register 操作数的指令") {
        Ciallang::Bytecode::Register reg{ 3 };
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::Mov,
                                                   Ciallang::Bytecode::Op::Operand{ reg } };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::Mov);
        REQUIRE(instr.operandCount() == 1);
        REQUIRE(instr.getOperand1<Ciallang::Bytecode::Register>().index() == 3);
    }

    SECTION("带 Label 操作数的指令") {
        Ciallang::Bytecode::Label label{ 100 };
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::Jmp,
                                                   Ciallang::Bytecode::Op::Operand{ label } };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::Jmp);
        REQUIRE(instr.operandCount() == 1);
        REQUIRE(instr.getOperand1<Ciallang::Bytecode::Label>().address() == 100);
    }
}

// 字节码生成测试需要Chunk类，暂时跳过

TEST_CASE("虚拟机指令 - 算术运算指令") {
    SECTION("Add 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::Add };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::Add);
    }

    SECTION("Sub 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::Sub };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::Sub);
    }

    SECTION("Mul 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::Mul };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::Mul);
    }

    SECTION("Div 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::Div };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::Div);
    }
}

TEST_CASE("虚拟机指令 - 比较运算指令") {
    SECTION("Eq 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::EQ };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::EQ);
    }

    SECTION("Ne 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::NEQ };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::NEQ);
    }

    SECTION("Lt 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::LT };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::LT);
    }

    SECTION("Le 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::LE };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::LE);
    }

    SECTION("Gt 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::GT };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::GT);
    }

    SECTION("Ge 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::GE };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::GE);
    }
}

// 逻辑运算指令在当前指令集中不存在，跳过

TEST_CASE("虚拟机指令 - 控制流指令") {
    SECTION("Jmp 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::Jmp };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::Jmp);
    }

    SECTION("JmpE 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::JmpE };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::JmpE);
    }

    SECTION("JmpNE 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::JmpNE };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::JmpNE);
    }

    SECTION("Call 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::Call };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::Call);
    }

    SECTION("Ret 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::Ret };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::Ret);
    }
}

TEST_CASE("虚拟机指令 - 内存操作指令") {
    SECTION("Load 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::Load };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::Load);
    }

    SECTION("PushReg 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::PushReg };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::PushReg);
    }

    SECTION("PopN 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::PopN };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::PopN);
    }

    SECTION("Mov 指令") {
        Ciallang::Bytecode::Op::Instruction instr{ Ciallang::Bytecode::Op::OpCode::Mov };
        REQUIRE(instr.opcode == Ciallang::Bytecode::Op::OpCode::Mov);
    }
}

// 复杂指令序列测试需要Chunk类，暂时跳过