# Ciallang

> 一个用 C++20 编写的轻量级脚本语言解释器，专门用于支持 Kirikiri（吉里吉里）游戏引擎中的 TJS2 脚本语言。

<p align="center">
  <img src="https://img.shields.io/badge/language-C%2B%2B20-blue?logo=c%2B%2B" alt="C++20">
  <img src="https://img.shields.io/badge/build-CMake-green?logo=cmake" alt="CMake">
  <img src="https://img.shields.io/badge/license-MIT-yellow?logo=open-source-initiative" alt="MIT License">
</p>

## 🚧 开发进度

### 🔧 核心特性
  * [x] `GC` 引用计数 标记清除
  * [x] `computed goto` 高效跳转（Clang/GCC 支持）
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
* [x] `var` 变量声明与使用
* [x] `if`
* [x] `while`
* [x] `do while`
* [x] `for`
* [x] `switch`
* [ ] `try-catch`
* [x] `function` 函数定义与调用
* [x] `property`
* [x] `class` // 继承未实现
* [ ] `with`

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
* [ ] `super`
* [ ] `global`
* [ ] `this`
* [x] `function` 函数表达式声明
* [ ] `[]` Array 声明
* [ ] `%[]` Dictionary 声明
* [ ] `<% xx xx ... %>` Octet 声明

#### 表达式
* [x] `A,B` 顺序表达式
* [x] `A <-> B` 交换表达式
* [x] `A operator B` 赋值表达式 `=  &=  |=  ^=  -=  +=  %=  /=  \=  *=  ||=  &&=  >>=  <<=  >>>=`
* [x] `A ? B : C` 条件运算符
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
* [x] `operator A` 实现的前缀单目运算符 `! new -`
* [ ] `operator A` 前缀单目运算符 `~ -- ++ invalidate isvalid delete typeof # $ + & *`
* [ ] `operator A` 类型转换运算符 `int real string`
* [ ] `A operator` 后缀单目运算符 `() [] . -- ++ !`
* [ ] `A instanceof B` 实例判断运算符
* [ ] `A incontextof B` 上下文替换运算符

---

### 📚 标准库
* [ ] Exception
* [ ] Array
* [ ] Dictionary
* [ ] Date
* [ ] Math
* [ ] RegExp
* [x] String // replace, split, 未实现，需要先实现RegExp和Array

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
            Get-ChildItem -Path ./src, ./tests -Recurse -File |  
            Where-Object { $_.Name -match '\.(cpp|cc|h|hpp|inc)$' } | 
            ForEach-Object { clang-format -i --verbose $_.FullName }
        ```
    - macos
        ```bash
            find ./src ./tests \( -name "*.cpp" -o -name "*.cc" -o -name "*.h" -o -name "*.hpp" -o -name "*.inc" \) \
            -type f -print0 | xargs -0 clang-format -i --verbose
        ```

---

**许可证**  
MIT License

---
