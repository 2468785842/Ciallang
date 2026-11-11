# LSP 与编辑器接入

`cial-lsp`（实现库 `src/lsp/`，入口 `tools/lsp/main.cpp`）是核心集 stdio 语言服务器：完整文本同步、
进程内实时编译/声明诊断（不派生子进程）、关键字与当前文档声明补全、基于保守类型推导的语义成员
补全、`textDocument/definition` 跳转（本文件作用域解析 + 跨文件顶层声明 + 声明条目）、纯文本悬停
（签名 + 声明文档注释）、词法文档符号与跨文件工作区符号搜索。诊断编译直接链接 `cialcli` 的
`cli::compile`（`src/lsp/Server.cpp`），检查过程不执行被检查脚本；位置从原生 UTF-8 字节列按完整
字符边界转换成 LSP UTF-16 列（`src/common/UTF16.hpp`）。声明解析经 `decl::parseDeclarations`
进程内完成（`src/lsp/Declarations.cpp`），不再调用 `--decl-json` 子进程。

## 系统声明（.d.tjs）与服务端边界

服务端不内置任何 API 事实：关键字说明只覆盖纯语言关键字；接口签名与文档一律来自 `.d.tjs` 的
`/** */` 注释——

- 构建期生成在可执行文件旁的 `declarations/`（每类型一个文件 + `globals.d.tjs` + 手写
  `script-builtins.d.tjs`，生成方式见 [声明文件](declarations.md) §3）；
- 安装布局 `<prefix>/share/ciallang/declarations/`（SDK 与 CLI 归档随装）；
- `initializationOptions.declarationFiles` 显式文件（外部宿主 SDK）；
- 工作区发现的 `.d.tjs`（未保存缓冲优先，关闭恢复磁盘）。

同名项目声明覆盖对应系统声明；无效声明整文件不提供接口，避免把残缺类型当作事实。
成员补全的接收者类型经声明返回类型、脚本显式类型注解与 `new`/字面量/拷贝链形态保守推导
（预算内深度 ≤ 3，循环/不一致/超限一律 unknown）；未知类型宁可空，不猜成员。
声明文件修改（缓冲或磁盘）会重新解析并更新条目。

启动命令（路径替换为实际绝对路径）：

```sh
build/Windows/Debug/tools/lsp/cial-lsp.exe [--declarations <dir>]
```

stdout 仅承载带 Content-Length 的 JSON-RPC 帧：Windows 下 stdin/stdout 显式切二进制模式，
避免文本模式把 `\n` 再译成 `\r\n` 破坏帧（`tools/lsp/main.cpp`）。

## VSCode

`editors/vscode/` 包含使用 `vscode-languageclient` 的客户端。进入该目录运行 `npm ci`，用 VSCode
打开该目录后按 F5；Extension Development Host 会打开工程根目录，再打开 `.tjs` 文件。客户端
直接启动 `cial-lsp` 可执行文件，不依赖 Python（`editors/vscode/extension.js`、`launch-options.js`）。

扩展通过 TextMate 文法提供关键字、字符串、嵌套注释、数值、Octet、正则与预处理高亮，
并识别模板串中的 `${expr}` / `&expr;` 插值；颜色由当前主题决定，基础高亮不依赖语言服务。
括号匹配、引号与括号自动闭合、注释快捷键由 `language-configuration.json` 注册。
词法规则依据 `src/parser/Token.hpp`、`Lexer.cpp` 与 [语法说明](syntax.md)；
实现见 `editors/vscode/syntaxes/ciallang.tmLanguage.json`，分词回归见 `test/grammar.test.js`。
正则/除法按局部上下文识别，不等价于解析器的完整上下文判定。

配置项 `ciallang.executable` 指定 `cial-lsp` 路径。未显式配置时，客户端根据活动文件和工作区向
父目录查找 `build/<平台>/Debug|Release/cial-lsp`（含 `tools/lsp` 子目录布局），最后回落 PATH。
`ciallang.declarationFiles` 可显式指定工作区之外的 `.d.tjs`（相对路径按活动工作区解析）；
修改配置后重新加载 VSCode 窗口。服务缺失或启动失败时弹出错误并提供设置入口。
源码调试及排错见[客户端说明](../editors/vscode/README.md)。

## IDEA / CLion 等客户端

在支持自定义 stdio Language Server 的客户端或插件中，注册上述启动命令，把 `.tjs` 关联到该
服务器即可。仓库提供通用 LSP 服务，未捆绑 JetBrains 插件，也未把该服务器注册进用户的 IDE 设置。

## 范围与验证

`workspace/symbol` 在工作区 `.tjs` 文件中按名称子串搜索函数、类、属性和变量声明，忽略大小写，
结果含文件 URI 与 UTF-16 名称范围。初始化读取 `workspaceFolders`；
`workspace/didChangeWorkspaceFolders` 可更新目录集合。没有工作区时只搜索已打开的文件。
未打开文件按磁盘内容索引；扫描不跟随目录符号链接，跳过 build、bin、ext、node_modules 以及常见
编辑器/缓存目录。单文件上限 2 MB，每次查询最多检查 2000 个候选文件和 10000 个目录，源内容预算
20 MB，最多返回排序后的 200 个匹配；可收窄名称继续查找。无法读取、编码非法或超限的文件跳过
（`src/lsp/Server.cpp` 的 `MAX_*`、`excludedDirectories`、`workspaceSources`）。

