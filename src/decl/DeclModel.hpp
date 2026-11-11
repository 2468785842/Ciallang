// Copyright (c) 2026/11/11
//
// 声明文件 (.d.tjs) 模型（doc/declarations.md）：解析、生成与注册目录转换共享
// 同一符号模型；纯 C++ 所有权，不涉及 GC 与脚本可见行为。
// 本头文件保持轻量（仅 common 依赖）；ApiCatalog 转换只做前向声明，
// 实现所在的 DeclModel.cpp 才引入 stdlib/NativeRegister.hpp。
#pragma once

#include <string>
#include <vector>

#include "common/SourceSpan.hpp"

namespace cial {
    class PreProcessor;

    namespace Common {
        class Result;
        class SourceFile;
    } // namespace Common

    struct ApiCatalog;
} // namespace cial

namespace cial::decl {
    enum class SymbolKind { Namespace, Class, Function, StaticFunction, Property, Variable };

    // 参数：`name: type = 默认字面量` / `name*` / `*`（未命名收拢）
    struct Param {
        std::string name; // 空串 = 未命名收拢 `*`
        std::string type; // 空串 = 类型未知（省略注解）
        bool optional{}; // 带字面量默认值
        bool collapse{}; // 收拢参数，必须是参数表最后一项
        std::string defaultText; // 默认值字面量的源码原文；空 = 必选
    };

    struct Symbol {
        SymbolKind kind{ SymbolKind::Function };
        std::string name;
        std::string doc; // 紧邻声明上方的 /** */ 文档（已去注释标记）；空 = 无
        std::vector<Param> params; // Function/StaticFunction
        std::string ret; // 函数返回类型；属性/变量类型（属性取 getter 返回，缺 getter
                         // 时取 setter 参数类型）；空 = 未知
        bool hasGetter{}; // Property：显式 getter；两者皆假 = 访问方式未指定（简写）
        bool hasSetter{}; // Property：显式 setter
        bool constant{}; // Variable：const
        std::vector<std::string> extends; // Class：基类名（按声明顺序）
        std::vector<Symbol> members; // Namespace/Class
        Common::SourceSpan span{}; // 声明头位置（导航用）
    };

    struct Module {
        std::vector<Symbol> symbols;
    };

    // 解析声明文本（实现在 src/parser/DeclParser.cpp）。诊断写入 Result，
    // 码以 decl. 为前缀（doc/declarations.md §1）；词法/预处理错误原样透传。
    Module parseDeclarations(Common::SourceFile &source, Common::Result &diagnostics, PreProcessor &pp);

    // 模型 → 声明文本（实现在 DeclModel.cpp）。输出仅由模型决定：
    // 2 空格缩进、类/命名空间块后无分号、属性按 hasGetter/hasSetter 选简写或
    // 显式访问器、无名参数生成 argN 占位。generate(parse(text)) == text 幂等。
    std::string generateDeclarations(const Module &module);

    // 注册事实（doc/api-metadata.md）→ 模型（实现在 DeclModel.cpp）：
    // typeMembers+typeNames 生成类符号（按注册名排序），globals 按 owner 点分
    // 路径归位为嵌套 namespace/class；variadic 转单个未命名收拢参数；
    // 登记属性一律转简写（读写性在注册点不可推导）。
    Module declarationsFromCatalog(const ApiCatalog &catalog);
} // namespace cial::decl
