# Ciallang / TJS2 VSCode 客户端

从源码调试：在本目录执行 `npm ci`，然后用 VSCode 打开本目录，按 F5。
启动前会执行 `npm run prepare` 更新内置 Python 服务脚本；新的 Extension Development Host
会打开 Ciallang 工程根目录。打开 `.tjs` 文件即可启动语言服务。

语法高亮在打开 `.tjs` 时直接生效，使用当前 VSCode 主题。覆盖关键字、类与函数声明、
字符串与转义、嵌套注释、数值、Octet、预处理指令，以及 `@"..."` / `@'...'` 中的
`${expr}`、`&expr;` 插值。支持括号匹配、引号与括号自动补全，以及注释快捷键。
这些编辑功能不依赖 Python 或 cial；正则与除法使用局部上下文保守区分，不提供语义着色。
词法依据见 [语法说明](../../doc/syntax.md)，规则位于 `syntaxes/ciallang.tmLanguage.json`。

诊断、补全、跳转和重命名需要 Python 3 和已构建的 cial。默认从活动文件目录、工作区及其父目录中查找
`build/<Windows|MacOS|Linux>/Debug/cial`，同级 Debug 不存在时尝试 Release；Windows 文件名为
`cial.exe`。没有找到构建时尝试 PATH 中的 `cial`。因此打开 `tests/tjs` 等工程子目录也可使用。

也可在 VSCode 设置中显式指定：

```json
{
  "ciallang.python": "C:/tools/Python/python.exe",
  "ciallang.executable": "C:/projects/Ciallang/build/Windows/Debug/cial.exe"
}
```

请替换为本机实际路径，包含空格的路径不用再添加引号字符。相对路径基于活动文件所属
工作区目录解析；没有工作区时基于活动文件目录。PATH 命令名也可直接配置。
修改配置后运行 VSCode 的 `Developer: Reload Window` 再打开 `.tjs`。

启动失败时检查弹出的错误和 Output 面板中的 Ciallang 输出：

- 找不到 Python：安装 Python 3 或设置 `ciallang.python`；修改系统 PATH 后重启 VSCode。
- 缺少 `server/cial_lsp.py`：源码环境执行 `npm run prepare`；安装版重新安装扩展。
- 文档提示 `Syntax checker unavailable`：确认 cial 已构建，并检查 `ciallang.executable`。
- 缺少 `vscode-languageclient/node`：在扩展源码目录执行 `npm ci`，再重新启动 F5 调试。

验证命令：

```sh
npm run check
npm test
```

`npm test` 使用 TextMate/Oniguruma 引擎验证高亮分词及启动路径解析。
`test/smoke.js` 在真实 VSCode 扩展宿主中验证语言配置、启动、编译器诊断与修复、补全、定义跳转和
重命名。它会覆盖工作区的 `smoke.tjs`，须使用独立的临时工作区；启动参数见
[宿主验收](../../doc/lsp.md#vscode-宿主验收)。完整功能和边界见 [LSP 文档](../../doc/lsp.md)。
