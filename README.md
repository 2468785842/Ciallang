# Ciallang

> 一个用 C++20 编写的轻量级脚本语言解释器，专门用于支持 Kirikiri（吉里吉里）游戏引擎中的 TJS2 脚本语言。

<p align="center">
  <img src="https://img.shields.io/badge/language-C%2B%2B20-blue?logo=c%2B%2B" alt="C++20">
  <img src="https://img.shields.io/badge/build-CMake-green?logo=cmake" alt="CMake">
  <img src="https://img.shields.io/badge/license-MIT-yellow?logo=open-source-initiative" alt="MIT License">
</p>

## 🚧 开发进度

### 🔧 核心特性
  * [x] `REPL` 交互运行（多行输入、表达式回显、共享全局与错误恢复；见 [doc/cli.md](doc/cli.md)）
  * [x] `JIT` 即时编译基础（可选 x64 整数原生代码后端，默认关闭；`--jit`，范围与构建见 [doc/jit.md](doc/jit.md)）
  * [x] `CFG` 控制流图生成（基本块级，含跳转/终止/try 隐式异常边，`src/gen/ControlFlowGraph.cpp`）
  * [x] `SSA` 静态单赋值构造（按需分析：支配树/pruned phi/重命名/def-use 链，`--dump-ssa`，见 [doc/ssa.md](doc/ssa.md)；执行 IR 尚未切换为 SSA，基于 SSA 的优化待后续）
  * [x] `常量传播` 与块间状态分析（块内溢出安全整数折叠 + 唯一前驱块间继承，`src/gen/Optimizer.cpp`）
  * [x] `死代码消除`（活跃性分析驱动的死 `LoadImm` 清理；常量池 `Load` 因实例化对象保守保留）
  * [x] `循环优化`（回边检测——支配树 + 自然循环识别，`ControlFlowGraph::getLoops()`；循环体变换待后续）
  * [x] `尾递归消除`（显式 `--tail-recursion`，保守数值自递归子集；见 [doc/tail-recursion.md](doc/tail-recursion.md)）
  * [x] `Trace JIT` 热路径跟踪基础（热点计数、路径录制、分支守卫与失败回退；`--trace-jit`，见 [doc/jit.md](doc/jit.md)）
  * [x] `可视化调试`（`--dump-cfg` 导出静态控制流；`--trace-execution <script> <output.dot>` 记录真实字节码节点、执行次数与跨帧路径；见 [doc/execution-trace.md](doc/execution-trace.md)）

---

### 🧩 语言特性

#### 预处理

* [x] `@set`
* [x] `@if` `@endif`

#### 语法结构
* [x] `var` / `const` 变量声明与使用（const 须初始化；赋值放行——对齐原版 tjs.y 设计现状；类型注解 `var a:int` 文档性解析并丢弃——对齐 tjs.y:339 "Ignore types"）
* [x] `if`
* [x] `while`
* [x] `do while`
* [x] `for`（含 `for(const)`）
* [x] `switch`
* [x] `try-catch`（含 `catch()` 空参数列表）
* [x] `function` 函数定义与调用（块内提升，先调用后声明）
* [x] `function f(a, b*)` collapse 参数 / `function f(*)` 未命名收拢（超额实参收拢成 Array）
* [x] `property`
* [x] `class`
* [x] `with`（含块内 `.member` 裸词链访问）

#### 数据类型
* [x] `Void` 空
* [x] `Integer` 整数类型
* [x] `Real` 实数类型
* [x] `String` 字符串类型
* [x] `Object` 对象类型
* [x] `Octet` 字节流类型

#### 原子表达式
* [x] `true` `false`
* [x] `void`
* [x] `null`
* [x] `Infinity` `-Infinity`
* [x] `0x1.07CCp14` 十六进制浮点 / `0b1.1` 二进制浮点 / `1e3` 无符号科学计数法
* [x] `super`
* [x] `global`
* [x] `this`
* [x] `function` 函数表达式声明
* [x] `@"xx"` 模版字符串声明
* [x] `[]` Array 声明
* [x] `%[]` Dictionary 声明（键形式 `"k" => v` 与 `sym: v`）
* [x] `<% xx xx ... %>` Octet 声明
* [x] `/pattern/flags` 正则字面量（flags：`g` 全局 / `i` 忽略大小写）