诊断在后台线程执行（150ms 防抖合并快速输入），只保留每个文档的最新版本，编译不阻塞后续请求；
stdio 帧经互斥锁有序写出。每次完整文本更新触发一次编译检查；关闭文档清空诊断并丢弃待检请求。
诊断编译与声明解析共用同一个进程内会话（每次检查新建 Runtime/Context，不注册标准库，
与 `--check-json` 的 compileOnly 路径一致）；`.d.tjs` 文档走声明模式诊断（`decl.*` 码）。

文件身份按解析后的真实路径匹配；已打开文件保留客户端原始 URI，未打开文件使用真实路径生成的
URI。协议测试使用含空格、百分号和中文的临时目录（`tests/cli/test_lsp.py`）。

### 保守类型推导与语义成员补全

`textDocument/completion` 在 `receiver.` 形态做类型推导：`DocumentIndex::memberContext` 识别接收者
（仅支持标识符或单次调用；`a.b.` 链式与下标结果一律 unknown），`inferReceiverType` 按声明与调用点
确定性推导——`new Class()`、字符串/整实数/`[]`/`%[]`/正则字面量、声明的函数返回值、变量拷贝链与
显式类型注解。推导命中后按类型返回成员：优先声明索引（系统/项目 `.d.tjs` 的 `membersOf`，含
extends 链与构造器排除），未命中时回退已打开文档中的工作区 `class` 成员；未知类型宁可空。
普通补全回落纯语言关键字 + 当前文档声明 + 声明索引顶层唯一名。声明查询上限 64 文件、单文件
2 MB、共享 20 MB 预算；正文未变时复用解析结果由调用方合并规则保证（`src/lsp/Lexical.cpp`、
`src/lsp/Server.cpp`）。

### 跳转定义与悬停

`textDocument/definition` 保留保守作用域解析：`DocumentIndex` 对平衡的、未使用预处理指令的源码
建立词法绑定索引——函数/getter/setter/catch 作用域、class/property 体、for/with 头部、参数与
var/const 声明（含初始化可见性、类型注解），按最近嵌套作用域解析；成员访问走声明/工作区类成员
条目。本文件解析不到的全局名先查声明索引（如 `Array.push` → `Array.d.tjs` 的真实文件），再查
工作区顶层声明，命中的全部位置按（同文件优先，URI，位置）排序后全部返回（`Location[]`），
唯一时保持单 `Location`。悬停展示声明签名与 `/** */` 文档、纯语言关键字说明或绑定类别
（`src/lsp/Lexical.cpp`、`src/lsp/Declarations.cpp`）。

### 边界（如实声明）

绑定级 `rename`/`prepareRename` 尚未从 Python 原型移植（客户端请求得到不支持错误）；
非流敏感，不做控制流/赋值序列分析；不解析 `@` 预处理指令——含指令或括号不平衡的源码整体
保守拒绝（宁缺毋错）；普通动态成员链与下标之后 unknown；Integer/Real 无成员表，回落词法补全。
这仍是保守语言服务，不是静态类型检查器；重载选择、声明/实现一致性与完整动态访问分析未实现。

### 测试与宿主验收

`tests/cli/test_lsp.py` 通过真实 stdio 子进程覆盖：握手与退出码、进程内编译诊断（含中文行
UTF-16 换算、修复后清空）、关键字/文档/声明索引补全与前缀过滤、成员补全的签名与 `/** */` 文档
（`values.push` → 构建期 `Array.d.tjs`）、未知接收者保守为空、悬停、跳转定义（本文件绑定、
跨文件唯一、声明文件）、文档符号、工作区符号、工作区 class 成员回退与 shutdown/exit。
由 CTest 注册（`CiallangLspProtocol`，需 `CIAL_BUILD_LSP=ON` 与 Python）。
VSCode 客户端 `npm run check` / `npm test` 做 JavaScript 语法与启动路径回归；
`test/smoke.js` 在真实 Extension Development Host 中验收补全、跳转与诊断。

协议参考：[LSP 3.17](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/)。

### 可选库与 feature 开关

可选能力库（如未来的网络库）以 CMake 开关门控注册（`CIAL_BUILD_*`/`CIAL_FEATURE_*` 惯例）：
库关闭时不注册 ApiMeta，构建期 `--api-decl-dir` 产物与安装的 `declarations/` 自然不含其声明，
LSP/编辑器随之不展示该接口；开启后声明随下一次构建自动出现，无需改动 LSP。
宿主自定义 SDK 同理：注册什么，声明就导出什么（`src/stdlib/StandardLibrary.cpp` 的注册入口、
`doc/declarations.md` §3）。

### VSCode 宿主验收

`editors/vscode/test/smoke.js` 使用真实 VSCode API 打开编辑器，调用补全/定义/诊断提供器，
断言 Array.push 补全、函数声明跳转与编译诊断。测试会覆盖工作区的 `smoke.tjs`，请使用独立的
临时工作区；工作区位于工程子目录时可省略服务器配置以验收自动发现。先构建 cial-lsp
（`CIAL_BUILD_LSP=ON`），再以 VSCode 可执行文件启动：

```text
Code --extensionDevelopmentPath=<仓库>/editors/vscode --extensionTestsPath=<仓库>/editors/vscode/test/smoke.js --user-data-dir=<临时用户目录> <临时工作区>
```

Windows 盘符 URI 可能为 `file:///d%3A/...`。服务端仅先还原盘符冒号，再对其余部分百分号解码，
避免相对路径误判与文件名二次解码（`src/lsp/Server.cpp` 的 `uriToPath`）。
