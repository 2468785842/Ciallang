# 交互式调试器（cial-dbg）

`cial-dbg` 是终端 CLI 调试器：断点、单步、`debugger` 语句或步进命中后进入
命令提示符，恢复命令返回后 VM 继续执行。单线程同步模型——停止期间 VM 不运行，
无并发；调试会话强制关闭 JIT（JIT 会整段跳过派发环，绕过调试钩子）。

```sh
cmake --preset "MacOS Debug" -DCIAL_BUILD_DBG=ON   # 或 -DCIAL_BUILD_TOOLS=ON 一键全开
cmake --build --preset "MacOS Debug" --target cial-dbg
./build/MacOS/Debug/tools/dbg/cial-dbg script.tjs
```

## 会话流程

启动后停在入口提示符（脚本已编译、入口帧已建、尚未执行）。`run` 开始执行；
命中停止条件后再次进入提示符。stdin EOF 等价 `quit`。

```
$ cial-dbg script.tjs
cial-dbg: loaded script.tjs (use `run` to start, `help` for commands)
Stopped at script.tjs:1 (<script>)
=> 1 | function add(a, b) {
cial-dbg> break 6
Breakpoint 1 at script.tjs:6 (1 location)
cial-dbg> run
Stopped at script.tjs:6 (<script>)
=> 6 | var y = add(x, 5);
cial-dbg> step
Stopped at script.tjs:2 (add)
=> 2 |     var t = a + b;
cial-dbg> locals
  a = 10
  b = 5
  t = void
cial-dbg> continue
15
program finished
```

## 命令

| 命令 | 缩写 | 说明 |
|---|---|---|
| `run` / `continue` | `r` / `c` | 恢复执行直到下一停止条件 |
| `step` | `s` | 行步进：停在下一条不同源码行（会进入调用） |
| `next` | `n` | 行步过：同 `step`，但不进入更深的调用帧 |
| `finish` | | 步出：等当前帧返回后停 |
| `break <line>` | `b` | 按源码行设断点（1 基）；无代码行向前吸附 ≤3 行并报告实际行 |
| `delete [<n>]` | `d` | 删除断点（无参数删全部） |
| `bt` | `backtrace` | 帧栈，`#0` 为最新帧，`*` 标记选中帧 |
| `frame <n>` | | 切换选中帧（编号同 `bt`），影响 `locals`/`print`/`list` |
| `locals` | | 列出选中帧的局部变量 |
| `print <name\|%reg>` | `p` | 打印局部/全局变量或当前帧寄存器 |
| `list [line]` | `l` | 源码窗口（上下各 5 行），默认当前行 |
| `disasm [n]` | `dis` | 反汇编当前指令前后（bc 偏移 + 行号 + 寄存器值注释） |
| `quit` | `q` | 放弃执行退出（退出码 0） |
| `help` | `h` | 命令列表 |

值打印与 `println` 同格式；数组/字典递归展开（深度限 3，环检测标记 `<cycle>`）。
空输入重复上一条命令。

## 实现

- 引擎侧钩子 `vm::DebugHook`（`src/vm/DebugHook.hpp`）：`VMState::setDebugHook`
  借用挂接，派发环在每条指令前同步调用 `onInstruction`（`VMState.cpp`，与
  ExecutionTrace 同位）；脚本 `debugger` 语句走 `onDebugger`。会话内抛
  `vm::DebugAbort`（非 `std::exception`，不被 runLoop 包装成脚本异常）可整段放弃执行。
- 会话核心 `tools/dbg/DebugSession.cpp`：断点按 `(chunk, bc 偏移)` 存站点；
  行→pc 经 `Chunk::rlc().rawEntries()` 反查；反汇编定位用 `Chunk::tacIndexAtBc`。
  入口 Chunk 由 `RootGuard` 保 GC 根贯穿会话，函数 Chunk 经常量图保持可达。
- `step`/`next` 以（帧深度，源码行）为基线：行号未知或同语句的中间指令不停。

## 已知限制（M1）

- 无帧内表达式求值（`print` 仅支持变量名与 `%寄存器`；`VM::evalExpr` 只在
  全局作用域求值，帧上下文求值是独立特性）。
- 无条件断点/监视点/数据断点；断点不能跨文件指定（仅当前脚本）。
- 部分语句节点（如 `return`、块语句）在 Parser 中不带位置，函数体内的行表
  对这些语句缺失，`break <行>` 会吸附到别处或拒绝；`debugger;` 语句不受影响。
  该缺口属前端 AST 位置元数据，修复在前端进行，不属调试器范围。
- 非当前帧的 `disasm` 不带寄存器值注释（`VMState::reg` 只读当前帧）。
