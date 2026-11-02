/*
 * Copyright (c) 2026/11/1
 *
 * 反编译器 M2 表达式恢复（doc/bytecode/decompiler-plan.md §5.2/§5.7）。
 *
 * 输入契约 = 后优化 TAC（方案 §2）：自产路径经 optimizeDefaultChunk，TJS2 路径
 * 为转译模板原样——两条路径在 Chunk TAC 上同构，本模块统一消费。范围内为单块
 * 直线代码：出现跳转/with/try/eval 等语句结构（M3/M5）或声明装配（M4）时如实
 * 报 Unsupported，不输出带缺口的半成品。
 *
 * 恢复规则（方案 §5.2）：按后优化形态表 tile 查表（LoadImm 立即数融合族、
 * CompareTest/CompareTestImm、拷贝传播后的形态），单次求值（单使用链）才内联
 * 临时值；多使用寄存器在函数体内物化为合成临时变量，顶层无局部作用域，纯读
 * 允许复制、非纯读多使用即 Unsupported（避免物化出可观察的全局状态）。
 */

#pragma once

#include "vm/Chunk.hpp"
#include "vm/Constant.hpp"

namespace cial {
    class Runtime;

    namespace decompile {
        // 一条恢复出的语句：文本 + 产生它的 TAC 下标（行表区间由调用方按序闭合）
        struct RecoveredStatement {
            u32 pc{};
            std::string text;
        };

        struct ExprRecovery {
            Vec<RecoveredStatement> statements;
            // 非空 = 单元不可恢复及原因（逐单元诊断，不产半成品文本）
            std::string unsupported;
        };

        // 直线块表达式恢复。meta 为函数体单元的 FuncMeta（局部名/参数名），
        // 顶层单元传 nullptr；functionLike 决定局部物化与隐式 void return 的处理
        ExprRecovery recoverUnitBody(Runtime &rt, vm::Chunk &chunk, const FuncMeta *meta, bool functionLike);
    } // namespace decompile
} // namespace cial
