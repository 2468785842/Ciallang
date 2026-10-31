# Ciallang

[![CI](https://github.com/2468785842/Ciallang/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/2468785842/Ciallang/actions/workflows/ci.yml)
[![C++20](doc/assets/cpp20.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](doc/assets/cmake.svg)](doc/building.md)
[![MIT](doc/assets/license.svg)](LICENSE)

用 C++20 实现的 TJS2 脚本语言解释器，面向 Kirikiri（吉里吉里）脚本兼容与宿主嵌入。
支持 Linux、macOS 和 Windows。

## 能做什么

- 执行 TJS2 源码：函数、类、属性、异常、预处理、变长参数与常用字面量。
- 提供 Array、Dictionary、String、Math、Date、RegExp、Exception 和 Octet 标准库能力。
- 加载 TJS2 字节码，转译为内部指令执行；兼容范围见[兼容性说明](doc/compatibility.md)。
- 提供 CLI、REPL、LSP 与 VSCode 客户端，以及控制流和执行路径导出。
- 作为 CMake 库嵌入，通过 C++ API 注册宿主类、函数和属性。

整数 JIT、Trace JIT 与尾递归优化需显式启用，适用范围见 [JIT](doc/jit.md) 和[尾递归](doc/tail-recursion.md)。

## 快速开始

准备 CMake 3.25+、C++20 编译器、Ninja 和 [vcpkg](https://github.com/microsoft/vcpkg)，
并设置 `VCPKG_ROOT`。Linux 预设使用 Clang。

```sh
git clone https://github.com/2468785842/Ciallang.git
cd Ciallang
cmake --preset "Linux Debug"
cmake --build --preset "Linux Debug"
ctest --preset "Linux Debug"
./build/Linux/Debug/cial --repl
```

macOS 和 Windows 分别使用 `MacOS Debug`、`Windows Debug` 预设；Windows 可执行文件带 `.exe`。
依赖、库模式与可选构建项见[构建指南](doc/building.md)。

```tjs
function greet(name) { return "Hello, " + name; }
println(greet("Ciallang"));
println([1, 2, 3].join(", "));
```

保存为 `hello.tjs` 后运行：

```sh
./build/Linux/Debug/cial hello.tjs
./build/Linux/Debug/cial --tjsbc program.tjsbc
```

## 文档与开发

| 入口 | 内容 |
|---|---|
| [命令行](doc/cli.md) · [宿主集成](doc/embedding.md) · [LSP](doc/lsp.md) | 使用与集成 |
| [语法](doc/syntax.md) · [兼容性](doc/compatibility.md) · [FAQ](doc/faq.md) | 语言支持与常见问题 |
| [架构](doc/architecture.md) · [性能记录](doc/performance.md) | 实现与测量依据 |
| [文档索引](doc/README.md) · [待办](TODO.md) · [开发约定](AGENTS.md) | 查阅与贡献 |

提交前运行相关测试，并按 `.clang-format` 格式化修改的 C++ 文件。
CI 覆盖三平台 Debug/Release，另有 Windows x64 Release + JIT 配置。

采用 [MIT License](LICENSE)。
