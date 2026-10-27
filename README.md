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
* [x] `var` / `const` 变量声明与使用（const 须初始化；赋值放行——对齐原版 tjs.y 设计现状）
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
* [x] `operator A` 前缀单目运算符 `! new invalidate isvalid - ~ typeof + & -- ++ delete # $ *`
* [x] `operator A` 类型转换运算符 `int real string`
* [x] `A operator` 后缀单目运算符 `() . -- ++`
* [x] `A operator` 后缀单目运算符 `[]`（数组/字典/字符串通用下标读写）
* [x] `A operator` 后缀单目运算符 `!`（即时求值：A 的字符串值作为 TJS 表达式编译执行；空串原值不变）
* [x] `A instanceof B` 实例判断运算符
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
5. 运行 TJS2 编译产物（字节码兼容，M1–M4 完成）：
   ```sh
   ./cial --tjsbc <compiled.tjsbc>
   ```
   加载 TJSCompiler 编译的 `"TJS2" "100\0"` 字节码文件，转译为内部指令后由同一
   VM 执行。kirikiri 官方语料 test_class / test_function / test_misc / test_variant /
   test_with 等全部通过（详见 [doc/bytecode/mapping.md](doc/bytecode/mapping.md)）。

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

报告落盘 `bench/results/`。当前与原版 TJS2（Release）wall-time 比值（2026-10-25，
本机多轮波动 ±10%）：
类实例化 ≈112%、整数循环 ≈121~127%、fib(25) ≈114~130%、字符串拼接 ≈151~155%。

---

### 未来计划

* ~~完整实现 TJS2 标准库~~（已完成：Date.parse 完整文法 / RegExp.last+码点位置 / Array·Dictionary save·load·pack 系）
* ~~字节码兼容~~（**已完成 M1–M4**：加载 TJS2 编译产物转译执行，`cial --tjsbc`；128 操作码转译覆盖，tjs2 语料全量通过，见 [doc/bytecode/mapping.md](doc/bytecode/mapping.md)）
* 性能追平原版（当前 112~155%，方向见 [TODO.md](TODO.md)：实参直写被调帧窗口、Value 内联 SSO、调用点内联缓存）
* 编译优化 Pass 接线（CFG/Optimizer 已建成未启用：LoadSub、常量传播、DCE）
* 集成 **LSP (Language Server Protocol)**，支持 VSCode / IDEA 智能提示

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
