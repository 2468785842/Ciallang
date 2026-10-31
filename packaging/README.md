# Ciallang CLI 预览版

本包提供 CLI，不包含稳定的 C++ SDK。版本与构建信息见 `build-info.json`。

Windows 使用 `bin/cial.exe`；Linux/macOS 使用 `bin/cial`：

```text
bin/cial --version
bin/cial examples/hello.tjs
bin/cial --repl
```

Windows 请将上述程序名替换为 `bin/cial.exe`。也可将 bin 目录加入 PATH。
程序不需要 vcpkg、源码目录或 Python；操作系统的基础运行库仍由系统提供。
Linux 的兼容性下限与构建环境有关，macOS 下限见构建信息。
JIT 是否编译启用见构建信息，预览版默认关闭。

功能与限制、源码、问题反馈：https://github.com/2468785842/Ciallang
主程序许可见 LICENSE，第三方许可与依赖版本资料见 licenses 目录。
