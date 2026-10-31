# TJSCompiler 文法快照

本目录保存 [2468785842/TJSCompiler](https://github.com/2468785842/TJSCompiler/tree/d8044c3a2314e647d81948085ee5476dc77eca2b/src/syntax/bison)
的 Bison C++ 适配文法，固定提交为 `d8044c3a2314e647d81948085ee5476dc77eca2b`。
文件与该提交逐字节一致；它们不是未经修改的 Kirikiri 原始发布包，也不参与 Ciallang 的 CMake 构建。
Ciallang 使用手写 Parser、PreProcessor 和 DateParser，对照入口见 [语法说明](../syntax.md)。

| 文件 | 上游路径 | SHA-256 |
|---|---|---|
| [tjs.y](tjs.y) | `src/syntax/bison/tjs.y` | `e581d6d1dfa95dbd8367cbf683b6925ddb05b9fd43494fce5ce73f88a02c2140` |
| [tjsdate.y](tjsdate.y) | `src/syntax/bison/tjsdate.y` | `b434d766e25faf10c1f57db010360e320eb01a9cb53896bb9fb18af4ef1d14c5` |
| [tjspp.y](tjspp.y) | `src/syntax/bison/tjspp.y` | `bb54ecf0a70f732855854b516a308e58c784adc429796f6173babadebd4cdca2` |

## 如何使用

- `tjs.y`：主语言产生式、语法树与代码生成动作；静态语义仍需结合 `tjsInterCodeGen.cpp`、`tjsInterCodeExec.cpp`。
- `tjspp.y`：32 位整数预处理表达式；赋值优先级由本文件声明，不等同于普通脚本表达式。
- `tjsdate.y`：日期字符串解析；月份名、时区词表和数值归一化还依赖 `tjsDateParser.*` / `tjsDateWordMap.cpp`。

文法要求 Bison 3.8.2 和 GNU m4。下面只检查生成，不链接参考引擎；生成的 C++ 仍依赖
TJSCompiler 的头文件、词法器和运行时，不能单独编译为 Ciallang 的解析器：

```sh
mkdir -p build/grammar-review
cd build/grammar-review
bison --report=state -Werror=conflicts-sr -Werror=conflicts-rr ../../doc/grammar/tjs.y
bison --report=state -Werror=conflicts-sr -Werror=conflicts-rr ../../doc/grammar/tjspp.y
bison --report=state -Werror=conflicts-sr -Werror=conflicts-rr ../../doc/grammar/tjsdate.y
```

三份快照均通过上述生成检查。`tjs.y` 的 `%expect 1` 对应 `catch` 后接 `(` 的移进/归约歧义；
不是 `else` 配对问题。其余两份无移进/归约或归约/归约冲突。
`-Wall` 仍会给出空产生式、无效结合性等风格提示，保留上游写法，不使用 `--update` 改写参考文件。

同步时整份替换快照并更新提交与摘要，不在 `.y` 中掺入 Ciallang 的规则或修复说明。
文档引用优先使用产生式名称，避免头文件调整后行号失效。
