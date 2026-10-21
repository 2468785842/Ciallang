# 文档索引

> 修改代码后请同步对应文档（约定见 [AGENTS.md](../AGENTS.md) 文档维护一节）。
> AI/新成员从 [AGENTS.md](../AGENTS.md) 进入。

## 架构与设计

| 文档 | 内容 |
|---|---|
| [architecture.md](architecture.md) | Ciallang 架构设计：编译/执行流水线、模块职责、关键类、类型系统与 GC、与 TJSCompiler 的逐段对照 |
| [bytecode/mapping.md](bytecode/mapping.md) | **TJS2 字节码 → Ciallang 内部指令的转译设计**（字节码兼容的实现蓝本：操作码映射表、寄存器映射、调用约定、常量池转换、风险清单） |

## 字节码（新旧项目）

| 文档 | 内容 |
|---|---|
| [bytecode/tjs2-bytecode.md](bytecode/tjs2-bytecode.md) | 旧版 TJS2（TJSCompiler）字节码规范：128 操作码全表、寄存器模型、`"TJS2" "100\0"` 序列化文件格式、内存态/文件态地址换算 |
| [bytecode/ciallang-bytecode.md](bytecode/ciallang-bytecode.md) | 本项目（Ciallang）指令集规范：55 条 TAC、扁平字节码 17 操作码与 LEB128 编码、Chunk 结构、未编码指令清单 |

## 语言

| 文档 | 内容 |
|---|---|
| [syntax.md](syntax.md) | TJS2 语法完整参考（词法/表达式优先级/语句/字面量/函数），每个构造标注本项目实现状态 |
| [compatibility.md](compatibility.md) | 源码/字节码/标准库三维兼容矩阵 + 分阶段路线图 |
| [TJS2_OPCODE.md](TJS2_OPCODE.md) | （历史笔记）TJS2 VM ISA 学习笔记——寄存器机模型与助记符分组，写作 tjs2-bytecode.md 之前的原始参考 |
| [grammar/](grammar/) | 原版 bison 语法副本：`tjs.y`（主语法，产生式权威）、`tjsdate.y`（Date 解析）、`tjspp.y`（预处理表达式） |
| [language/](language/) | **原版 TJS2 语言规范中文总结**（逐章对应官方 リファレンス：词法/预处理/变量/类型/项/语句/控制流/异常/函数/属性/类/运算符），索引见 [language/README.md](language/README.md) |

## 外部参考

- 官方语言参考（语法权威）：[TJS2 リファレンス](https://hydrozoa.felisworks.com/doc/tjs2doc/contents/index.html)
- 旧版参考实现：`D:\Users\LiDon\CLionProjects\TJSCompiler`（原版 Kirikiri TJS2 提取版）

## 任务追踪

- [TODO.md](../TODO.md)：性能/正确性/语言特性缺口清单（改代码前先看）
