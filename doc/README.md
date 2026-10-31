# 文档索引

| 主题 | 文档 |
|---|---|
| 入门与排错 | [构建](building.md) · [打包与发布](releasing.md) · [命令行与 REPL](cli.md) · [FAQ](faq.md) |
| 宿主与编辑器 | [C++ 集成](embedding.md) · [LSP 与 VSCode](lsp.md) |
| 语言 | [语法与实现范围](syntax.md) · [兼容性](compatibility.md) · [语言参考](language/README.md) |
| 架构与优化 | [架构](architecture.md) · [SSA](ssa.md) · [JIT](jit.md) · [尾递归](tail-recursion.md) |
| 调试 | [执行路径导出](execution-trace.md) |
| 字节码 | [TJS2 格式](bytecode/tjs2-bytecode.md) · [内部指令](bytecode/ciallang-bytecode.md) · [转译](bytecode/mapping.md) · [反编译器方案](bytecode/decompiler-plan.md) |
| 测量与诊断 | [性能记录](performance.md) · [历史静态检查数据](reports/) · [参考侧复核](reports/2026-10-31-reference-audit.md) |
| 规划 | [SDK、开发工具与模块化路线图](roadmap.md) · [诊断与源码位置重构](diagnostics-plan.md) |
| 参与开发 | [待办](../TODO.md) · [开发约定](../AGENTS.md) |
| 脚本单元测试 | [TJS 用例与双引擎运行](../tests/tjs/README.md) |

官方语法参考：[TJS2 リファレンス](https://hydrozoa.felisworks.com/doc/tjs2doc/contents/index.html)。
TJSCompiler 的 Bison 文法见[文法快照与来源](grammar/README.md)。

使用方法写入专题指南，局部实现约束写在代码旁，常见问题集中到 FAQ。
待办只保留未完成项；历史测量注明环境与基线，修复过程可从 Git 历史查阅。
