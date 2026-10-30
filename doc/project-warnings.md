# 全项目 CLion 诊断报告（2026-10-30）

本轮扫描时只检查、不修改代码。后续按用户要求已删除未被引用的失效头文件
`src/parser/ast/AstFormatter.hpp`；下面的统计与 JSON 保留为删除前的历史快照。CLion `get_file_problems --errorsOnly false` 检查了 **198 个文件**，
全部返回结果，无工具调用失败。原始数据见 [完整 JSON](reports/clion-inspections-2026-10-30.json)，
包含每个文件、诊断级别、行列、原始描述和源码行，也包含零提示文件。

| 编辑器级别 | 数量 |
|---|---:|
| ERROR | 3 |
| WARN | 358 |
| WEAK WARNING | 10 |
| INFO | 1465 |

68 个文件有 ERROR/WARN/弱警告；32 个文件无任何提示，其余文件只有 INFO。
INFO 中 598 条为拼写提示，139 条为 DateWord 指定初始化建议，不能当成运行时缺陷计数。
同一根因可能产生多条提示，诊断条数不等于独立缺陷数。

## 范围与限制

- 范围涵盖 `src/`、`tests/`、`tools/lsp/`、`bench/`、`fuzz/`、`editors/vscode/`，以及根目录/
  `cmake/`/`.github/` 的 CMake、JSON、YAML、shell 配置。
- 不包括 build/bin、第三方依赖、IDE 元数据、文档和参考 grammar、TJS 脚本及字节码 fixture；
  这些不属于本轮 CLion 项目代码警告清单。
- 环境：macOS arm64，CLion `CL-262.11879.74`，已加载 MacOS Debug / Release 配置。
  逐文件接口使用 IDE 当前分析上下文，不代表分别遍历了两个配置，也不覆盖 Windows/Linux
  条件编译和未启用的 JIT/fuzz 配置。
- 这是编辑器静态分析快照，不是编译器全量告警构建，也不是无缺陷证明。未为本轮重新运行
  全量测试；仅对下述两个诊断文件做了独立编译验证，所有源码保持不变。
- CLion 级别与编译器级别可能不同，部分跨函数数据流和宏展开提示需人工确认。

## 已复核的重点

| 位置 | 编辑器诊断 | 人工复核与建议 |
|---|---|---|
| `src/parser/ast/AstFormatter.hpp:188/192` | 2 条 ERROR，另有抽象类/无法解析成员等关联 WARN | **优先修复或明确移除旧实现。** 已用 C++20 编译器单独包含该头，得到 5 条 error、1 条 warning。旧 UnaryExprNode 已不存在，ClassDeclNode 无 body，Token 是值而非指针，visitor 未覆盖新节点。源码检索未发现当前构建引用该头，因此这是未被主构建覆盖的失效代码，不是当前 CLI 的构建失败。 |
| `src/types/RegExp.cpp:60` | 1 条 ERROR：局部 pattern 遮蔽字段 | **编辑器级别偏重，可作命名清理。** 同样的当前源码用 clang++ -std=c++20 -fsyntax-only 独立编译成功；局部变量传给新 RegExp 的构造器，是合法 C++，未证实语义错误。 |
| `src/tjsbc/TjsTranslator.cpp:447–514/603/1135/1150` | 12 条算术后转换提示、2 条 size_t→i32 缩窄提示 | **值得边界复核。** 检查 i32 指令偏移的加法及 size_t TAC 下标进入 i32 的范围约束；wordToTac 的显式 i32 返回类型中也存在返回 TAC 索引的转换。未构造出溢出输入，不将这些提示直接认定为可触发缺陷。 |
| `src/cli/Compilation.cpp:20/35`、`src/cli/Application.cpp:66` | 3 条可能未初始化 | **目前未确认实际未初始化。** Compilation.chunk 有默认初始化，result 为默认构造的 optional（`src/gen/Register.hpp`）；继续保留诊断，不按未初始化整数处理。 |
| `src/gen/Optimizer.cpp:627/687` | destination 未初始化 | **当前控制流有保护。** tryGetImmediateLoadDestination 成功时先赋值再返回 true，调用方仅在 true 分支读取；暂不认定为 UB。 |
| `src/types/Value.hpp:207` | reserved 未初始化 | **布局预留字节需区别于业务字段。** 检查其后是否存在语义读取或整块比较，再决定是否初始化；不要为了消警告直接给热路径增加清零。本轮未证明发生非法读取。 |
| `src/stdlib/SerializeLib.cpp:595`、`src/tjsbc/TjsBytecodeReader.cpp:24` | aggregate 字段 data/size 未初始化 | 当前使用点通过聚合初始化传入数据和长度（SerializeLib.cpp:1865/1931、TjsBytecodeReader.cpp:263）；未确认现有使用点遗漏。 |
| `tools/lsp/cial_lsp.py:951` | 某些路径缺少 return（弱警告） | TemporaryDirectory 内正常路径及已捕获异常路径均有 return；文件写入异常会抛出。优先核对上下文管理器退出路径建模，不将其直接认定为返回 None。 |

独立编译使用 `/usr/bin/clang++ -std=c++20 -fsyntax-only -DUTF8PROC_STATIC`，配置 `src` 与
当前 Debug vcpkg include 路径。日志临时保存在 `/tmp/cial-ast-syntax-check.log`、
`/tmp/cial-regexp-syntax-check.log`；AST 头检查入口为 `/tmp/cial-ast-formatter-check.cpp`。
完整编译诊断不等于 CLion 的错误条数，所以这里出现 5 条编译错误与 2 条编辑器 ERROR 的差异。

## 批量整理建议

- 57 条参数可改 const 指针、42 条疑似无用 include、25 条 push_back→emplace_back、
  21 条 constexpr、21 条 ranges 算法建议：可分批整理，注意公开回调签名兼容性和直接包含原则。
- 31 条 base→derived static_cast 提示：VM/容器中许多由 TypeId/opcode 保护，遵循 AGENTS.md
  的热路径规则，不机械改成 dynamic_cast。
- 21 条 AstFormatter visitor 可见性提示：private override 本身合法，优先处理旧 API 失效，
  不必为了可见性风格提示把 visitor 全部公开。
- 9 条 class/struct 前置声明不一致：集中在指令宏体系，可统一声明方式；不等同于 9 个运行时错误。
- 4 条从未使用的局部变量：TjsTranslator.cpp:584、Class.cpp:161、Function.cpp:51、
  tests/ssa_test.cpp:428。Class.cpp 的循环当前只是重复执行同一个 assert，属于可清理的调试残留。
- 测试中的 `moved = moved` 是自赋值覆盖，`assigned` 是句柄赋值/析构生命周期覆盖，不应按生产死代码删掉。
- 3 条“异常不继承 std::exception”涉及内部异常协议，需要检查捕获路径后决定，不扩大为统一替换规则。

## 模块统计

下表保留含源码或提示的模块；INFO 单列以免与警告混淆。

| 模块 | 文件数 | ERROR | WARN | 弱警告 | INFO |
|---|---:|---:|---:|---:|---:|
| bench | 2 | 0 | 0 | 2 | 0 |
| fuzz | 6 | 0 | 2 | 0 | 0 |
| src/Binding.hpp | 1 | 0 | 0 | 0 | 3 |
| src/cli | 10 | 0 | 10 | 0 | 46 |
| src/common | 13 | 0 | 2 | 0 | 31 |
| src/config.h | 1 | 0 | 0 | 0 | 2 |
| src/gc | 2 | 0 | 2 | 0 | 5 |
| src/gen | 13 | 0 | 66 | 0 | 175 |
| src/logging | 2 | 0 | 1 | 0 | 4 |
| src/main.cpp | 1 | 0 | 0 | 0 | 2 |
| src/parser | 14 | 2 | 31 | 0 | 85 |
| src/runtime | 6 | 0 | 0 | 0 | 23 |
| src/stdlib | 28 | 0 | 149 | 0 | 534 |
| src/tjsbc | 4 | 0 | 25 | 0 | 106 |
| src/types | 26 | 1 | 16 | 0 | 135 |
| src/vm | 17 | 0 | 25 | 0 | 147 |
| tests | 38 | 0 | 29 | 1 | 167 |
| tools | 2 | 0 | 0 | 7 | 0 |

## ERROR/WARN/弱警告完整列表

以下是原始编辑器提示，不表示每条都已确认是缺陷。INFO 详见 JSON。