#### 表达式
* [x] `A,B` 顺序表达式
* [x] `A <-> B` 交换表达式
* [x] `A operator B` 赋值表达式 `=  &=  |=  ^=  -=  +=  %=  /=  \=  *=  ||=  &&=  >>=  <<=  >>>=`
* [x] `A ? B : C` 条件运算符
* [x] `A if B` 后缀条件表达式（B 为真才求值 A，否则 void）
* [x] `A || B` 逻辑或
* [x] `A && B` 逻辑与
* [x] `A | B` 按位或
* [x] `A & B` 按位与
* [x] `A ^ B` 按位异或
* [x] `A operator B` 相等判断运算符 `== != === !==`
* [x] `A operator B` 比较运算符 `< > <= >=`
* [x] `A operator B` 位移运算符 `>> << >>>`
* [x] `A operator B` 加减运算符 `+ -`
* [x] `A operator B` 乘除运算符 `% / \ *`
* [x] `operator A` 前缀单目运算符 `! new invalidate isvalid - ~ typeof + & -- ++ delete # $ *`
* [x] `operator A` 类型转换运算符 `int real string`
* [x] `A operator` 后缀单目运算符 `() . -- ++`
* [x] `f(expr*)` 展开实参 / `f(*)` 未命名透传 / `f(...)` 省略透传（透传本帧实际实参）
* [x] `A operator` 后缀单目运算符 `[]`（数组/字典/字符串通用下标读写）
* [x] `A operator` 后缀单目运算符 `!`（即时求值：A 的字符串值作为 TJS 表达式编译执行；空串原值不变）
* [x] `A instanceof B` 实例判断运算符
* [x] `A in B`（保留字：对齐原版——操作数不求值、结果 void，勿当成成员存在判断）
* [x] `A incontextof B` 上下文替换运算符

---

### 📚 标准库

* [x] Exception // message 成员；原生错误自动包装为 Exception 实例
* [x] Array // 字面量/new Array()/下标读写/push/pop/unshift/shift/insert/remove/erase/reverse/join/split/sort(含比较器回调)/count/save(文本行)/load(文本行)/saveStruct(文本+二进制)/loadStruct(二进制)/pack/assignStruct
* [x] Dictionary // 字面量/new Dictionary()/下标读写/assign/assignStruct/clear/erase/count/saveStruct(文本+二进制)/loadStruct(二进制入自身,文本回落解析)；load/save 原版即 TODO 空实现，按特性同步
* [x] Date // new Date(now/分量/字符串) + get/set 系 + getTimezoneOffset + parse 完整文法（tjsdate.y 全 11 形态，对拍逐位一致）
* [x] Math // 静态方法 16 个+常量 8 个+random(Xorshift)+Math.RandomGenerator（MT19937 与原版序列逐位一致、serialize 互认）
* [x] RegExp // /pattern/flags 字面量 + test/match/exec/compile/replace(含函数回调)/split + 全部匹配状态属性(码点语义) + RegExp.last 静态属性 + new RegExp(pattern, flags) 构造器
* [x] String // charAt/indexOf/toUpperCase/toLowerCase/substring/sprintf/replace(g 全局)/split/escape/trim/reverse/repeat/length

## 构建与使用

要求 CMake 3.25+、C++20 编译器与 vcpkg；预设使用 Ninja，Linux 预设使用 Clang。
设置 `VCPKG_ROOT` 后：

```sh
cmake --preset "Linux Debug"
cmake --build --preset "Linux Debug"
ctest --preset "Linux Debug"
./build/Linux/Debug/cial script.tjs
./build/Linux/Debug/cial --repl
./build/Linux/Debug/cial --tjsbc program.tjsbc
```

Windows/macOS 使用 `Windows Debug` / `MacOS Debug` 预设，Windows 产物带 `.exe`。
无输入文件也会进入 REPL，支持多行代码、表达式回显和会话共享变量。
运行成功返回 `0`，编译/运行失败返回 `1`，参数错误返回 `2`。
`--help` 查看参数，`--time` 显式开启计时并写到 stderr。

作为 CMake 子项目引入时默认只构建库，目标为 `Ciallang::Ciallang`。
CLI 的 argparse、测试的 Catch2、JIT 的 asmjit 分别由 `cli`、`tests`、`jit` vcpkg feature 控制。
构建开关和依赖见 [构建指南](doc/building.md)，宿主 API 见 [库集成](doc/embedding.md)，
全部命令与 REPL 见 [命令行指南](doc/cli.md)。

### ⚡ 基准与 TJS2 对拍

