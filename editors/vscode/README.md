# Ciallang / TJS2 VSCode 客户端

从源码调试：在本目录执行 `npm ci`，然后用 VSCode 打开本目录，按 F5。
Extension Development Host 会打开 Ciallang 工程根目录。打开 `.tjs` 文件即可启动语言服务。
客户端直接启动 `cial-lsp` 可执行文件，不依赖 Python（服务器实现见 `src/lsp/`，[LSP 文档](../../doc/lsp.md)）。

语法高亮在打开 `.tjs` 时直接生效，使用当前 VSCode 主题。覆盖关键字、类与函数声明、
字符串与转义、嵌套注释、数值、Octet、预处理指令，以及 `@"..."` / `@'...'` 中的
`${expr}`、`&expr;` 插值。支持括号匹配、引号与括号自动补全，以及注释快捷键。
这些编辑功能不依赖语言服务；正则与除法使用局部上下文保守区分，不提供语义着色。
词法依据见 [语法说明](../../doc/syntax.md)，规则位于 `syntaxes/ciallang.tmLanguage.json`。

诊断、补全、悬停与跳转需要已构建的 `cial-lsp`（配置预设含 `CIAL_BUILD_LSP`；工具 exe 位于
`build/<平台>/<配置>/tools/lsp/`）。默认从活动文件目录、工作区及其父目录中查找
`build/<Windows|MacOS|Linux>/<Debug|Release>/cial-lsp`（含 `tools/lsp` 子目录布局），
没有找到时尝试 PATH 中的 `cial-lsp`。因此打开 `tests/tjs` 等工程子目录也可使用。

也可在 VSCode 设置中显式指定：

```json
{
  "ciallang.executable": "C:/projects/Ciallang/build/Windows/Debug/tools/lsp/cial-lsp.exe",
  "ciallang.declarationFiles": ["types/host.d.tjs"]
}
```

请替换为本机实际路径，包含空格的路径不用再添加引号字符。相对路径基于活动文件所属
工作区目录解析；没有工作区时基于活动文件目录。PATH 命令名也可直接配置。
修改配置后运行 VSCode 的 `Developer: Reload Window` 再打开 `.tjs`。

系统接口（Array/String/Math 等）的签名与文档来自构建期生成的 `.d.tjs`
（`build/<平台>/<配置>/declarations/`，随 `cial-lsp`/`cial` 自动发现）；工作区 `.d.tjs`
自动纳入，`ciallang.declarationFiles` 可追加宿主 SDK 声明。

启动失败时检查弹出的错误和 Output 面板中的 Ciallang 输出：

- 找不到 `cial-lsp`：构建 cial-lsp（预设含 `CIAL_BUILD_LSP`）或设置 `ciallang.executable`。
- 补全/跳转没有系统接口：确认 `cial` 构建成功——`stdlib.d.tjs` 等声明在 cial 链接后
  由 POST_BUILD 生成到其旁的 `declarations/`。

验证命令：

```sh
npm run check
npm test
```

`npm test` 使用 TextMate/Oniguruma 引擎验证高亮分词及服务器启动路径解析。
`test/smoke.js` 在真实 VSCode 扩展宿主中验证语言配置、启动、编译器诊断、补全与定义跳转。
它会覆盖工作区的 `smoke.tjs`，须使用独立的临时工作区；启动参数见
[宿主验收](../../doc/lsp.md#vscode-宿主验收)。
