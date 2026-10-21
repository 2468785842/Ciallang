# TJS2 语言规范笔记（doc/language）

本目录是**原版 TJS2 语言参考**（TJS2 リファレンス，W.Dee 著）的中文总结，按原文章节逐章整理。

* 源站（英文/日文混排镜像）：https://a-rabin.github.io/krkr2doc-en/tjs2doc/contents/index.html
* 官方站（语法权威）：https://hydrozoa.felisworks.com/doc/tjs2doc/contents/index.html
* 与本项目实现状态的关系：本目录描述**原版语义**（行为基准）；本项目“实现了多少、缺什么”见 [../syntax.md](../syntax.md) 状态矩阵与 [../compatibility.md](../compatibility.md)。
* 产生式（语法权威）另见 [../grammar/tjs.y](../grammar/tjs.y)。

## 章节索引

| 文档 | 内容 | 原文章节 |
|---|---|---|
| [Token.md](Token.md) | 裸单词、保留字、标识符、符号（最长匹配）、数值/字符串/@字符串/octet/正则字面量 | トークン |
| [Preprocessor.md](Preprocessor.md) | 条件编译：@set / @if / @endif、version 预定义变量 | Preprocessor |
| [Variable.md](Variable.md) | var 声明、重复声明语义、块级作用域与遮蔽、全局作用域 | 変数 |
| [Types.md](Types.md) | 六种内部类型（void/Integer/Real/String/Object/Octet）、typeof 汇总 | Datatypes |
| [Factor.md](Factor.md) | 项：true/false、void/null、Infinity/NaN、super/global/this、式中函数/数组/字典 | 項（Keywords） |
| [StatementAndBlock.md](StatementAndBlock.md) | 语句与块、执行顺序（函数/类/属性先注册可后向引用）、全局对象 | ステートメントとブロック |
| [ControlFlow.md](ControlFlow.md) | if 与 else 配对、while/do-while、for 三段语义、switch（表达式 case/穿透）、with（只求值一次） | if / while / for / switch / with |
| [Exception.md](Exception.md) | try/catch、异常对象 e.message、throw、重抛 | try |
| [Function.md](Function.md) | 声明/调用、实参 void 空位、个数不匹配容错、默认参数、匿名函数、`...` 转发、`形参*` 数组化、`表达式*` 展开 | 関数 |
| [Property.md](Property.md) | property 声明、setter/getter、属性对象与 `&`/`*` 运算符 | プロパティ |
| [Class.md](Class.md) | 类声明、new/invalidate/finalize、闭包与 incontextof、extends/super、多重继承、override | Class |
| [ExpressionAndOperator.md](ExpressionAndOperator.md) | 表达式、真假规则、按优先级从低到高的全部运算符 | 式と演算子 |

## 未收录的原文章节

* **About TJS2**：语言定位简介（面向嵌入的面向对象脚本语言，语法类似 JavaScript/Java）。
* **Array / Dictionary / String / Math / Date / RegExp / Exception / Octet / RandomGenerator 类**：标准库参考（类与方法），非语言语法；需要时直接查原文对应页面。
* **vmcodes**：TJS2 虚拟机指令参考——本项目已有更完整的 [../bytecode/tjs2-bytecode.md](../bytecode/tjs2-bytecode.md)。
* **tTJS / iTJSDispatch2 / 基本数据类型 / tTJSVariant 型 / 基本的な使い方**：宿主 C++ 侧（引擎类、对象接口、嵌入示例），与脚本语法无关。其中 C++ 类型的要点在 [Types.md](Types.md) 末尾有备注。
* **Keywords（キーワードリスト）**：全书符号/关键字交叉索引页。
