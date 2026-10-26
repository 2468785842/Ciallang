// Copyright (c) 2026/10/26
//
// TJS2 字节码 → Ciallang TAC 转译器（doc/bytecode/mapping.md 阶段 2，M1 范围）。
// 寄存器恒等平移：cialReg(r) = r + K（K = maxVariableCount + variableReserveCount）。
// 标志物化：%F = K+MFC+1（bool 值寄存器），%T = K+MFC+2（临时），%N = K+MFC+3（名字）。
// M1 支持集：§4.1 传送 / §4.2 基型双目+INC·DEC / 标志族+跳转 / §4.3 GPD·GPI·SPD·
// SPI·SPDE·SPDEH / §4.4 INT·REAL·STR·CHS·LNOT·INV·CHKINV·CHKINS·TYPEOF·ASC·CHR /
// §4.5 CALL·CALLD·CALLI / §4.6 SRV·RET·THROW·ENTRY·EXTRY·CHGTHIS·GLOBAL·DEBUGGER。
// 已知 M2 缺口：类/property context、NEW、PD/PI/P 运算变体、GPDS/SPDS 穿透族、
// SETP/GETP、DELD/DELI、TYPEOFD/I、collapse/展开调用、octet 常量、顶层 global
// proxy 注册模式（顶层函数定义经 ra[-1] 代理解析成员名，见 s1 顶层码字）。
#pragma once

#include "tjsbc/TjsBytecodeReader.hpp"
#include "vm/Chunk.hpp"
#include "vm/Constant.hpp"

namespace cial {

    class Runtime;
    struct FuncMeta;

    namespace tjsbc {

        // 转译产物：每个 context 一个 Chunk；函数 context 另有 FuncMeta
        //（INTER_OBJECT 常量经 Constant(FuncMeta*) 回填，createValue 建可调用对象）
        struct TjsTranslatedContext {
            i32 parent = -1;
            TjsContextType type = TjsContextType::TopLevel;
            i32 arity = 0;
            bool isFunctionLike = false; // Function/ExprFunction
            vm::Chunk *chunk = nullptr;  // GC 持有（翻译器根保护）
            FuncMeta *funcMeta = nullptr; // 仅 isFunctionLike
        };

        struct TjsTranslatedProgram {
            Vec<TjsTranslatedContext> contexts{};
            i32 topLevel = -1; // 顶层 context 索引（入口）
        };

        class TjsTranslator {
        public:
            TjsTranslator(Runtime &rt, const TjsBytecodeFile &file);
            ~TjsTranslator();

            TjsTranslator(const TjsTranslator &) = delete;
            TjsTranslator &operator=(const TjsTranslator &) = delete;

            // 转译全部 context（产物生命周期由翻译器 GC 根保护至析构）
            TjsTranslatedProgram translate() const;

        private:
            struct Impl;
            std::unique_ptr<Impl> _impl;
        };

    } // namespace tjsbc

} // namespace cial
