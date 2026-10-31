# 命令行与 REPL

CLI 使用 argparse 解析参数；`src/cli/Options.cpp` 校验模式和参数数量，`Application.cpp` 调度，`Compilation.cpp` 处理编译/执行，`Diagnostics.cpp` 输出诊断，`Repl.cpp` 提供交互循环。`src/main.cpp` 只负责进程入口和顶层异常报告。

```sh
cial script.tjs
cial --tjsbc program.tjsbc
cial --check-json script.tjs
cial --check-json --source-name untitled:1 -        # 从 stdin 读取，保留逻辑源码名
cial --dump-cfg script.tjs
cial --dump-ssa script.tjs
cial --trace-execution script.tjs output.dot
cial --tail-recursion --time script.tjs
cial --jit script.tjs
cial --trace-jit script.tjs
cial --repl
cial --help
```

无输入文件时进入 REPL。参数可放在输入文件前后，`--` 结束选项解析，可用于以 `-` 开头的文件名。脚本/字节码/编译检查/CFG/SSA/REPL 模式互斥；JIT 与 Trace JIT 互斥；编译模式拒绝执行参数。JIT 需要构建时启用，范围见 [jit.md](jit.md)；精确路径记录使用解释器。

## 输出与退出码

- 普通执行的 stdout 只包含脚本输出；错误、JIT/尾递归统计与可选 `--time` 计时写 stderr。
- `--check-json` 只编译，不初始化脚本标准库，不执行用户脚本；stdout 为一个 JSON 对象，位置为从零开始的 UTF-8 字节列，LSP 负责转换为 UTF-16。见 [lsp.md](lsp.md)。
- `--dump-cfg` / `--dump-ssa` 编译并输出分析结果，不执行脚本；`--dump-tac` 将执行前的 TAC 写 stderr。
- `--trace-execution` 把执行路径写入指定 DOT 文件；运行期错误发生时仍保存此前的路径；拒绝将输出写回输入文件。
- 退出码：成功 `0`，输入文件/编译/运行/输出错误 `1`，命令行参数错误 `2`。

## 诊断

文本诊断格式为 `cial: error[code]: 源:行:列: 消息`，随后附源码摘录与按显示宽度对齐的指示符（`src/common/SourceFile.cpp`）；位置 1 基展示，内部按字节半开区间记录。运行期错误同样由顶层渲染——VM 只传播异常值，位置作为元数据记录（`src/vm/VMState.cpp` 的 FaultInfo），脚本捕获的 `e.message` 因此不含行号；摘录在打印时按源码身份重读文件渲染（Python linecache 式），stdin/REPL/`<eval>` 等逻辑名或文件已变动时读不到即退化为单行，不伪造内容；执行字节码产物（`--tjsbc`）无源码身份，只打印纯消息。颜色默认按 stderr 是否为终端自动启用，`NO_COLOR` 环境变量与 `--no-color` 可关闭。

`--check-json` 默认输出 schema 2：`schema`、`ok`、`columnEncoding`、`source`（源码身份）与 `diagnostics[]`；每条含 `code`、`severity`、纯文本 `message`、半开的 `line/column/endLine/endColumn`（0 基字节列，`end` 为排他）及可选 `range`（字节偏移）。`--json-schema 1` 派生输出旧字段形状（无 `schema` 标记、`endColumn` 为包含式），供未迁移消费者过渡；其他版本号报参数错误。stdin 输入（`-`）配合 `--source-name` 为未保存内容保留逻辑源码名，宿主不得把临时路径暴露给编辑器。

错误码按阶段前缀划分（`lex.*`、`parse.*`、`gen.*`、`io.*`），首批覆盖已迁移路径，见 [诊断重构计划](diagnostics-plan.md)。

`CIAL_DUMP` 和 `CIAL_TJSBC_DUMP` 保留用于调试，输出统一写 stderr；后者值为 `all` 时输出全部转译 context（`src/cli/Application.cpp`）。

## REPL

```text
>>> var x = 40;
>>> x + 2
42
>>> function add(v) {
...     return v + 1;
... }
>>> add(x)
41
```

声明和函数保留在当前会话，表达式只求值一次并输出非 void 结果。未闭合的括号、字符串和块注释自动续行；行尾 `\` 可显式续行。分号遵循 TJS 语法，单独的表达式可省略末尾分号。多行 `if/else` 或 `try/catch` 应将续接关键字放在闭括号同一行，或用行尾 `\` 继续。

命令为 `:help`、`:quit`（或 `:q`）、`:abort`（丢弃未完成输入）。Ctrl-D / Windows EOF 结束输入；未完成输入到 EOF 会报告错误。语法或运行错误后可继续执行，已发生的赋值/副作用保留。管道输入不显示提示符或横幅；发生过错误则退出码为 `1`，交互终端正常退出为 `0`（`src/cli/Repl.cpp`）。

## 版本

`cial --version` 输出由 Python 从 `vcpkg.json` 的 `version-string` 生成的完整运行时版本；预发布后缀也包含在内。
CLI、vcpkg manifest、压缩包与版本标签的一致性检查见[发布指南](releasing.md)。