| 文件与行号 | 级别 | 原始描述 |
|---|---|---|
| [bench/test_bytecode_conformance.py:66](../bench/test_bytecode_conformance.py#L66) | WEAK WARNING | Parameter 'cwd' value is not used |
| [bench/test_bytecode_conformance.py:66](../bench/test_bytecode_conformance.py#L66) | WEAK WARNING | Parameter 'timeout' value is not used |
| [fuzz/fuzz_lexer.cc:1](../fuzz/fuzz_lexer.cc#L1) | WARN | Possibly unused #include directive |
| [fuzz/fuzz_parser.cc:1](../fuzz/fuzz_parser.cc#L1) | WARN | Possibly unused #include directive |
| [src/cli/Application.cpp:1](../src/cli/Application.cpp#L1) | WARN | Clang-Tidy: Declaration uses identifier '_CRT_SECURE_NO_WARNINGS', which is a reserved identifier |
| [src/cli/Application.cpp:7](../src/cli/Application.cpp#L7) | WARN | Possibly unused #include directive |
| [src/cli/Application.cpp:66](../src/cli/Application.cpp#L66) | WARN | Local variable 'compilation' might not be initialized |
| [src/cli/Compilation.cpp:14](../src/cli/Compilation.cpp#L14) | WARN | Parameter 'context' can be made reference to const |
| [src/cli/Compilation.cpp:20](../src/cli/Compilation.cpp#L20) | WARN | Local variable 'compilation' might not be initialized |
| [src/cli/Compilation.cpp:35](../src/cli/Compilation.cpp#L35) | WARN | Local variable 'compilation' might not be initialized |
| [src/cli/Compilation.cpp:64](../src/cli/Compilation.cpp#L64) | WARN | Parameter 'runtime' can be made reference to const |
| [src/cli/Compilation.hpp:4](../src/cli/Compilation.hpp#L4) | WARN | Possibly unused #include directive |
| [src/cli/Diagnostics.cpp:43](../src/cli/Diagnostics.cpp#L43) | WARN | Clang-Tidy: Escaped string literal can be written as a raw string literal |
| [src/cli/Diagnostics.cpp:50](../src/cli/Diagnostics.cpp#L50) | WARN | Clang-Tidy: Escaped string literal can be written as a raw string literal |
| [src/common/Colorizer.hpp:15](../src/common/Colorizer.hpp#L15) | WARN | Possibly unused #include directive |
| [src/common/Ret.hpp:90](../src/common/Ret.hpp#L90) | WARN | Clang-Tidy: Throw expression should throw anonymous temporary values instead |
| [src/gc/gc.hpp:34](../src/gc/gc.hpp#L34) | WARN | Clang-Tidy: Function 'needsFinalization' should be marked [[nodiscard]] |
| [src/gc/gc.hpp:35](../src/gc/gc.hpp#L35) | WARN | Clang-Tidy: Function 'canFinalizeIn' should be marked [[nodiscard]] |
| [src/gen/ControlFlowGraph.cpp:76](../src/gen/ControlFlowGraph.cpp#L76) | WARN | Clang-Tidy: Use auto when initializing with a template cast to avoid duplicating the type name |
| [src/gen/ControlFlowGraph.cpp:97](../src/gen/ControlFlowGraph.cpp#L97) | WARN | Variable 'invalidBlock' can be made constexpr |
| [src/gen/ControlFlowGraph.hpp:6](../src/gen/ControlFlowGraph.hpp#L6) | WARN | Possibly unused #include directive |
| [src/gen/ControlFlowGraph.hpp:8](../src/gen/ControlFlowGraph.hpp#L8) | WARN | Possibly unused #include directive |
| [src/gen/ControlFlowGraph.hpp:43](../src/gen/ControlFlowGraph.hpp#L43) | WARN | Clang-Tidy: Function 'getBasicBlocks' should be marked [[nodiscard]] |
| [src/gen/ControlFlowGraph.hpp:46](../src/gen/ControlFlowGraph.hpp#L46) | WARN | Clang-Tidy: Function 'getLoops' should be marked [[nodiscard]] |
| [src/gen/IRGenerator.cpp:17](../src/gen/IRGenerator.cpp#L17) | WARN | Possibly unused #include directive |
| [src/gen/IRGenerator.cpp:677](../src/gen/IRGenerator.cpp#L677) | WARN | Call to 'any_of' can be replaced with a call to 'ranges::any_of' |
| [src/gen/IRGenerator.cpp:776](../src/gen/IRGenerator.cpp#L776) | WARN | Member function can be made const |
| [src/gen/IRGenerator.cpp:780](../src/gen/IRGenerator.cpp#L780) | WARN | Member function can be made const |
| [src/gen/IRGenerator.cpp:1639](../src/gen/IRGenerator.cpp#L1639) | WARN | Member function can be made const |
| [src/gen/IRGenerator.hpp:148](../src/gen/IRGenerator.hpp#L148) | WARN | Clang-Tidy: Parameter 'identifier' is const-qualified in the function declaration; const-qualification of parameters only has an effect in function definitions |
| [src/gen/Instruction.cpp:19](../src/gen/Instruction.cpp#L19) | WARN | Possibly unused #include directive |
| [src/gen/Instruction.cpp:21](../src/gen/Instruction.cpp#L21) | WARN | Possibly unused #include directive |
| [src/gen/Instruction.cpp:22](../src/gen/Instruction.cpp#L22) | WARN | Possibly unused #include directive |
| [src/gen/Instruction.cpp:24](../src/gen/Instruction.cpp#L24) | WARN | Possibly unused #include directive |
| [src/gen/Instruction.cpp:101](../src/gen/Instruction.cpp#L101) | WARN | Clang-Tidy: 'operator+=' called with a string literal consisting of a single character; consider using the more efficient overload accepting a character |
| [src/gen/Instruction.hpp:217](../src/gen/Instruction.hpp#L217) | WARN | DumpInst declared as a struct here but previously declared as a class |
| [src/gen/Instruction.hpp:396](../src/gen/Instruction.hpp#L396) | WARN | CallV defined as a struct here but previously declared as a class |
| [src/gen/Instruction.hpp:412](../src/gen/Instruction.hpp#L412) | WARN | CompareTest defined as a struct here but previously declared as a class |
| [src/gen/Instruction.hpp:419](../src/gen/Instruction.hpp#L419) | WARN | Clang-Tidy: Function 'opcode' should be marked [[nodiscard]] |
| [src/gen/Instruction.hpp:425](../src/gen/Instruction.hpp#L425) | WARN | GMethodC defined as a struct here but previously declared as a class |
| [src/gen/Instruction.hpp:432](../src/gen/Instruction.hpp#L432) | WARN | Clang-Tidy: Function 'opcode' should be marked [[nodiscard]] |
| [src/gen/Instruction.hpp:436](../src/gen/Instruction.hpp#L436) | WARN | CallMethodV defined as a struct here but previously declared as a class |
| [src/gen/Instruction.hpp:440](../src/gen/Instruction.hpp#L440) | WARN | Clang-Tidy: Function 'opcode' should be marked [[nodiscard]] |
| [src/gen/Instruction.hpp:446](../src/gen/Instruction.hpp#L446) | WARN | CallOmit defined as a struct here but previously declared as a class |
| [src/gen/Instruction.hpp:463](../src/gen/Instruction.hpp#L463) | WARN | CallExpand defined as a struct here but previously declared as a class |
| [src/gen/Instruction.hpp:483](../src/gen/Instruction.hpp#L483) | WARN | CollapseArgs defined as a struct here but previously declared as a class |
| [src/gen/Instruction.hpp:518](../src/gen/Instruction.hpp#L518) | WARN | DumpInst defined as a struct here but previously declared as a class |
| [src/gen/Optimizer.cpp:92](../src/gen/Optimizer.cpp#L92) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/gen/Optimizer.cpp:95](../src/gen/Optimizer.cpp#L95) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/gen/Optimizer.cpp:98](../src/gen/Optimizer.cpp#L98) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/gen/Optimizer.cpp:106](../src/gen/Optimizer.cpp#L106) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/gen/Optimizer.cpp:112](../src/gen/Optimizer.cpp#L112) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/gen/Optimizer.cpp:115](../src/gen/Optimizer.cpp#L115) | WARN | views::values can be used |
| [src/gen/Optimizer.cpp:120](../src/gen/Optimizer.cpp#L120) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/gen/Optimizer.cpp:373](../src/gen/Optimizer.cpp#L373) | WARN | Call to 'fill' can be replaced with a call to 'ranges::fill' |
| [src/gen/Optimizer.cpp:439](../src/gen/Optimizer.cpp#L439) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/gen/Optimizer.cpp:455](../src/gen/Optimizer.cpp#L455) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/gen/Optimizer.cpp:476](../src/gen/Optimizer.cpp#L476) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/gen/Optimizer.cpp:543](../src/gen/Optimizer.cpp#L543) | WARN | Call to 'fill' can be replaced with a call to 'ranges::fill' |
| [src/gen/Optimizer.cpp:555](../src/gen/Optimizer.cpp#L555) | WARN | Clang-Tidy: Use auto when initializing with a template cast to avoid duplicating the type name |
| [src/gen/Optimizer.cpp:556](../src/gen/Optimizer.cpp#L556) | WARN | Clang-Tidy: Use auto when initializing with a template cast to avoid duplicating the type name |
| [src/gen/Optimizer.cpp:565](../src/gen/Optimizer.cpp#L565) | WARN | Clang-Tidy: Use auto when initializing with a template cast to avoid duplicating the type name |
| [src/gen/Optimizer.cpp:566](../src/gen/Optimizer.cpp#L566) | WARN | Clang-Tidy: Use auto when initializing with a template cast to avoid duplicating the type name |
| [src/gen/Optimizer.cpp:567](../src/gen/Optimizer.cpp#L567) | WARN | Clang-Tidy: Use auto when initializing with a template cast to avoid duplicating the type name |
| [src/gen/Optimizer.cpp:627](../src/gen/Optimizer.cpp#L627) | WARN | Clang-Tidy: Uninitialized record type: 'destination' |
| [src/gen/Optimizer.cpp:651](../src/gen/Optimizer.cpp#L651) | WARN | Call to 'fill' can be replaced with a call to 'ranges::fill' |
| [src/gen/Optimizer.cpp:660](../src/gen/Optimizer.cpp#L660) | WARN | Call to 'fill' can be replaced with a call to 'ranges::fill' |
| [src/gen/Optimizer.cpp:687](../src/gen/Optimizer.cpp#L687) | WARN | Clang-Tidy: Uninitialized record type: 'destination' |
| [src/gen/Optimizer.hpp:10](../src/gen/Optimizer.hpp#L10) | WARN | Possibly unused #include directive |
| [src/gen/Optimizer.hpp:78](../src/gen/Optimizer.hpp#L78) | WARN | Clang-Tidy: Function 'name' should be marked [[nodiscard]] |
| [src/gen/SSA.cpp:42](../src/gen/SSA.cpp#L42) | WARN | Functional-style cast is used instead of a C++ cast |
| [src/gen/SSA.cpp:42](../src/gen/SSA.cpp#L42) | WARN | Functional-style cast is used instead of a C++ cast |
| [src/gen/SSA.cpp:291](../src/gen/SSA.cpp#L291) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/gen/SSA.cpp:299](../src/gen/SSA.cpp#L299) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/gen/SSA.cpp:308](../src/gen/SSA.cpp#L308) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/gen/SSA.cpp:403](../src/gen/SSA.cpp#L403) | WARN | Clang-Tidy: Use emplace_back instead of push_back |
| [src/gen/SSA.cpp:411](../src/gen/SSA.cpp#L411) | WARN | Clang-Tidy: Use emplace_back instead of push_back |
| [src/gen/SSA.cpp:498](../src/gen/SSA.cpp#L498) | WARN | Clang-Tidy: Use emplace_back instead of push_back |
| [src/gen/SSA.cpp:637](../src/gen/SSA.cpp#L637) | WARN | Clang-Tidy: Use emplace_back instead of push_back |
| [src/gen/SSA.cpp:655](../src/gen/SSA.cpp#L655) | WARN | Clang-Tidy: Use emplace_back instead of push_back |
| [src/gen/SSA.cpp:699](../src/gen/SSA.cpp#L699) | WARN | Clang-Tidy: Use range-based for loop instead |
| [src/gen/SSA.cpp:709](../src/gen/SSA.cpp#L709) | WARN | Variable 'empty' can be made constexpr |
| [src/gen/SSA.cpp:812](../src/gen/SSA.cpp#L812) | WARN | Clang-Tidy: 'operator+=' called with a string literal consisting of a single character; consider using the more efficient overload accepting a character |
| [src/gen/SSA.cpp:855](../src/gen/SSA.cpp#L855) | WARN | Clang-Tidy: 'operator+=' called with a string literal consisting of a single character; consider using the more efficient overload accepting a character |
| [src/logging/Logger.cpp:64](../src/logging/Logger.cpp#L64) | WARN | Function 'cial::vformat_to_string' can be made static or moved into an anonymous namespace to enforce internal linkage |
| [src/parser/Lexer.cpp:560](../src/parser/Lexer.cpp#L560) | WARN | Clang-Tidy: Switch has 2 consecutive identical branches |
| [src/parser/Lexer.cpp:781](../src/parser/Lexer.cpp#L781) | WARN | Clang-Tidy: Use auto when initializing with a cast to avoid duplicating the type name |
| [src/parser/Lexer.cpp:1011](../src/parser/Lexer.cpp#L1011) | WARN | Clang-Tidy: Repeated branch body in conditional chain |
| [src/parser/Lexer.hpp:20](../src/parser/Lexer.hpp#L20) | WARN | Possibly unused #include directive |
| [src/parser/PreProcessor.cpp:18](../src/parser/PreProcessor.cpp#L18) | WARN | Possibly unused #include directive |
| [src/parser/ast/AstFormatter.hpp:21](../src/parser/ast/AstFormatter.hpp#L21) | WARN | Possibly unused #include directive |
| [src/parser/ast/AstFormatter.hpp:25](../src/parser/ast/AstFormatter.hpp#L25) | WARN | Clangd: Abstract class is marked 'final' |
| [src/parser/ast/AstFormatter.hpp:107](../src/parser/ast/AstFormatter.hpp#L107) | WARN | Visibility of function cial::AstFormatter::visit changes from public in base struct cial::syntax::AstNode::Visitor to private |
| [src/parser/ast/AstFormatter.hpp:112](../src/parser/ast/AstFormatter.hpp#L112) | WARN | Visibility of function cial::AstFormatter::visit changes from public in base struct cial::syntax::AstNode::Visitor to private |
| [src/parser/ast/AstFormatter.hpp:114](../src/parser/ast/AstFormatter.hpp#L114) | WARN | Visibility of function cial::AstFormatter::visit changes from public in base struct cial::syntax::AstNode::Visitor to private |
| [src/parser/ast/AstFormatter.hpp:118](../src/parser/ast/AstFormatter.hpp#L118) | WARN | Visibility of function cial::AstFormatter::visit changes from public in base struct cial::syntax::AstNode::Visitor to private |
| [src/parser/ast/AstFormatter.hpp:132](../src/parser/ast/AstFormatter.hpp#L132) | WARN | Visibility of function cial::AstFormatter::visit changes from public in base struct cial::syntax::AstNode::Visitor to private |
| [src/parser/ast/AstFormatter.hpp:166](../src/parser/ast/AstFormatter.hpp#L166) | WARN | Visibility of function cial::AstFormatter::visit changes from public in base struct cial::syntax::AstNode::Visitor to private |
| [src/parser/ast/AstFormatter.hpp:172](../src/parser/ast/AstFormatter.hpp#L172) | WARN | Cannot resolve symbol 'body' |
| [src/parser/ast/AstFormatter.hpp:177](../src/parser/ast/AstFormatter.hpp#L177) | WARN | Visibility of function cial::AstFormatter::visit changes from public in base struct cial::syntax::AstNode::Visitor to private |
| [src/parser/ast/AstFormatter.hpp:182](../src/parser/ast/AstFormatter.hpp#L182) | WARN | Cannot use '->' to access member of a class which does not overload 'operator ->' |
| [src/parser/ast/AstFormatter.hpp:188](../src/parser/ast/AstFormatter.hpp#L188) | ERROR | Clangd: Class member cannot be redeclared |
| [src/parser/ast/AstFormatter.hpp:188](../src/parser/ast/AstFormatter.hpp#L188) | WARN | Visibility of function cial::AstFormatter::visit changes from public in base struct cial::syntax::AstNode::Visitor to private |
| [src/parser/ast/AstFormatter.hpp:188](../src/parser/ast/AstFormatter.hpp#L188) | WARN | Cannot resolve symbol 'UnaryExprNode' |
| [src/parser/ast/AstFormatter.hpp:192](../src/parser/ast/AstFormatter.hpp#L192) | ERROR | Clangd: Member reference type 'const Token' is not a pointer; did you mean to use '.'? (fix available) |
| [src/parser/ast/AstFormatter.hpp:198](../src/parser/ast/AstFormatter.hpp#L198) | WARN | Visibility of function cial::AstFormatter::visit changes from public in base struct cial::syntax::AstNode::Visitor to private |
| [src/parser/ast/AstFormatter.hpp:213](../src/parser/ast/AstFormatter.hpp#L213) | WARN | Visibility of function cial::AstFormatter::visit changes from public in base struct cial::syntax::AstNode::Visitor to private |
| [src/parser/ast/AstFormatter.hpp:224](../src/parser/ast/AstFormatter.hpp#L224) | WARN | Visibility of function cial::AstFormatter::visit changes from public in base struct cial::syntax::AstNode::Visitor to private |
| [src/parser/ast/AstFormatter.hpp:237](../src/parser/ast/AstFormatter.hpp#L237) | WARN | Visibility of function cial::AstFormatter::visit changes from public in base struct cial::syntax::AstNode::Visitor to private |
| [src/parser/ast/AstFormatter.hpp:239](../src/parser/ast/AstFormatter.hpp#L239) | WARN | Visibility of function cial::AstFormatter::visit changes from public in base struct cial::syntax::AstNode::Visitor to private |
| [src/parser/ast/AstFormatter.hpp:257](../src/parser/ast/AstFormatter.hpp#L257) | WARN | Visibility of function cial::AstFormatter::visit changes from public in base struct cial::syntax::AstNode::Visitor to private |
| [src/parser/ast/AstFormatter.hpp:270](../src/parser/ast/AstFormatter.hpp#L270) | WARN | Visibility of function cial::AstFormatter::visit changes from public in base struct cial::syntax::AstNode::Visitor to private |
| [src/parser/ast/AstFormatter.hpp:295](../src/parser/ast/AstFormatter.hpp#L295) | WARN | Visibility of function cial::AstFormatter::visit changes from public in base struct cial::syntax::AstNode::Visitor to private |
| [src/parser/ast/AstFormatter.hpp:308](../src/parser/ast/AstFormatter.hpp#L308) | WARN | Visibility of function cial::AstFormatter::visit changes from public in base struct cial::syntax::AstNode::Visitor to private |
| [src/parser/ast/AstFormatter.hpp:310](../src/parser/ast/AstFormatter.hpp#L310) | WARN | Visibility of function cial::AstFormatter::visit changes from public in base struct cial::syntax::AstNode::Visitor to private |
| [src/parser/ast/AstFormatter.hpp:312](../src/parser/ast/AstFormatter.hpp#L312) | WARN | Visibility of function cial::AstFormatter::visit changes from public in base struct cial::syntax::AstNode::Visitor to private |
| [src/parser/ast/AstFormatter.hpp:317](../src/parser/ast/AstFormatter.hpp#L317) | WARN | Visibility of function cial::AstFormatter::visit changes from public in base struct cial::syntax::AstNode::Visitor to private |
| [src/parser/ast/AstFormatter.hpp:327](../src/parser/ast/AstFormatter.hpp#L327) | WARN | Visibility of function cial::AstFormatter::visit changes from public in base struct cial::syntax::AstNode::Visitor to private |
| [src/stdlib/ArrayLib.cpp:42](../src/stdlib/ArrayLib.cpp#L42) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/stdlib/ArrayLib.cpp:62](../src/stdlib/ArrayLib.cpp#L62) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/ArrayLib.cpp:78](../src/stdlib/ArrayLib.cpp#L78) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/ArrayLib.cpp:94](../src/stdlib/ArrayLib.cpp#L94) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/ArrayLib.cpp:107](../src/stdlib/ArrayLib.cpp#L107) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/ArrayLib.cpp:125](../src/stdlib/ArrayLib.cpp#L125) | WARN | Call to 'reverse' can be replaced with a call to 'ranges::reverse' |
| [src/stdlib/ArrayLib.cpp:128](../src/stdlib/ArrayLib.cpp#L128) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/ArrayLib.cpp:144](../src/stdlib/ArrayLib.cpp#L144) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/ArrayLib.cpp:218](../src/stdlib/ArrayLib.cpp#L218) | WARN | Default case is not handled in the switch statement |
| [src/stdlib/ArrayLib.cpp:218](../src/stdlib/ArrayLib.cpp#L218) | WARN | Clang-Tidy: Potential uncovered code path; add a default label |
| [src/stdlib/ArrayLib.cpp:220](../src/stdlib/ArrayLib.cpp#L220) | WARN | Call to 'stable_sort' can be replaced with a call to 'ranges::stable_sort' |
| [src/stdlib/ArrayLib.cpp:221](../src/stdlib/ArrayLib.cpp#L221) | WARN | Call to 'sort' can be replaced with a call to 'ranges::sort' |
| [src/stdlib/ArrayLib.cpp:224](../src/stdlib/ArrayLib.cpp#L224) | WARN | Call to 'stable_sort' can be replaced with a call to 'ranges::stable_sort' |
| [src/stdlib/ArrayLib.cpp:225](../src/stdlib/ArrayLib.cpp#L225) | WARN | Call to 'sort' can be replaced with a call to 'ranges::sort' |
| [src/stdlib/ArrayLib.cpp:228](../src/stdlib/ArrayLib.cpp#L228) | WARN | Call to 'stable_sort' can be replaced with a call to 'ranges::stable_sort' |
| [src/stdlib/ArrayLib.cpp:229](../src/stdlib/ArrayLib.cpp#L229) | WARN | Call to 'sort' can be replaced with a call to 'ranges::sort' |
| [src/stdlib/ArrayLib.cpp:232](../src/stdlib/ArrayLib.cpp#L232) | WARN | Call to 'stable_sort' can be replaced with a call to 'ranges::stable_sort' |
| [src/stdlib/ArrayLib.cpp:233](../src/stdlib/ArrayLib.cpp#L233) | WARN | Call to 'sort' can be replaced with a call to 'ranges::sort' |
| [src/stdlib/ArrayLib.cpp:236](../src/stdlib/ArrayLib.cpp#L236) | WARN | Call to 'stable_sort' can be replaced with a call to 'ranges::stable_sort' |
| [src/stdlib/ArrayLib.cpp:237](../src/stdlib/ArrayLib.cpp#L237) | WARN | Call to 'sort' can be replaced with a call to 'ranges::sort' |
| [src/stdlib/ArrayLib.cpp:240](../src/stdlib/ArrayLib.cpp#L240) | WARN | Call to 'stable_sort' can be replaced with a call to 'ranges::stable_sort' |
| [src/stdlib/ArrayLib.cpp:241](../src/stdlib/ArrayLib.cpp#L241) | WARN | Call to 'sort' can be replaced with a call to 'ranges::sort' |
| [src/stdlib/ArrayLib.cpp:244](../src/stdlib/ArrayLib.cpp#L244) | WARN | Call to 'stable_sort' can be replaced with a call to 'ranges::stable_sort' |
| [src/stdlib/ArrayLib.cpp:245](../src/stdlib/ArrayLib.cpp#L245) | WARN | Call to 'sort' can be replaced with a call to 'ranges::sort' |
| [src/stdlib/DateLib.cpp:4](../src/stdlib/DateLib.cpp#L4) | WARN | Clang-Tidy: Declaration uses identifier '_CRT_SECURE_NO_WARNINGS', which is a reserved identifier |
| [src/stdlib/DateLib.cpp:9](../src/stdlib/DateLib.cpp#L9) | WARN | Possibly unused #include directive |
| [src/stdlib/DateLib.cpp:117](../src/stdlib/DateLib.cpp#L117) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/DateLib.cpp:124](../src/stdlib/DateLib.cpp#L124) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/DateLib.cpp:131](../src/stdlib/DateLib.cpp#L131) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/DateLib.cpp:138](../src/stdlib/DateLib.cpp#L138) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/DateLib.cpp:145](../src/stdlib/DateLib.cpp#L145) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/DateLib.cpp:152](../src/stdlib/DateLib.cpp#L152) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/DateLib.cpp:159](../src/stdlib/DateLib.cpp#L159) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/DateLib.cpp:166](../src/stdlib/DateLib.cpp#L166) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/DateParser.cpp:4](../src/stdlib/DateParser.cpp#L4) | WARN | Clang-Tidy: Declaration uses identifier '_CRT_SECURE_NO_WARNINGS', which is a reserved identifier |
| [src/stdlib/DateParser.cpp:8](../src/stdlib/DateParser.cpp#L8) | WARN | Possibly unused #include directive |
| [src/stdlib/DateParser.cpp:12](../src/stdlib/DateParser.cpp#L12) | WARN | Clang-Tidy: Inclusion of deprecated C++ header 'time.h'; consider using 'ctime' instead |
| [src/stdlib/DateParser.cpp:165](../src/stdlib/DateParser.cpp#L165) | WARN | Clang-Tidy: Method 'matchWord' can be made static |
| [src/stdlib/DateParser.cpp:165](../src/stdlib/DateParser.cpp#L165) | WARN | Member function can be made static |
| [src/stdlib/DateParser.hpp:6](../src/stdlib/DateParser.hpp#L6) | WARN | Possibly unused #include directive |
| [src/stdlib/DictLib.cpp:27](../src/stdlib/DictLib.cpp#L27) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/stdlib/DictLib.cpp:31](../src/stdlib/DictLib.cpp#L31) | WARN | Parameter 'vm' can be made pointer to const |
| [src/stdlib/DictLib.cpp:41](../src/stdlib/DictLib.cpp#L41) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/DictLib.cpp:53](../src/stdlib/DictLib.cpp#L53) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/ExceptionLib.hpp:7](../src/stdlib/ExceptionLib.hpp#L7) | WARN | Possibly unused #include directive |
| [src/stdlib/ExceptionLib.hpp:14](../src/stdlib/ExceptionLib.hpp#L14) | WARN | Parameter 'vm' can be made reference to const |
| [src/stdlib/MathLib.cpp:4](../src/stdlib/MathLib.cpp#L4) | WARN | Clang-Tidy: Declaration uses identifier '_USE_MATH_DEFINES', which is a reserved identifier |
| [src/stdlib/MathLib.cpp:12](../src/stdlib/MathLib.cpp#L12) | WARN | Possibly unused #include directive |
| [src/stdlib/MathLib.cpp:13](../src/stdlib/MathLib.cpp#L13) | WARN | Possibly unused #include directive |
| [src/stdlib/MathLib.cpp:92](../src/stdlib/MathLib.cpp#L92) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/MathLib.cpp:98](../src/stdlib/MathLib.cpp#L98) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/MathLib.cpp:104](../src/stdlib/MathLib.cpp#L104) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/MathLib.cpp:110](../src/stdlib/MathLib.cpp#L110) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/MathLib.cpp:116](../src/stdlib/MathLib.cpp#L116) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/MathLib.cpp:122](../src/stdlib/MathLib.cpp#L122) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/MathLib.cpp:128](../src/stdlib/MathLib.cpp#L128) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/MathLib.cpp:134](../src/stdlib/MathLib.cpp#L134) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/MathLib.cpp:140](../src/stdlib/MathLib.cpp#L140) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/MathLib.cpp:146](../src/stdlib/MathLib.cpp#L146) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/MathLib.cpp:153](../src/stdlib/MathLib.cpp#L153) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/MathLib.cpp:169](../src/stdlib/MathLib.cpp#L169) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/MathLib.cpp:188](../src/stdlib/MathLib.cpp#L188) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/MathLib.cpp:194](../src/stdlib/MathLib.cpp#L194) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/MathLib.cpp:200](../src/stdlib/MathLib.cpp#L200) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/MathLib.cpp:206](../src/stdlib/MathLib.cpp#L206) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/MathLib.cpp:212](../src/stdlib/MathLib.cpp#L212) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/RandomLib.cpp:7](../src/stdlib/RandomLib.cpp#L7) | WARN | Possibly unused #include directive |
| [src/stdlib/RandomLib.cpp:8](../src/stdlib/RandomLib.cpp#L8) | WARN | Possibly unused #include directive |
| [src/stdlib/RandomLib.cpp:131](../src/stdlib/RandomLib.cpp#L131) | WARN | Member function can be made const |
| [src/stdlib/RandomLib.cpp:218](../src/stdlib/RandomLib.cpp#L218) | WARN | Parameter 'vm' can be made pointer to const |
| [src/stdlib/RandomLib.cpp:218](../src/stdlib/RandomLib.cpp#L218) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/RegExpLib.cpp:26](../src/stdlib/RegExpLib.cpp#L26) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/stdlib/RegExpLib.cpp:81](../src/stdlib/RegExpLib.cpp#L81) | WARN | Call to 'find' can be replaced with a call to 'ranges::find' |
| [src/stdlib/RegExpLib.cpp:105](../src/stdlib/RegExpLib.cpp#L105) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/RegExpLib.cpp:116](../src/stdlib/RegExpLib.cpp#L116) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/RegExpLib.cpp:125](../src/stdlib/RegExpLib.cpp#L125) | WARN | Clang-Tidy: Use range-based for loop instead |
| [src/stdlib/RegExpLib.cpp:131](../src/stdlib/RegExpLib.cpp#L131) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/RegExpLib.cpp:142](../src/stdlib/RegExpLib.cpp#L142) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/RegExpLib.cpp:152](../src/stdlib/RegExpLib.cpp#L152) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/RegExpLib.cpp:159](../src/stdlib/RegExpLib.cpp#L159) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/RegExpLib.cpp:192](../src/stdlib/RegExpLib.cpp#L192) | WARN | Clang-Tidy: Use auto when initializing with a cast to avoid duplicating the type name |
| [src/stdlib/RegExpLib.cpp:197](../src/stdlib/RegExpLib.cpp#L197) | WARN | Clang-Tidy: Use range-based for loop instead |
| [src/stdlib/RegExpLib.cpp:214](../src/stdlib/RegExpLib.cpp#L214) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/RegExpLib.cpp:224](../src/stdlib/RegExpLib.cpp#L224) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/ScriptsLib.cpp:66](../src/stdlib/ScriptsLib.cpp#L66) | WARN | Clang-Tidy: Constness of 'raw' prevents automatic move |
| [src/stdlib/ScriptsLib.cpp:79](../src/stdlib/ScriptsLib.cpp#L79) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/stdlib/ScriptsLib.cpp:82](../src/stdlib/ScriptsLib.cpp#L82) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/ScriptsLib.cpp:131](../src/stdlib/ScriptsLib.cpp#L131) | WARN | Clang-Tidy: Throwing an exception whose type 'vm::ScriptExceptionCarrier' is not derived from 'std::exception' |
| [src/stdlib/ScriptsLib.cpp:208](../src/stdlib/ScriptsLib.cpp#L208) | WARN | Clang-Tidy: Constness of 'p' prevents automatic move |
| [src/stdlib/ScriptsLib.cpp:236](../src/stdlib/ScriptsLib.cpp#L236) | WARN | Clang-Tidy: The parameter 'thisVal' of type 'Value' is copied for each invocation but only used as a const reference; consider making it a const reference |
| [src/stdlib/ScriptsLib.cpp:236](../src/stdlib/ScriptsLib.cpp#L236) | WARN | Parameter 'thisVal' is copied for each invocation, consider passing by const reference |
| [src/stdlib/ScriptsLib.cpp:248](../src/stdlib/ScriptsLib.cpp#L248) | WARN | Clang-Tidy: The parameter 'thisVal' of type 'Value' is copied for each invocation but only used as a const reference; consider making it a const reference |
| [src/stdlib/ScriptsLib.cpp:248](../src/stdlib/ScriptsLib.cpp#L248) | WARN | Parameter 'thisVal' is copied for each invocation, consider passing by const reference |
| [src/stdlib/SerializeLib.cpp:10](../src/stdlib/SerializeLib.cpp#L10) | WARN | Clang-Tidy: Declaration uses identifier '_CRT_SECURE_NO_WARNINGS', which is a reserved identifier |
| [src/stdlib/SerializeLib.cpp:17](../src/stdlib/SerializeLib.cpp#L17) | WARN | Possibly unused #include directive |
| [src/stdlib/SerializeLib.cpp:18](../src/stdlib/SerializeLib.cpp#L18) | WARN | Possibly unused #include directive |
| [src/stdlib/SerializeLib.cpp:94](../src/stdlib/SerializeLib.cpp#L94) | WARN | Clang-Tidy: Use auto when initializing with a cast to avoid duplicating the type name |
| [src/stdlib/SerializeLib.cpp:165](../src/stdlib/SerializeLib.cpp#L165) | WARN | Clang-Tidy: Constness of 'raw' prevents automatic move |
| [src/stdlib/SerializeLib.cpp:382](../src/stdlib/SerializeLib.cpp#L382) | WARN | Clang-Tidy: Use auto when initializing with a cast to avoid duplicating the type name |
| [src/stdlib/SerializeLib.cpp:398](../src/stdlib/SerializeLib.cpp#L398) | WARN | Clang-Tidy: Repeated branch body in conditional chain |
| [src/stdlib/SerializeLib.cpp:477](../src/stdlib/SerializeLib.cpp#L477) | WARN | Clang-Tidy: Replace loop by 'std::ranges::any_of()' |
| [src/stdlib/SerializeLib.cpp:501](../src/stdlib/SerializeLib.cpp#L501) | WARN | Clang-Tidy: 'operator+=' called with a string literal consisting of a single character; consider using the more efficient overload accepting a character |
| [src/stdlib/SerializeLib.cpp:527](../src/stdlib/SerializeLib.cpp#L527) | WARN | Clang-Tidy: 'operator+=' called with a string literal consisting of a single character; consider using the more efficient overload accepting a character |
| [src/stdlib/SerializeLib.cpp:595](../src/stdlib/SerializeLib.cpp#L595) | WARN | Clang-Tidy: Constructor does not initialize these fields: data, size |
| [src/stdlib/SerializeLib.cpp:679](../src/stdlib/SerializeLib.cpp#L679) | WARN | Clang-Tidy: Use auto when initializing with a template cast to avoid duplicating the type name |
| [src/stdlib/SerializeLib.cpp:684](../src/stdlib/SerializeLib.cpp#L684) | WARN | Clang-Tidy: Use auto when initializing with a template cast to avoid duplicating the type name |
| [src/stdlib/SerializeLib.cpp:741](../src/stdlib/SerializeLib.cpp#L741) | WARN | Variable 'guard' can be moved to inner scope |
| [src/stdlib/SerializeLib.cpp:756](../src/stdlib/SerializeLib.cpp#L756) | WARN | Variable 'guard' can be moved to inner scope |
| [src/stdlib/SerializeLib.cpp:825](../src/stdlib/SerializeLib.cpp#L825) | WARN | Clang-Tidy: Method 'fail' can be made static |
| [src/stdlib/SerializeLib.cpp:825](../src/stdlib/SerializeLib.cpp#L825) | WARN | Member function can be made static |
| [src/stdlib/SerializeLib.cpp:845](../src/stdlib/SerializeLib.cpp#L845) | WARN | Variable 'e' can be moved to switch-statement |
| [src/stdlib/SerializeLib.cpp:874](../src/stdlib/SerializeLib.cpp#L874) | WARN | Clang-Tidy: Method 'parseHexFloat' can be made static |
| [src/stdlib/SerializeLib.cpp:874](../src/stdlib/SerializeLib.cpp#L874) | WARN | Member function can be made static |
| [src/stdlib/SerializeLib.cpp:1358](../src/stdlib/SerializeLib.cpp#L1358) | WARN | Clang-Tidy: Use auto when initializing with a cast to avoid duplicating the type name |
| [src/stdlib/SerializeLib.cpp:1392](../src/stdlib/SerializeLib.cpp#L1392) | WARN | Clang-Tidy: Switch has 2 consecutive identical branches |
| [src/stdlib/SerializeLib.cpp:1530](../src/stdlib/SerializeLib.cpp#L1530) | WARN | Clang-Tidy: Use emplace_back instead of push_back |
| [src/stdlib/SerializeLib.cpp:1557](../src/stdlib/SerializeLib.cpp#L1557) | WARN | Clang-Tidy: Use emplace_back instead of push_back |
| [src/stdlib/SerializeLib.cpp:1567](../src/stdlib/SerializeLib.cpp#L1567) | WARN | Clang-Tidy: Use emplace_back instead of push_back |
| [src/stdlib/SerializeLib.cpp:1576](../src/stdlib/SerializeLib.cpp#L1576) | WARN | Clang-Tidy: Use emplace_back instead of push_back |
| [src/stdlib/SerializeLib.cpp:1585](../src/stdlib/SerializeLib.cpp#L1585) | WARN | Clang-Tidy: Use emplace_back instead of push_back |
| [src/stdlib/SerializeLib.cpp:1595](../src/stdlib/SerializeLib.cpp#L1595) | WARN | Clang-Tidy: Use emplace_back instead of push_back |
| [src/stdlib/SerializeLib.cpp:1635](../src/stdlib/SerializeLib.cpp#L1635) | WARN | Clang-Tidy: Use emplace_back instead of push_back |
| [src/stdlib/SerializeLib.cpp:1646](../src/stdlib/SerializeLib.cpp#L1646) | WARN | Clang-Tidy: Use emplace_back instead of push_back |
| [src/stdlib/SerializeLib.cpp:1656](../src/stdlib/SerializeLib.cpp#L1656) | WARN | Clang-Tidy: Use emplace_back instead of push_back |
| [src/stdlib/SerializeLib.cpp:1665](../src/stdlib/SerializeLib.cpp#L1665) | WARN | Clang-Tidy: Use emplace_back instead of push_back |
| [src/stdlib/SerializeLib.cpp:1674](../src/stdlib/SerializeLib.cpp#L1674) | WARN | Clang-Tidy: Use emplace_back instead of push_back |
| [src/stdlib/SerializeLib.cpp:1687](../src/stdlib/SerializeLib.cpp#L1687) | WARN | Clang-Tidy: Use emplace_back instead of push_back |
| [src/stdlib/SerializeLib.cpp:1697](../src/stdlib/SerializeLib.cpp#L1697) | WARN | Clang-Tidy: Use emplace_back instead of push_back |
| [src/stdlib/SerializeLib.cpp:1706](../src/stdlib/SerializeLib.cpp#L1706) | WARN | Clang-Tidy: Use emplace_back instead of push_back |
| [src/stdlib/SerializeLib.cpp:1739](../src/stdlib/SerializeLib.cpp#L1739) | WARN | Clang-Tidy: Use emplace_back instead of push_back |
| [src/stdlib/SerializeLib.cpp:1763](../src/stdlib/SerializeLib.cpp#L1763) | WARN | Clang-Tidy: Use emplace_back instead of push_back |
| [src/stdlib/SerializeLib.cpp:1768](../src/stdlib/SerializeLib.cpp#L1768) | WARN | Clang-Tidy: Use emplace_back instead of push_back |
| [src/stdlib/SerializeLib.cpp:1772](../src/stdlib/SerializeLib.cpp#L1772) | WARN | Clang-Tidy: Use emplace_back instead of push_back |
| [src/stdlib/SerializeLib.cpp:1817](../src/stdlib/SerializeLib.cpp#L1817) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/SerializeLib.cpp:1829](../src/stdlib/SerializeLib.cpp#L1829) | WARN | Clang-Tidy: Use emplace_back instead of push_back |
| [src/stdlib/SerializeLib.cpp:1840](../src/stdlib/SerializeLib.cpp#L1840) | WARN | Clang-Tidy: Use emplace_back instead of push_back |
| [src/stdlib/SerializeLib.cpp:1844](../src/stdlib/SerializeLib.cpp#L1844) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/SerializeLib.cpp:1858](../src/stdlib/SerializeLib.cpp#L1858) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/SerializeLib.cpp:1871](../src/stdlib/SerializeLib.cpp#L1871) | WARN | Parameter 'vm' can be made pointer to const |
| [src/stdlib/SerializeLib.cpp:1871](../src/stdlib/SerializeLib.cpp#L1871) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/SerializeLib.cpp:1890](../src/stdlib/SerializeLib.cpp#L1890) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/SerializeLib.cpp:1900](../src/stdlib/SerializeLib.cpp#L1900) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/SerializeLib.cpp:1924](../src/stdlib/SerializeLib.cpp#L1924) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/SerializeLib.cpp:1943](../src/stdlib/SerializeLib.cpp#L1943) | WARN | Parameter 'vm' can be made pointer to const |
| [src/stdlib/SerializeLib.cpp:1943](../src/stdlib/SerializeLib.cpp#L1943) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/SerializeLib.cpp:1962](../src/stdlib/SerializeLib.cpp#L1962) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/SerializeLib.cpp:1978](../src/stdlib/SerializeLib.cpp#L1978) | WARN | Parameter 'args' can be made pointer to const |
| [src/stdlib/SerializeLib.hpp:7](../src/stdlib/SerializeLib.hpp#L7) | WARN | Possibly unused #include directive |
| [src/stdlib/StringLib.hpp:16](../src/stdlib/StringLib.hpp#L16) | WARN | Possibly unused #include directive |
| [src/tjsbc/TjsBytecodeReader.cpp:6](../src/tjsbc/TjsBytecodeReader.cpp#L6) | WARN | Possibly unused #include directive |
| [src/tjsbc/TjsBytecodeReader.cpp:24](../src/tjsbc/TjsBytecodeReader.cpp#L24) | WARN | Clang-Tidy: Constructor does not initialize these fields: data, size |
| [src/tjsbc/TjsTranslator.cpp:6](../src/tjsbc/TjsTranslator.cpp#L6) | WARN | Possibly unused #include directive |
| [src/tjsbc/TjsTranslator.cpp:58](../src/tjsbc/TjsTranslator.cpp#L58) | WARN | Clang-Tidy: Switch has 2 consecutive identical branches |
| [src/tjsbc/TjsTranslator.cpp:78](../src/tjsbc/TjsTranslator.cpp#L78) | WARN | Clang-Tidy: Switch has 2 consecutive identical branches |
| [src/tjsbc/TjsTranslator.cpp:191](../src/tjsbc/TjsTranslator.cpp#L191) | WARN | Clang-Tidy: Switch has 2 consecutive identical branches |
| [src/tjsbc/TjsTranslator.cpp:207](../src/tjsbc/TjsTranslator.cpp#L207) | WARN | Clang-Tidy: Switch has 3 consecutive identical branches |
| [src/tjsbc/TjsTranslator.cpp:230](../src/tjsbc/TjsTranslator.cpp#L230) | WARN | Clang-Tidy: Switch has 3 consecutive identical branches |
| [src/tjsbc/TjsTranslator.cpp:447](../src/tjsbc/TjsTranslator.cpp#L447) | WARN | Clang-Tidy: Either cast from 'int' to 'size_t' (aka 'unsigned long') is ineffective, or there is loss of precision before the conversion |
| [src/tjsbc/TjsTranslator.cpp:448](../src/tjsbc/TjsTranslator.cpp#L448) | WARN | Clang-Tidy: Either cast from 'int' to 'size_t' (aka 'unsigned long') is ineffective, or there is loss of precision before the conversion |
| [src/tjsbc/TjsTranslator.cpp:452](../src/tjsbc/TjsTranslator.cpp#L452) | WARN | Clang-Tidy: Either cast from 'int' to 'size_t' (aka 'unsigned long') is ineffective, or there is loss of precision before the conversion |
| [src/tjsbc/TjsTranslator.cpp:458](../src/tjsbc/TjsTranslator.cpp#L458) | WARN | Clang-Tidy: Either cast from 'int' to 'size_t' (aka 'unsigned long') is ineffective, or there is loss of precision before the conversion |
| [src/tjsbc/TjsTranslator.cpp:460](../src/tjsbc/TjsTranslator.cpp#L460) | WARN | Clang-Tidy: Either cast from 'int' to 'size_t' (aka 'unsigned long') is ineffective, or there is loss of precision before the conversion |
| [src/tjsbc/TjsTranslator.cpp:467](../src/tjsbc/TjsTranslator.cpp#L467) | WARN | Clang-Tidy: Either cast from 'int' to 'size_t' (aka 'unsigned long') is ineffective, or there is loss of precision before the conversion |
| [src/tjsbc/TjsTranslator.cpp:468](../src/tjsbc/TjsTranslator.cpp#L468) | WARN | Clang-Tidy: Either cast from 'int' to 'size_t' (aka 'unsigned long') is ineffective, or there is loss of precision before the conversion |
| [src/tjsbc/TjsTranslator.cpp:470](../src/tjsbc/TjsTranslator.cpp#L470) | WARN | Clang-Tidy: Either cast from 'int' to 'size_t' (aka 'unsigned long') is ineffective, or there is loss of precision before the conversion |
| [src/tjsbc/TjsTranslator.cpp:511](../src/tjsbc/TjsTranslator.cpp#L511) | WARN | Clang-Tidy: Either cast from 'int' to 'size_t' (aka 'unsigned long') is ineffective, or there is loss of precision before the conversion |
| [src/tjsbc/TjsTranslator.cpp:512](../src/tjsbc/TjsTranslator.cpp#L512) | WARN | Clang-Tidy: Either cast from 'int' to 'size_t' (aka 'unsigned long') is ineffective, or there is loss of precision before the conversion |
| [src/tjsbc/TjsTranslator.cpp:514](../src/tjsbc/TjsTranslator.cpp#L514) | WARN | Clang-Tidy: Either cast from 'int' to 'size_t' (aka 'unsigned long') is ineffective, or there is loss of precision before the conversion |
| [src/tjsbc/TjsTranslator.cpp:584](../src/tjsbc/TjsTranslator.cpp#L584) | WARN | Local variable 'tacBase' is never used |
| [src/tjsbc/TjsTranslator.cpp:603](../src/tjsbc/TjsTranslator.cpp#L603) | WARN | Clang-Tidy: Either cast from 'i32' (aka 'int') to 'size_t' (aka 'unsigned long') is ineffective, or there is loss of precision before the conversion |
| [src/tjsbc/TjsTranslator.cpp:688](../src/tjsbc/TjsTranslator.cpp#L688) | WARN | Clang-Tidy: Unnecessary temporary object created while calling emplace_back |
| [src/tjsbc/TjsTranslator.cpp:1135](../src/tjsbc/TjsTranslator.cpp#L1135) | WARN | Clang-Tidy: Narrowing conversion from 'size_t' (aka 'unsigned long') to signed type 'i32' (aka 'int') is implementation-defined |
| [src/tjsbc/TjsTranslator.cpp:1150](../src/tjsbc/TjsTranslator.cpp#L1150) | WARN | Clang-Tidy: Narrowing conversion from 'size_t' (aka 'unsigned long') to signed type 'i32' (aka 'int') is implementation-defined |
| [src/tjsbc/TjsTranslator.hpp:69](../src/tjsbc/TjsTranslator.hpp#L69) | WARN | Clang-Tidy: Function 'translate' should be marked [[nodiscard]] |
| [src/types/Class.cpp:16](../src/types/Class.cpp#L16) | WARN | Possibly unused #include directive |
| [src/types/Class.cpp:63](../src/types/Class.cpp#L63) | WARN | Variable 'parent' can be moved to if-statement |
| [src/types/Class.cpp:161](../src/types/Class.cpp#L161) | WARN | Local variable 'i' is never used |
| [src/types/Function.cpp:19](../src/types/Function.cpp#L19) | WARN | Possibly unused #include directive |
| [src/types/Function.cpp:51](../src/types/Function.cpp#L51) | WARN | Local variable 'tail' is never used |
| [src/types/Function.cpp:84](../src/types/Function.cpp#L84) | WARN | Member function can be made const |
| [src/types/Octet.cpp:13](../src/types/Octet.cpp#L13) | WARN | Possibly unused #include directive |
| [src/types/RegExp.cpp:60](../src/types/RegExp.cpp#L60) | ERROR | Clangd: Declaration shadows a field of 'cial::RegExp' |
| [src/types/RegExp.cpp:105](../src/types/RegExp.cpp#L105) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/types/String.cpp:5](../src/types/String.cpp#L5) | WARN | Possibly unused #include directive |
| [src/types/String.hpp:7](../src/types/String.hpp#L7) | WARN | Possibly unused #include directive |
| [src/types/TypeConverter.cpp:4](../src/types/TypeConverter.cpp#L4) | WARN | Possibly unused #include directive |
| [src/types/TypeConverter.cpp:65](../src/types/TypeConverter.cpp#L65) | WARN | Function 'cial::TypeConverter::realToHexString' can be made static or moved into an anonymous namespace to enforce internal linkage |
| [src/types/Value.cpp:13](../src/types/Value.cpp#L13) | WARN | Possibly unused #include directive |
| [src/types/Value.hpp:25](../src/types/Value.hpp#L25) | WARN | Possibly unused #include directive |
| [src/types/Value.hpp:207](../src/types/Value.hpp#L207) | WARN | Member 'reserved' is not initialized in this constructor |
| [src/types/Value.hpp:219](../src/types/Value.hpp#L219) | WARN | Clang-Tidy: Function 'tag' should be marked [[nodiscard]] |
| [src/vm/Bytecode.cpp:5](../src/vm/Bytecode.cpp#L5) | WARN | Possibly unused #include directive |
| [src/vm/Bytecode.hpp:6](../src/vm/Bytecode.hpp#L6) | WARN | Possibly unused #include directive |
| [src/vm/Chunk.hpp:119](../src/vm/Chunk.hpp#L119) | WARN | Clang-Tidy: The parameter 'value' of type 'Constant' is copied for each invocation but only used as a const reference; consider making it a const reference |
| [src/vm/Chunk.hpp:172](../src/vm/Chunk.hpp#L172) | WARN | views::keys can be used |
| [src/vm/Constant.hpp:232](../src/vm/Constant.hpp#L232) | WARN | Clang-Tidy: Std::move of the expression of the trivially-copyable type 'ConstantValue' (aka 'std::variant<std::monostate, Real, Atom, FuncMeta *, ClassMeta *, PropMeta *, SharedPayload<Octet> *>') has no effect; remove std::move() |
| [src/vm/Constant.hpp:244](../src/vm/Constant.hpp#L244) | WARN | Clang-Tidy: Std::move of the expression of the trivially-copyable type 'ConstantValue' (aka 'std::variant<std::monostate, Real, Atom, FuncMeta *, ClassMeta *, PropMeta *, SharedPayload<Octet> *>') has no effect; remove std::move() |
| [src/vm/Constant.hpp:281](../src/vm/Constant.hpp#L281) | WARN | Variable 'p' can be moved to if-statement |
| [src/vm/ExecutionTrace.hpp:41](../src/vm/ExecutionTrace.hpp#L41) | WARN | Parameter 'rt' can be made reference to const |
| [src/vm/IntegerJit.cpp:318](../src/vm/IntegerJit.cpp#L318) | WARN | Clang-Tidy: Method 'tryRun' can be made static |
| [src/vm/IntegerJit.cpp:384](../src/vm/IntegerJit.cpp#L384) | WARN | Clang-Tidy: Method 'observe' can be made static |
| [src/vm/VM.hpp:188](../src/vm/VM.hpp#L188) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/vm/VM.hpp:219](../src/vm/VM.hpp#L219) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/vm/VM.hpp:260](../src/vm/VM.hpp#L260) | WARN | Clang-Tidy: Throwing an exception whose type 'vm::ScriptExceptionCarrier' is not derived from 'std::exception' |
| [src/vm/VMState.cpp:228](../src/vm/VMState.cpp#L228) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/vm/VMState.cpp:239](../src/vm/VMState.cpp#L239) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/vm/VMState.cpp:280](../src/vm/VMState.cpp#L280) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/vm/VMState.cpp:902](../src/vm/VMState.cpp#L902) | WARN | Clang-Tidy: Throwing an exception whose type 'ScriptExceptionCarrier' is not derived from 'std::exception' |
| [src/vm/VMState.cpp:1358](../src/vm/VMState.cpp#L1358) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/vm/VMState.cpp:1391](../src/vm/VMState.cpp#L1391) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/vm/VMState.cpp:1451](../src/vm/VMState.cpp#L1451) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/vm/VMState.cpp:1470](../src/vm/VMState.cpp#L1470) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/vm/VMState.cpp:1514](../src/vm/VMState.cpp#L1514) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/vm/VMState.cpp:1561](../src/vm/VMState.cpp#L1561) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/vm/VMState.cpp:1931](../src/vm/VMState.cpp#L1931) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [src/vm/VMState.cpp:1950](../src/vm/VMState.cpp#L1950) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [tests/cli_test.py:17](../tests/cli_test.py#L17) | WEAK WARNING | Function name should be lowercase |
| [tests/fuzz_regress_test.cpp:36](../tests/fuzz_regress_test.cpp#L36) | WARN | Variable 'preProcessor' can be moved to inner scope |
| [tests/ieee_test.cpp:52](../tests/ieee_test.cpp#L52) | WARN | Variable 'nan' can be made constexpr |
| [tests/ieee_test.cpp:53](../tests/ieee_test.cpp#L53) | WARN | Variable 'inf' can be made constexpr |
| [tests/ieee_test.cpp:113](../tests/ieee_test.cpp#L113) | WARN | Variable 'posInf' can be made constexpr |
| [tests/ieee_test.cpp:118](../tests/ieee_test.cpp#L118) | WARN | Variable 'negInf' can be made constexpr |
| [tests/ieee_test.cpp:122](../tests/ieee_test.cpp#L122) | WARN | Variable 'nanVal' can be made constexpr |
| [tests/ieee_test.cpp:128](../tests/ieee_test.cpp#L128) | WARN | Variable 'zero' can be made constexpr |
| [tests/ieee_test.cpp:133](../tests/ieee_test.cpp#L133) | WARN | Variable 'negZero' can be made constexpr |
| [tests/ieee_test.cpp:137](../tests/ieee_test.cpp#L137) | WARN | Variable 'one' can be made constexpr |
| [tests/ieee_test.cpp:153](../tests/ieee_test.cpp#L153) | WARN | Variable 'values' can be made constexpr |
| [tests/ieee_test.cpp:164](../tests/ieee_test.cpp#L164) | WARN | Variable 'a' can be made constexpr |
| [tests/ieee_test.cpp:164](../tests/ieee_test.cpp#L164) | WARN | Variable 'b' can be made constexpr |
| [tests/jit_test.cpp:109](../tests/jit_test.cpp#L109) | WARN | Variable 'comparisons' can be made constexpr |
| [tests/runtime_lifetime_test.cpp:36](../tests/runtime_lifetime_test.cpp#L36) | WARN | Local variable 'assigned' is only assigned but never accessed |
| [tests/runtime_lifetime_test.cpp:177](../tests/runtime_lifetime_test.cpp#L177) | WARN | Clang-Tidy: Do not use static_cast to downcast from a base to a derived class; use dynamic_cast instead |
| [tests/runtime_lifetime_test.cpp:234](../tests/runtime_lifetime_test.cpp#L234) | WARN | Variable 'bytes' can be made constexpr |
| [tests/ssa_test.cpp:428](../tests/ssa_test.cpp#L428) | WARN | Local variable 'r0' is never used |
| [tests/tjsbc_reader_test.cpp:12](../tests/tjsbc_reader_test.cpp#L12) | WARN | Possibly unused #include directive |
| [tests/tjsbc_reader_test.cpp:14](../tests/tjsbc_reader_test.cpp#L14) | WARN | Possibly unused #include directive |
| [tests/tjsbc_reader_test.cpp:47](../tests/tjsbc_reader_test.cpp#L47) | WARN | Variable 'header' can be made constexpr |
| [tests/tjsbc_reader_test.cpp:51](../tests/tjsbc_reader_test.cpp#L51) | WARN | Variable 'dataArea' can be made constexpr |
| [tests/type_system_test.cpp:52](../tests/type_system_test.cpp#L52) | WARN | Variable 'bytes' can be made constexpr |
| [tests/type_system_test.cpp:103](../tests/type_system_test.cpp#L103) | WARN | Variable 'bytes' can be made constexpr |
| [tests/type_system_test.cpp:109](../tests/type_system_test.cpp#L109) | WARN | Variable 'bytes' can be made constexpr |
| [tests/type_system_test.cpp:152](../tests/type_system_test.cpp#L152) | WARN | Variable 'bytes' can be made constexpr |
| [tests/type_system_test.cpp:180](../tests/type_system_test.cpp#L180) | WARN | Clang-Tidy: Local copy 'copy' of the variable 'original' of type 'const Value' is never modified; consider avoiding the copy |
| [tests/value_storage_test.cpp:23](../tests/value_storage_test.cpp#L23) | WARN | Binary operator '=' acts on identical operands |
| [tests/value_storage_test.cpp:27](../tests/value_storage_test.cpp#L27) | WARN | Clang-Tidy: Local copy 'independent' of the variable 'small' of type 'Value' is never modified; consider avoiding the copy |
| [tests/value_storage_test.cpp:30](../tests/value_storage_test.cpp#L30) | WARN | Clang-Tidy: Local copy 'grown' of the variable 'independent' of type 'Value' is never modified; consider avoiding the copy |
| [tools/lsp/cial_lsp.py:395](../tools/lsp/cial_lsp.py#L395) | WEAK WARNING | Shadows name 'scope' from outer scope |
| [tools/lsp/cial_lsp.py:720](../tools/lsp/cial_lsp.py#L720) | WEAK WARNING | Shadows name 'path' from outer scope |
| [tools/lsp/cial_lsp.py:721](../tools/lsp/cial_lsp.py#L721) | WEAK WARNING | Shadows name 'root' from outer scope |
| [tools/lsp/cial_lsp.py:951](../tools/lsp/cial_lsp.py#L951) | WEAK WARNING | Missing return statement on some paths |
| [tools/lsp/test_protocol.py:73](../tools/lsp/test_protocol.py#L73) | WEAK WARNING | Method 'position_of' may be 'static' |
| [tools/lsp/test_protocol.py:19](../tools/lsp/test_protocol.py#L19) | WEAK WARNING | Function name should be lowercase |
| [tools/lsp/test_protocol.py:24](../tools/lsp/test_protocol.py#L24) | WEAK WARNING | Function name should be lowercase |

## 后续修复（2026-10-31）

第一笔提交 `0757384` 已移除失效 AstFormatter，同时提交本次原生绑定重构。
本轮后续改动处理了 RegExp 字段遮蔽、指令 class/struct 声明不一致、编译结果显式初始化、
优化器输出寄存器初始化、未使用的局部变量和重复调试断言。没有为了消除提示而给
Value 预留字节增加热路径清零，也没有改动测试中的自赋值/生命周期覆盖。

偏移警告的源码复核进一步确认了转译器的边界问题，已增加：

- 固定长度指令和 CALL/展开调用操作数边界检查；非法负计数在 reserve 前报错。
- 类体常量/成员槽、superclass context 和 getter 指针检查，类体偏移采用 size_t。
- 跳转/异常目标使用宽整数计算后检查代码边界，TAC 映射采用 size_t + optional，
  不再用负值哨兵与窄化的 TAC 下标混用。
- 删除四码字属性指令及 INC/DEC 成员变体对第五码字的多余读取。
- 修复 MUL 的 PD/PI/P 解码范围遗漏，新增 direct-member 执行回归。

源码与行为测试分别见 `src/tjsbc/TjsTranslator.cpp` 和
`tests/tjsbc_translator_test.cpp`；专项 4 个用例 / 24 条断言通过，macOS Debug/Release
各 242 项 CTest 全过。

复查了 10 个修改文件，ERROR 为 0。对应文件在原始快照中的 ERROR/WARN/弱警告共
69 条，修复后为 35 条；另删除的 AstFormatter 在原始快照中有 28 条。
这是局部复查比较，不是全项目重新扫描后的剩余总数。
原始 3 条 ERROR 对应的源位置已删除或更名处理。
[修改文件复查 JSON](reports/clion-warning-fixes-2026-10-31.json) 保留剩余提示，主要为
类型标记保护的 static_cast、const/ranges/auto 建议及 opcode 分组的重复 case 风格提示。
