# LSP 与编辑器接入

`tools/lsp/cial_lsp.py` 使用 Python 3 标准库提供 stdio LSP：完整文本同步、实时语法/编译诊断、关键字与当前文档声明补全、内置关键字悬停说明、词法级文档符号列表，以及跨文件工作区符号搜索。诊断由 `cial --check-json file.tjs` 返回，检查过程不执行被检查脚本；位置从原生 UTF-8 字节列转换成 LSP UTF-16 列（`src/cli/Application.cpp`、`src/cli/Compilation.cpp`、`src/common/SourceFile.cpp`、`tools/lsp/cial_lsp.py`）。

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

`workspace/symbol` 支持在工作区所有 `.tjs` 文件中按名称子串搜索函数、类、属性和变量声明，忽略大小写，结果包含文件 URI 与 UTF-16 名称范围。支持该请求的客户端可显示全局符号搜索入口，例如 VSCode 的“转到工作区中的符号”。初始化读取 `workspaceFolders`，兼容 `rootUri` / `rootPath`；`workspace/didChangeWorkspaceFolders` 可更新目录集合。没有工作区目录时只搜索已打开的文件，不自行扫描当前进程目录（`tools/lsp/cial_lsp.py` 的 `workspace_symbols`、`update_workspace_folders`）。

未打开文件按磁盘内容索引，查询时检查文件时间与大小并缓存声明；未保存缓冲优先覆盖磁盘内容，关闭后恢复磁盘版本，删除文件会移出索引。扫描不跟随目录符号链接，并排除解析后位于工作区之外的文件链接；跳过 build、bin、ext、node_modules 以及常见编辑器/缓存目录。单文件上限 2 MB，每次查询最多检查 2000 个 `.tjs` 候选文件和 10000 个目录，源内容预算 20 MB，最多返回排序后的 200 个匹配；可收窄名称继续查找。无法读取、编码非法或超限的文件会跳过（`tools/lsp/cial_lsp.py` 的 `MAX_*`、`EXCLUDED_DIRECTORIES`、`workspace_symbols`）。

这仍是基础语言服务，不包含类型推导、作用域感知的跨文件名称解析、重命名、跳转定义或语义级成员补全。声明提取会隐藏注释、字符串和常规正则字面量，再匹配声明；不会求值预处理条件、解析模板插值或建立作用域绑定，同名局部声明可能产生多个搜索结果。每次完整文本更新触发独立的编译检查，单次超时 5 秒，诊断文档上限 2 MB；关闭文档清空诊断，过期版本不会覆盖新内容（`tools/lsp/cial_lsp.py` 的 `declarations`、`check`、`handle`）。

`tools/lsp/test_protocol.py` 通过真正的子进程 stdio 测试初始化、诊断与修复、关闭、补全、悬停、符号、未知请求、shutdown/exit，以及 Unicode 位置转换；另外覆盖未打开文件的工作区搜索、磁盘修改/删除、未保存缓冲更新与关闭恢复、工作区目录切换、结果数量限制、超限/编码非法文件跳过，以及带空格、百分号和中文的 URI。Python 可用时由 CTest 注册。VSCode 客户端已做依赖安装与 JavaScript 语法检查，图形编辑器中的交互仍需在目标 IDE 实测（`tools/lsp/test_protocol.py`、`tests/CMakeLists.txt`）。

协议参考：[LSP 3.17](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/)。
