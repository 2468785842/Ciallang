# Ciallang CLI 预览版

本包提供 CLI 与构建的工具 exe（`bin/cial-lsp` 语言服务器、`bin/cial-disasm` 反汇编器、`bin/cial-dbg` 调试器、`bin/cial-fmt` 格式化器），以及 `share/ciallang/declarations/` 系统接口声明；C++ SDK 从源码另行安装。版本与构建信息见 `build-info.json`。

Windows 使用 `bin/cial.exe`；Linux/macOS 使用 `bin/cial`：

```text
bin/cial --version
bin/cial examples/hello.tjs
bin/cial --repl
bin/cial-fmt examples/hello.tjs
bin/cial-lsp --help
```

Windows 下程序名带 `.exe` 后缀。`cial-lsp` 从自身相对路径读取 `share/ciallang/declarations`（也可放到编辑器可发现的位置）。也可将 bin 目录加入 PATH。
程序不需要 vcpkg、源码目录或 Python；操作系统的基础运行库仍由系统提供。
Linux 的兼容性下限与构建环境有关，macOS 下限见构建信息。
JIT 是否编译启用见构建信息，预览版默认关闭。

功能与限制、源码、问题反馈：https://github.com/2468785842/Ciallang
主程序许可见 LICENSE，第三方许可与依赖版本资料见 licenses 目录。
