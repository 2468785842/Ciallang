# Ciallang

目前是个玩具

Ciallang 是一个用 C++20 编写的轻量级脚本语言解释器，专门用于支持 Kirikiri（吉里吉里）游戏引擎中的 TJS2 脚本语言。目前，Ciallang 实现了以下语言特性：

## TODO List

- 特性

  * [ ] `GC` 分代垃圾回收器(标记清除和复制)

- 语法

   * [ ] `class` 
   * [ ] `array`
   * [x] `if` 条件判断
   * [x] `while` 循环
   * [x] `do while` 循环
   * [x] `function` 函数定义与调用
   * [x] `var` 变量声明与使用

- 数据类型
   * [x] `real` 小数类型


## 编译与运行


1. 克隆项目到本地：
   ```sh
   git clone https://github.com/2468785842/Ciallang.git
   ```

2. 使用 CMake 编译：
   ```sh
   TODO
   ```

3. 运行解释器：
   ```sh
   ./Ciallang <script-file>
   ```

---

### FAQ

#### 如何在 VSCode 中配置 clangd 插件以避免错误代码分析？

在使用 `clangd` 插件时，有时它可能会错误地将正常代码标记为错误。为了解决这个问题，你需要在 `config.yaml` 文件中添加以下代码：

```yaml
CompileFlags:
  Add: -ferror-limit=0
```

此配置会移除 `clangd` 对错误数量的限制，从而避免一些不必要的错误提示。

### 未来计划

- 提高性能并完善调试工具。
- 添加LSP语言服务协议, 以支持IDEA, VSCode的代码提示, 等..
- 实现完整的 TJS2 标准库。

---

### 贡献

欢迎任何形式的贡献！如果你对这个项目感兴趣，请提交 PR 或者报告问题。

---

**许可证**  
MIT License

---