内置双引擎基准与兼容性对拍设施。TJSCompiler 参考实现路径
默认从当前项目的兄弟目录 `../TJSCompiler` 探测；可用环境变量
`CIAL_TJS_ROOT` 或 CMake 缓存变量 `TJS_ROOT` 覆盖，也可用 `CIAL_TJS_EXE`
指定可执行文件。相对路径均以 Ciallang 源码根目录为基准解析，见 `CMakeLists.txt`：

```sh
# 基准（bench/*.tjs 内置 throw 自校验，TJS→Ciallang 交替执行取 min/mean）。
cmake -S . -B build/bench -DCMAKE_BUILD_TYPE=Release -DCIAL_BUILD_BENCH=ON
cmake --build build/bench --target bench

# TJS2 兼容性对拍报告（tests/test_files/tjs2 语料双引擎 stdout 语义比对）
cmake --build build/bench --target conformance-report
```

报告落盘 `bench/results/`。同机实测、统计口径与性能缺口见 [doc/performance.md](doc/performance.md)；不同机器和不同批次的历史比例不可直接比较。可选 JIT 与尾递归当前以受限子集的正确性为主，尚不能视为全程序性能追平。

---

### 未来计划

* ~~完整实现 TJS2 标准库~~（已完成：Date.parse 完整文法 / RegExp.last+码点位置 / Array·Dictionary save·load·pack 系）
* ~~字节码兼容~~（**已完成 M1–M4**：加载 TJS2 编译产物转译执行，`cial --tjsbc`；128 操作码转译覆盖，tjs2 语料全量通过，见 [doc/bytecode/mapping.md](doc/bytecode/mapping.md)）
* 性能追平原版（仍有差距；已完成的优化和后续方向见 [TODO.md](TODO.md)、[性能记录](doc/performance.md)）
* 扩展编译优化（CFG、跨块整数常量传播、死加载清理、比较/Test 融合和回边检测已接入；见 `src/gen/Optimizer.cpp`）
* **LSP 基础已接入**：语法诊断、词法补全、内置提示与文档符号，含 VSCode 客户端；IDEA/CLion 可接通用 stdio LSP 客户端，尚未提供 JetBrains 专用插件。跨文件语义补全与 IDE 交互验证待后续（[doc/lsp.md](doc/lsp.md)，`tools/lsp/`、`editors/vscode/`）

---

### 贡献

- clang-format格式化:
    - windows
        ```powershell
            Get-ChildItem -Path ./src, ./fuzz, ./tests -Recurse -File |  
            Where-Object { $_.Name -match '\.(cpp|cc|h|hpp|inc)$' } | 
            ForEach-Object { clang-format -i --verbose $_.FullName }
        ```
    - macos
        ```bash
            find ./src ./fuzz ./tests \( -name "*.cpp" -o -name "*.cc" -o -name "*.h" -o -name "*.hpp" -o -name "*.inc" \) \
            -type f -print0 | xargs -0 clang-format -i --verbose
        ```

---

### 📖 文档索引

- **[AGENTS.md](AGENTS.md)** — AI 协作规范与项目导航（目录地图/构建测试/代码风格/已知坑位），新成员与 AI 的第一入口
- **[doc/README.md](doc/README.md)** — 全部文档索引，含：
  - [架构设计](doc/architecture.md)（流水线/模块/与旧版 TJSCompiler 对照）
  - 字节码规范：[旧版 TJS2](doc/bytecode/tjs2-bytecode.md) · [本项目](doc/bytecode/ciallang-bytecode.md) · [转译设计](doc/bytecode/mapping.md)
  - [TJS2 语法参考与实现状态](doc/syntax.md) · [兼容性矩阵与路线图](doc/compatibility.md)
- **[TODO.md](TODO.md)** — 后续任务清单

---

**许可证**  
MIT License

---

### 构建基线与 CI

Windows 预设使用 `x64-windows-static` 与静态 CRT，测试无需复制依赖 DLL（`CMakePresets.json`）。
Release 配置若缺少优化标志会直接报错，避免污染性能数据（`CMakeLists.txt`）。
GitHub Actions 覆盖 Linux/macOS/Windows 的 Debug 与 Release，构建 CLI/测试并执行 CTest
，另有 Windows x64 Release 启用 JIT 的验证配置
（`.github/workflows/ci.yml`；新增配置尚未在远端运行）；本机验证结果与基准见 [TODO.md](TODO.md)。
