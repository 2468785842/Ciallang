/*
 * Copyright (c) 2026/11/1
 *
 * 反编译器表达式/语句恢复（doc/bytecode/decompiler-plan.md §5.2/§5.4/§5.7）。
 *
 * 输入契约 = 后优化 TAC（方案 §2）：自产路径经 optimizeDefaultChunk，TJS2 路径
 * 为转译模板原样——两条路径在 Chunk TAC 上同构，本模块统一消费。
 *
 * M2：单块直线表达式恢复——后优化形态表 tile（LoadImm 立即数融合族、
 * CompareTest/CompareTestImm、拷贝传播后的形态），单次求值（单使用链）才内联
 * 临时值；多使用寄存器在函数体内物化为合成临时变量，顶层无局部作用域，纯读
 * 允许复制、非纯读多使用即 Unsupported（避免物化出可观察的全局状态）。
 * M3：CFG 块 + 条件区语句结构化（if/else、while/do-while、break/continue、
 * 三元跨块折叠）。
 * M4：声明装配与复合赋值逆融合——Load 声明常量（FuncMeta/ClassMeta/PropMeta）
 * 经 DeclHost 装配为 function/class/property 声明文本；GProp+Op+DProp 模板
 * 满足"单读者"守卫时逆融合为 +=/++（防 getter 计数翻倍的误融合）。
 */

#pragma once

#include "vm/Chunk.hpp"
#include "vm/Constant.hpp"

namespace cial {
    class Runtime;

    namespace decompile {
        class DeclHost;

        // 一条恢复出的语句：文本 + 产生它的 TAC 区间（行表按序闭合）。
        // tacEnd = 0 表示单指令（pc+1）；声明装配等多指令 tile 给出显式区间。
        // 文本为相对缩进：首行顶格、内层行按层级带缩进，写入期由宿主叠加体 pad
        struct RecoveredStatement {
            u32 pc{};
            std::string text;
            u32 tacEnd{};
            bool synthetic{}; // 降级框架/合成局部声明不冒充原始 TAC 来源。
        };

        struct ExprRecovery {
            Vec<RecoveredStatement> statements;
            // 非空 = 单元不可恢复及原因（逐单元诊断，不产半成品文本）
            std::string unsupported;
        };

        // 声明形态：Bare = 裸声明（function f(){} / class A{} / property p{}），
        // Assign = 赋值右值字面量（var f = function(){...} / obj.m = function(){...}）
        enum class DeclForm : u8 { Bare, Assign };

        // 声明装配宿主（M4）：ExprBuilder 遇到 Load 声明常量 + 安装链时回调，
        // 由持有壳元数据上下文的一侧（Decompiler 收集器）装配声明文本。
        // 返回文本为完整多行块（含各层缩进，首行顶格）；不可恢复时置 reason。
        // literalName：Bare = 声明名；Assign = ""（匿名函数字面量）。
        // 类/property 仅支持 Bare；Assign 形式的类/property 由调用方拒绝
        class DeclHost {
        public:
            virtual ~DeclHost() = default;
            // 参考 ctExprFunction 的显示名非空，但没有词法自绑定；仅来源元数据
            // 可以证明它能按匿名字面量输出，不能仅凭名字字符串猜测。
            [[nodiscard]] virtual bool isAnonymousExpression(const FuncMeta &) const { return false; }
            [[nodiscard]] virtual std::string declarationText(const Constant &constant, DeclForm form,
                                                              const std::string &literalName, u32 indent,
                                                              std::string &reason) = 0;
        };

        // 表达式/语句恢复。meta 为函数体单元的 FuncMeta（局部名/参数名），
        // 顶层单元传 nullptr；functionLike 决定局部物化与隐式 void return 的处理；
        // host 为声明装配宿主（M4），nullptr 时声明常量如实拒绝
        ExprRecovery recoverUnitBody(Runtime &rt, vm::Chunk &chunk, const FuncMeta *meta, bool functionLike,
                                     DeclHost *host = nullptr);
    } // namespace decompile
} // namespace cial
