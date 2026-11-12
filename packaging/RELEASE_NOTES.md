这是 Ciallang 的第二个预览版，在首个预览版的基础上交付 C++ 语言服务器与可分发的系统接口声明。

## 2026-11-12 同版本修订

- VSCode 插件更新为 0.2.1：缺少 SDK 时提供下载/设置通知，使用新的缓存目录以替换原 preview.2 缓存。
- 修复真实工程目录下 Array.push 等声明跳转；增加常驻文件、符号和声明索引及文件变更更新。
- Exception 类引用仅跳转到类声明；限定类型引用按完整名称解析。
- 声明支持联合类型、限定类型和数组元素类型，补齐标准库参数、返回值、异常与示例文档。
- SDK 声明包新增工程规范和文件 SHA-256 清单。真正动态的值保留 any；完整静态检查仍不支持。

## 本版提供

- **C++ 语言服务器 `cial-lsp`**（新增，随发行包提供）：stdio LSP 提供实时编译/声明诊断、关键字与文档补全、基于保守类型推导的成员补全、悬停、跳转定义与文档/工作区符号。诊断与声明解析在进程内完成；VSCode 扩展（`editors/vscode`）直接启动该服务器，不再依赖 Python。绑定级重命名尚未移植。
- **VSCode 扩展**：提供可安装的 `.vsix`，缺少本地语言服务器时通知下载匹配版本的工具 SDK，校验 SHA-256 并引用 `bin/cial-lsp.exe`；支持离线 SDK 路径与缓存复用。
- **系统接口声明随包分发**：构建期按顶层符号生成 `.d.tjs`（`Array.d.tjs`、`Math.d.tjs`、`globals.d.tjs` 等，CLI 新增 `--api-decl-dir`），CLI 压缩包与 SDK 安装均包含 `share/ciallang/declarations/`；脚本工具直接读取声明文件获得签名与文档，无需查看 C++ 源码。
- **标准库具名签名与文档**：Math、Date、Dictionary、RegExp、Array、String、Octet、RandomGenerator、Scripts 的方法登记具名参数、返回类型与 `/** */` 文档，类级文档随声明导出。
- **宿主 SDK 声明导出**：注册的类可附类型级文档（`ApiCatalog` typeDocs），可选能力库按构建开关门控注册——关闭的库不出现在声明产物中。
- CLI、REPL、JSON 编译诊断、TJS2 字节码加载与转译、源码 SDK 安装等首版能力延续，兼容范围见[兼容性说明](https://github.com/2468785842/Ciallang/blob/v0.1.0-preview.2/doc/compatibility.md)。

## 下载与运行

按系统和架构选择附件，使用 `SHA256SUMS` 或对应 `.sha256` 校验后解压：

```sh
bin/cial --version
bin/cial examples/hello.tjs
bin/cial --repl
```

Windows 将程序名替换为 `bin/cial.exe`。程序运行不需要源码、vcpkg 或 Python。
包内 `bin/` 还包含 `cial-lsp`（语言服务器）、`cial-disasm`、`cial-dbg` 与 `cial-fmt`；
`build-info.json` 记录提交、系统/架构、编译器、运行库及功能开关，`licenses/` 包含第三方许可与依赖资料，`share/ciallang/declarations/` 为系统接口声明（`cial-lsp` 按自身相对路径读取）。
本版关闭可选 JIT。Linux 包使用 Ubuntu 24.04 构建；macOS 部署下限为 15.6，均只面向包名所列架构，系统基础运行库仍由操作系统提供。

## 已知限制

- 语言服务器的绑定级重命名、声明/实现一致性检查与完整静态类型检查尚未实现；范围见[LSP 文档](https://github.com/2468785842/Ciallang/blob/v0.1.0-preview.2/doc/lsp.md)。
- 尚不能直接替换 krkr2 的 TJS2 核心：双向对象桥、TVP/XP3 接入及真实游戏验收仍未完成，见[接入计划](https://github.com/2468785842/Ciallang/blob/v0.1.0-preview.2/doc/krkr2-integration.md)。
- 字节码 proxy、缺失成员 typeof、动态类及部分 Unicode 边界仍有缺口；模块系统和 DAP 调试器尚未实现。

普通 CI 覆盖三平台 Debug/Release、Windows JIT、LSP 协议回归与 Release SDK 安装消费测试。
发布流程另在各平台执行构建、全部 CTest、打包、独立目录解压运行及校验和核验，全部通过后才生成 Release 草稿。
