# Ciallang

> 一个用 C++20 编写的轻量级脚本语言解释器，专门用于支持 Kirikiri（吉里吉里）游戏引擎中的 TJS2 脚本语言。

<p align="center">
  <img src="https://img.shields.io/badge/language-C%2B%2B20-blue?logo=c%2B%2B" alt="C++20">
  <img src="https://img.shields.io/badge/build-CMake-green?logo=cmake" alt="CMake">
  <img src="https://img.shields.io/badge/license-MIT-yellow?logo=open-source-initiative" alt="MIT License">
</p>

## 🚧 开发进度

### 🔧 核心特性
  * [ ] `JIT` 即时编译优化
  * [ ] `CFG` 控制流图生成
  * [ ] `SSA` 静态单赋值构造
  * [ ] `常量传播` 与块间状态分析
  * [ ] `死代码消除`
  * [ ] `循环优化`（回边检测）
  * [ ] `尾递归消除`
  * [ ] `Trace JIT` 热路径跟踪
  * [ ] `可视化调试`（控制流/执行路径分析）

---

### 🧩 语言特性

#### 预处理

* [x] `@set`
* [x] `@if` `@endif`

#### 语法结构
* [x] `var` / `const` 变量声明与使用（const 只读性检查未实现，当前与 var 等价）
* [x] `if`
* [x] `while`
* [x] `do while`
* [x] `for`（含 `for(const)`）
* [x] `switch`
* [x] `try-catch`（含 `catch()` 空参数列表）
* [x] `function` 函数定义与调用（块内提升，先调用后声明）
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
* [x] `operator A` 前缀单目运算符 `! new invalidate isvalid -`
* [x] `operator A` 前缀单目运算符 `~ typeof + &`
* [x] `operator A` 前缀单目运算符 `-- ++ delete # $ * &`
* [x] `operator A` 类型转换运算符 `int real string`
* [x] `A operator` 后缀单目运算符 `() . -- ++`
* [x] `A operator` 后缀单目运算符 `[]`（数组/字典/字符串通用下标读写）
* [ ] `A operator` 后缀单目运算符 `!`
* [x] `A instanceof B` 实例判断运算符
* [x] `A incontextof B` 上下文替换运算符

---

### 📚 标准库
* [x] Exception // message 成员；原生错误自动包装为 Exception 实例
* [ ] Array // ◐ 字面量/下标读写/push/pop/unshift/shift/insert/remove/erase/reverse/join/split/sort(含比较器回调)/count；save/load/pack 未实现
* [ ] Dictionary // ◐ 字面量/下标读写/assign/clear/erase/count；save/load 未实现
* [ ] Date
* [ ] Math // ◐ abs/acos/asin/atan/atan2/ceil/exp/floor/log/pow/max/min/random/round/sin/cos/sqrt/tan + E/PI/LN2/... 常量；RandomGenerator 未实现
* [ ] RegExp // ◐ /pattern/flags 字面量 + test/match/exec/compile/replace(含函数回调)/split + 匹配状态属性（start/index/lastIndex/matches/lastMatch/...）；new RegExp 构造器未实现
* [x] String // charAt/indexOf/toUpperCase/toLowerCase/substring/sprintf/replace(g 全局)/split/escape/trim/reverse/repeat/length

## 编译与运行

1. 克隆项目到本地：
   ```sh
   git clone https://github.com/2468785842/Ciallang.git
   ```
2. 安装vcpkg设置VCPKG_ROOT变量

3. 使用 CMake 编译：
    - Windows
       ```sh
       cmake --preset="Windows Debug"
       cmake --build --preset="Windows Debug"
       ```
    - Linux
       ```sh
       cmake --preset="Linux Debug"
       cmake --build --preset="Linux Debug"
       ```
4. 运行解释器：
   ```sh
   ./cial <script-file>
   ```

### ⚡ 基准与 TJS2 对拍

内置双引擎基准与兼容性对拍设施（TJSCompiler 参考实现路径经
`CMakeUserPresets.json` 的 `CIAL_TJS_ROOT` 注入，或兄弟目录自动探测）：

```sh
# 基准（bench/*.tjs 内置 throw 自校验，TJS→Ciallang 交替执行取 min/mean）。
# 本机预设 "Windows Local Bench" 定义在 CMakeUserPresets.json（不入库）；
# 也可手动：cmake -B build/Windows/Bench -DCIAL_BUILD_BENCH=ON 后构建
cmake --preset "Windows Local Bench" && cmake --build --preset "Windows Local Bench"

# TJS2 兼容性对拍报告（tests/test_files/tjs2 语料双引擎 stdout 语义比对）
cmake --build build/Windows/Bench --target conformance-report
```

报告落盘 `bench/results/`。当前与原版 TJS2（Release）wall-time 比值：
类实例化 ≈110%、整数循环 ≈121%、fib(25) ≈127%、字符串拼接 ≈185%（2026-10-24）。

---

### 未来计划

* 提升解释器性能与调试可视化能力
* 集成 **LSP (Language Server Protocol)**，支持 VSCode / IDEA 智能提示
* 完整实现 **TJS2 标准库**
* 增加更多编译优化 Pass（如 DCE、常量传播、循环展开）

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
