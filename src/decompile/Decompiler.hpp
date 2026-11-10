/*
 * Copyright (c) 2026/11/1
 *
 * 字节码反编译器（doc/bytecode/decompiler-plan.md，TAC 层直译路径，M0）。
 *
 * 两条入口共用一个核心：
 *  - 自产路径：Parser → IRGenerator(+Optimizer) → Chunk 树（沿常量图收集
 *    嵌套函数/类/属性），输入契约 = 后优化 TAC；
 *  - TJS2 路径：TjsBytecodeReader → TjsTranslator → TjsTranslatedProgram
 *    （壳由加载器元数据透传，体为各 context 的 Chunk 与 TacOrigin 来源表）。
 *
 * 产物从第一天起就是 {文本, 行表}：行表以 TAC 半开区间为主坐标（编码前快照——
 * Chunk::toBytecode 会原地改义 Rlc 下标），bc 路径附带 TJS2 码字区间与源偏移；
 * 提供原源码时才把偏移换算为行号（UTF-16 码元 → 行），不伪造。
 *
 * M0 范围：反汇编级诊断视图（显式标注、不可重编译）；严格模式（默认）对全部
 * 单元返回 Unsupported——表达式/语句恢复（M2/M3）落地后才可能 Success。
 */

#pragma once

#include <deque>

#include "common/SourceFile.hpp"
#include "tjsbc/TjsTranslator.hpp"
#include "vm/Chunk.hpp"
#include "vm/Constant.hpp"

namespace cial {
    class Runtime;

    namespace decompile {
        // 严格模式状态（方案 §3）：Unsupported 不生成可误当成完整程序的产物
        enum class Status : u8 { Success, Unsupported, InvalidInput, InternalError };

        // 行表一行：输出文本行 ↔ TAC 半开区间（主坐标）。空区间 = synthetic
        // （壳行/缩进/生成声明）。tjsWord/sourceOffset/sourceLine 仅 bc 路径可得。
        struct LineMapRow {
            u32 textLine{}; // 0 基，相对整个输出文本
            u32 tacStart{}, tacEnd{};
            bool synthetic{};
            Opt<u32> tjsWordStart{};
            Opt<i32> sourceOffset{}; // outputdebug 产物的源字符偏移（UTF-16 码元）
            Opt<u32> sourceLine{}; // 仅在提供原源码时换算
        };

        // 一个壳+体单元（顶层/函数/getter/setter/类/字段初始化）
        struct DecompiledUnit {
            std::string name;
            std::string kind; // top-level / function / property-getter / property-setter / class / class-field-init
            std::string text; // 以 \n 结尾的行序列
            Vec<LineMapRow> lines;
        };

        struct DecompileResult {
            Status status{ Status::Unsupported };
            // deque：壳组装（M4）在体发射中途追加子单元，引用/指针必须稳定
            std::deque<DecompiledUnit> units;
            Vec<std::string> diagnostics; // Unsupported/InvalidInput 的原因（逐单元）

            [[nodiscard]] std::string text() const; // 全部单元文本按序拼接
            // 行表 JSON（状态/逐单元/逐行：textLine、tac 半开区间、synthetic、
            // 可选 tjsWord/srcOffset/srcLine）——供工具与 LSP 消费
            [[nodiscard]] std::string mapJson() const;
        };

        struct Options {
            // 反汇编级诊断视图（显式标注不可重编译）；false = 严格模式，
            // M0 对全部单元 Unsupported，diagnostics 给出原因
            bool diagnosticView{};
            // 纯反汇编（与 diagnosticView 同用）：只渲染指令与来源表，不附恢复
            // 注释，也不记"视图不可重编译"诊断——反汇编本身不失败，状态可为
            // Success；供 cial-disasm 等只读工具消费
            bool disassemblyOnly{};
            // bc 路径可选：原源码文本（sourceOffset → 行号换算；无则只保留偏移）
            const Common::SourceFile *originalSource{};
        };

        // 自产路径：top 为编译产物顶层 Chunk（常量图遍历收集嵌套单元；非 const
        // 因 Chunk::getConstants 无 const 重载，模块只读不写）
        DecompileResult decompileChunk(Runtime &rt, vm::Chunk &top, const Options &options = {});
        // 编译产物持有时提供 const 重载（Compilation::chunk 经 Opt 解引用为 const）
        DecompileResult decompileChunk(Runtime &rt, const vm::Chunk &top, const Options &options = {});

        // TJS2 路径：壳透传自 TjsTranslatedProgram（ctClass 无 TAC 体，只出壳行）
        DecompileResult decompileProgram(Runtime &rt, const tjsbc::TjsTranslatedProgram &program,
                                         const Options &options = {});
    } // namespace decompile
} // namespace cial
