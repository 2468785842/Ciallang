# LSP 与编辑器接入

`tools/lsp/cial_lsp.py` 使用 Python 3 标准库提供 stdio LSP：完整文本同步、实时语法/编译诊断、关键字与当前文档声明补全、内置关键字悬停说明，以及词法级文档符号列表。诊断由 `cial --check-json file.tjs` 返回，检查过程不执行被检查脚本；位置从原生 UTF-8 字节列转换成 LSP UTF-16 列（`src/cli/Application.cpp`、`src/cli/Compilation.cpp`、`src/common/SourceFile.cpp`、`tools/lsp/cial_lsp.py`）。

启动命令（将路径替换为实际绝对路径）：

```sh
python tools/lsp/cial_lsp.py --cial build/Windows/Debug/cial.exe
```

这里展示的是命令结构。客户端中应为 Python、服务器脚本及 cial 配置绝对路径，避免工作目录变化。Linux/macOS 通常使用 `python3`，cial 没有 `.exe` 后缀。stdout 仅承载带 Content-Length 的 JSON-RPC 消息，不能把普通 CLI 输出混入该通道（`tools/lsp/cial_lsp.py`）。

## VSCode

`editors/vscode/` 包含使用 `vscode-languageclient` 的客户端。进入该目录运行 `npm ci`、`npm run prepare`，用 VSCode 打开该目录后按 F5 启动 Extension Development Host，再打开 Ciallang 工作区中的 `.tjs` 文件。`prepare.js` 把服务器脚本复制进扩展目录，确保打包时不依赖仓库外的相对路径（`editors/vscode/package.json`、`extension.js`、`prepare.js`、`.vscode/launch.json`）。

配置项 `ciallang.python` 指定 Python，`ciallang.executable` 指定已构建的 cial。未显式配置时，客户端根据平台尝试当前工作区 `build/<平台>/Debug/cial`。服务器或解释器缺失时会发布明确的检查器不可用诊断（`editors/vscode/extension.js`、`tools/lsp/cial_lsp.py`）。

## IDEA / CLion 等客户端

在支持自定义 stdio Language Server 的客户端或插件中，注册上述启动命令，把 `.tjs` 关联到该服务器即可。仓库提供通用 LSP 服务，未捆绑 JetBrains 插件，也未把该服务器注册进用户的 IDE 设置（`tools/lsp/cial_lsp.py`）。

## 范围与验证

这是基础语言服务，不包含类型推导、跨文件符号解析、重命名、跳转定义或语义级成员补全；声明补全与符号列表基于词法模式，可能包含注释/字符串中的类似声明。每次完整文本更新触发独立的编译检查，单次超时 5 秒，诊断文档上限 2 MB；关闭文档清空诊断，过期版本不会覆盖新内容（`tools/lsp/cial_lsp.py`）。

`tools/lsp/test_protocol.py` 通过真正的子进程 stdio 测试初始化、诊断与修复、关闭、补全、悬停、符号、未知请求、shutdown/exit，以及 Unicode 位置转换；Python 可用时由 CTest 注册。VSCode 客户端已做依赖安装与 JavaScript 语法检查，图形编辑器中的交互仍需在目标 IDE 实测（`tests/CMakeLists.txt`）。

协议参考：[LSP 3.17](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/)。
