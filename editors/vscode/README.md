# Ciallang / TJS2 VSCode 客户端

从源码调试：在本目录执行 `npm ci`，然后用 VSCode 打开本目录，按 F5。
Extension Development Host 会打开 Ciallang 工程根目录。打开 `.tjs` 文件即可启动语言服务。
客户端直接启动 `cial-lsp` 可执行文件，不依赖 Python（服务器实现见 `src/lsp/`，[LSP 文档](../../doc/lsp.md)）。

语法高亮在打开 `.tjs` 时直接生效，使用当前 VSCode 主题。覆盖关键字、类与函数声明、
字符串与转义、嵌套注释、数值、Octet、预处理指令，以及 `@"..."` / `@'...'` 中的
`${expr}`、`&expr;` 插值。支持括号匹配、引号与括号自动补全，以及注释快捷键。
这些编辑功能不依赖语言服务；正则与除法使用局部上下文保守区分，不提供语义着色。
词法依据见 [语法说明](../../doc/syntax.md)，规则位于 `syntaxes/ciallang.tmLanguage.json`。

安装发布附件 `ciallang-language-0.2.0.vsix` 后打开 `.tjs` 即可使用。首次找不到语言服务器时，
插件自动从官方 GitHub Release 下载指定版本的 Cial 工具 SDK，校验同名 `.sha256`，
解压到 VSCode 全局存储并启动 `bin/cial-lsp.exe`（Linux/macOS 为 `bin/cial-lsp`）。
系统声明通过 SDK 的 `share/ciallang/declarations/` 自动加载，无需手动配置路径。
下载完成后可离线复用；下载或校验失败不会留下半成品，修复网络后重新加载窗口即可重试。
工具 SDK 是 CLI 发布包（含工具和声明）；C++ 开发库仍按源码 SDK 安装流程提供。

查找顺序为：`ciallang.executable` → `ciallang.sdk.path` → 活动文件/工作区祖先中的
`build/<平台>/<Debug|Release>/[tools/lsp/]cial-lsp` → PATH → 自动下载的 SDK。
显式路径配置错误时直接报告，不会改用其他服务器。SDK 缓存按版本、系统、架构隔离。
默认版本为 `0.1.0-preview.2`；仅在对应 Release 附件公开后可下载，旧 `preview.1` 不含所需 LSP。
Windows x64、Linux x64、macOS ARM64 对应当前发布矩阵；其他架构需要自行构建并配置路径。
Linux/macOS 的系统下限与运行库要求见[发布说明](../../packaging/RELEASE_NOTES.md)。
未受信任的工作区不启动扩展。下载使用 HTTPS；离线或网络限制环境可手动下载、校验并解压 SDK。

也可在 VSCode 设置中显式指定：

```json
{
  "ciallang.sdk.path": "C:/tools/ciallang-0.1.0-preview.2-windows-x64",
  "ciallang.sdk.autoDownload": true,
  "ciallang.sdk.version": "0.1.0-preview.2",
  "ciallang.declarationFiles": ["types/host.d.tjs"]
}
```

`ciallang.sdk.path` 可留空以使用自动安装；`ciallang.executable` 可覆盖单独的 LSP 路径。
`ciallang.sdk.autoDownload` 设为 `false` 可关闭联网下载。请替换为本机实际路径，包含空格的路径不用再添加引号字符。相对路径基于活动文件所属
工作区目录解析；没有工作区时基于活动文件目录。PATH 命令名也可直接配置。
修改配置后运行 VSCode 的 `Developer: Reload Window` 再打开 `.tjs`。

系统接口（Array/String/Math 等）的签名与文档来自构建期生成的 `.d.tjs`
（`build/<平台>/<配置>/declarations/`，随 `cial-lsp`/`cial` 自动发现）；工作区 `.d.tjs`
自动纳入，`ciallang.declarationFiles` 可追加宿主 SDK 声明。

启动失败时检查弹出的错误和 Output 面板中的 Ciallang 输出：

- 找不到 `cial-lsp`：检查 SDK 版本的 Release 附件与网络，或设置 `ciallang.sdk.path` / `ciallang.executable`。
- 补全/跳转没有系统接口：确认 `cial` 构建成功——`stdlib.d.tjs` 等声明在 cial 链接后
  由 POST_BUILD 生成到其旁的 `declarations/`。

验证命令：

```sh
npm run check
npm test
```

`npm test` 验证高亮分词、服务器发现、SDK ZIP/TAR.GZ 安装、校验与缓存恢复。
`npm run package` 生成 `build/packages/*.vsix`（先创建该目录）；CI 与 Release 流程自动执行。
`test/smoke.js` 在真实 VSCode 扩展宿主中验证语言配置、启动、编译器诊断、补全与定义跳转。
它会覆盖工作区的 `smoke.tjs`，须使用独立的临时工作区；启动参数见
[宿主验收](../../doc/lsp.md#vscode-宿主验收)。
