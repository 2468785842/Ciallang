// Copyright (c) 2026/10/26
//
// TJS2 字节码 → Ciallang TAC 转译器（doc/bytecode/mapping.md 阶段 2，M1+M2 范围）。
// 寄存器恒等平移：cialReg(r) = r + K（K = max(VC+RC, argc+2)）。
// 标志物化：%F = K+MFC+1（bool 值寄存器），%T/%T2 = +2/+3（临时），%N = +4（名字）。
// M1 支持集：§4.1 传送 / §4.2 基型+INC·DEC·PD·PI / 标志族+跳转 / §4.3 GPD·GPI·
// SPD·SPI 全族（含 S 穿透变体）/ §4.4 转换一元 / §4.5 CALL 族 / §4.6 返回异常全族。
// M2 支持集：ctClass 加载期解释（var 成员/initDefaultVal/构造器/成员 property/
// superClassGetter→extends）、NEW→Call（ClassObject::call 构造）、P 变体
// （GETP/SETP/DerefGet·Set 与 INCP/二目 P 的读-算-写）、DELD/DELI→DelProp、
// TYPEOFD/I→GProp+TypeOf、octet 常量、proxy(ra[-2]) 访存→GThis/DThis/GRefRaw
// （this→global 回落链，TJS2 objthis-proxy 语义）。
// 已知近似（见 mapping.md §8.1）：TYPEOFD/I 成员缺失时抛错（TJS2 返回
// "undefined"）；GPI/SPIS/CALLI 经 proxy 无回落；SPDS 经 proxy 落 this 槽。
// 仍缺口（M4）：collapse 参数、省略/展开调用（argc=-1/-2，加载期报错）、
// EVAL/EEXP、octet 转换指令。
#pragma once

#include "runtime/AtomTable.hpp"
#include "tjsbc/TjsBytecodeReader.hpp"
#include "vm/Chunk.hpp"
#include "vm/Constant.hpp"
#include <memory>

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
            // ctClass：加载期解释产物（body 不生成 TAC）
            bool isClass = false;
            ClassMeta *classMeta = nullptr;
            // ctProperty：getter/setter 聚合
            bool isProperty = false;
            PropMeta *propMeta = nullptr;
            Atom nameAtom{}; // ctx 名（STRING 池下标 intern）
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
