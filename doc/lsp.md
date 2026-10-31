# LSP 与编辑器接入

`tools/lsp/cial_lsp.py` 使用 Python 3 标准库提供 stdio LSP：完整文本同步、实时语法/编译诊断、关键字与当前文档声明补全、基于保守类型推导的语义成员补全、`textDocument/definition` 跳转（本文件作用域解析 + 跨文件顶层声明）、`textDocument/rename` 绑定级重命名、内置关键字悬停说明、词法级文档符号列表，以及跨文件工作区符号搜索。诊断由 `cial --check-json --source-name <文档URI> -` 经 stdin 返回（schema 2：半开范围、纯文本消息、稳定错误码透传为 `code`），不落临时文件、不暴露宿主路径；检查过程不执行被检查脚本，位置从原生 UTF-8 字节列按完整字符边界转换成 LSP UTF-16 列（`src/cli/Application.cpp`、`src/cli/Compilation.cpp`、`src/common/SourceFile.cpp`、`tools/lsp/cial_lsp.py`）。无 `schema` 字段的旧版检查器走兼容转换（包含式列 +1、剥离消息位置前缀）。

启动命令（将路径替换为实际绝对路径）：

```sh
python tools/lsp/cial_lsp.py --cial build/Windows/Debug/cial.exe
```

这里展示的是命令结构。客户端中应为 Python、服务器脚本及 cial 配置绝对路径，避免工作目录变化。Linux/macOS 通常使用 `python3`，cial 没有 `.exe` 后缀。stdout 仅承载带 Content-Length 的 JSON-RPC 消息，不能把普通 CLI 输出混入该通道（`tools/lsp/cial_lsp.py`）。

## VSCode

`editors/vscode/` 包含使用 `vscode-languageclient` 的客户端。进入该目录运行 `npm ci`，用 VSCode 打开该目录后按 F5；启动前自动执行 `npm run prepare`，Extension Development Host 会打开工程根目录，再打开 `.tjs` 文件。`prepare.js` 把服务器脚本复制进扩展目录，确保打包时不依赖仓库外的相对路径（`editors/vscode/package.json`、`extension.js`、`prepare.js`、`.vscode/launch.json`）。

扩展通过 TextMate 文法提供关键字、字符串、嵌套注释、数值、Octet、正则与预处理高亮，
并识别模板串中的 `${expr}` / `&expr;` 插值；颜色由当前主题决定，基础高亮不依赖语言服务。
括号匹配、引号与括号自动闭合、注释快捷键由 `language-configuration.json` 注册。
词法规则依据 `src/parser/Token.hpp`、`Lexer.cpp` 与 [语法说明](syntax.md)；
实现见 `editors/vscode/syntaxes/ciallang.tmLanguage.json`，分词回归见 `test/grammar.test.js`。
正则/除法按局部上下文识别，不等价于解析器的完整上下文判定。

配置项 `ciallang.python` 指定 Python，`ciallang.executable` 指定已构建的 cial。未显式配置时，客户端根据活动文件和工作区向父目录查找 `build/<平台>/Debug/cial`，同级 Debug 不存在时尝试 Release，最后回落 PATH 的 `cial`。配置的相对路径按活动文件所属工作区解析；修改配置后重新加载 VSCode 窗口。服务端缺失或 Python 启动失败时弹出错误并提供设置入口；cial 不可用时发布检查器不可用诊断（`editors/vscode/extension.js`、`launch-options.js`、`tools/lsp/cial_lsp.py`）。源码调试及排错见[客户端说明](../editors/vscode/README.md)。

## IDEA / CLion 等客户端

在支持自定义 stdio Language Server 的客户端或插件中，注册上述启动命令，把 `.tjs` 关联到该服务器即可。仓库提供通用 LSP 服务，未捆绑 JetBrains 插件，也未把该服务器注册进用户的 IDE 设置（`tools/lsp/cial_lsp.py`）。

## 范围与验证

`workspace/symbol` 支持在工作区所有 `.tjs` 文件中按名称子串搜索函数、类、属性和变量声明，忽略大小写，结果包含文件 URI 与 UTF-16 名称范围。支持该请求的客户端可显示全局符号搜索入口，例如 VSCode 的“转到工作区中的符号”。初始化读取 `workspaceFolders`，兼容 `rootUri` / `rootPath`；`workspace/didChangeWorkspaceFolders` 可更新目录集合。没有工作区目录时只搜索已打开的文件，不自行扫描当前进程目录（`tools/lsp/cial_lsp.py` 的 `workspace_symbols`、`update_workspace_folders`）。

未打开文件按磁盘内容索引，查询时检查文件时间与大小并缓存声明；未保存缓冲优先覆盖磁盘内容，关闭后恢复磁盘版本，删除文件会移出索引。扫描不跟随目录符号链接，并排除解析后位于工作区之外的文件链接；跳过 build、bin、ext、node_modules 以及常见编辑器/缓存目录。单文件上限 2 MB，每次查询最多检查 2000 个 `.tjs` 候选文件和 10000 个目录，源内容预算 20 MB，最多返回排序后的 200 个匹配；可收窄名称继续查找。无法读取、编码非法或超限的文件会跳过（`tools/lsp/cial_lsp.py` 的 `MAX_*`、`EXCLUDED_DIRECTORIES`、`workspace_symbols`）。

诊断在独立后台线程执行（`CHECK_DEBOUNCE_SECONDS`=0.15s 防抖合并快速输入），编译检查不阻塞后续请求；发布前按文档版本丢弃过期结果，stdio 帧经互斥锁有序写出。每次完整文本更新触发一次编译检查，单次超时 `CHECK_TIMEOUT_SECONDS`=5 秒（超时杀掉子进程），诊断文档上限 2 MB；关闭文档清空诊断并丢弃待检请求，在途结果由版本检查拦下；检查器不可用发布带 `lsp.checker-unavailable` 错误码的诊断，与编译器诊断区分（`tools/lsp/cial_lsp.py` 的 `declarations`、`check`、`schedule_check`、`_check_loop`）。

文件身份按解析后的真实路径匹配，确保路径别名指向的未保存缓冲覆盖同一磁盘文件。已打开文件在工作区符号、跳转定义和重命名编辑中保留客户端原始 URI；未打开文件使用真实路径生成的 URI，避免 macOS `/var` 与 `/private/var`、Windows 短路径与长路径混用时误判绑定或遗漏缓冲（`tools/lsp/cial_lsp.py` 的 `file_path`、`workspace_sources`）。协议测试对磁盘路径先执行 `resolve()`，并使用含 `..`、空格、百分号和中文的客户端 URI 验证别名覆盖、跨文件定义、双向重命名与关闭恢复（`tools/lsp/test_protocol.py`）。

### 保守类型推导与语义成员补全

`textDocument/completion` 在 `receiver.` 形态（光标紧跟点号，或点号后已输入半截成员名）做类型推导：`DefinitionIndex.member_context` 识别接收者（仅支持标识符或单次 `foo()` 调用；`a.b.` 链式与下标结果一律 unknown），`infer_receiver_type` / `infer_expression` 按声明与调用点确定性推导——`new Array()/Dictionary()/RegExp()/Date()`、字符串/整实数/`[]`/`%[]`/正则字面量、`new MyClass()` 的工作区 class、`foo()` 的函数返回值（分析函数体 `return` 表达式，跨绑定递归深度 ≤ `MAX_INFER_DEPTH`=3，循环/多返回不一致/预算耗尽一律 unknown）。推导命中后按类型返回成员：内置类型走 `BUILTIN_MEMBERS` 名单（逐一核对自 `src/stdlib/ArrayLib.cpp`、`StringLib.cpp`、`DictLib.cpp`、`MathLib.cpp`、`DateLib.cpp`、`RegExpLib.cpp`、`SerializeLib.cpp`、`ExceptionLib.hpp` 的注册代码，含 kind 与一行签名）；工作区 class 给出类体内 var/function/property 声明（`DefinitionIndex` 现在把类体字段/property 名绑定进类作用域），并沿 `extends` 名字解析父类成员（`Server.type_members`、`unique_workspace_class`，层级上限 `MAX_CLASS_HIERARCHY`=8，派生类成员覆盖基类同名成员）。推导不出类型时原样回落既有词法补全（关键字 + 文档声明），不报错；非成员位置行为完全不变（`tools/lsp/cial_lsp.py` 的 `member_context`、`infer_*`、`BUILTIN_MEMBERS`、`type_members`）。

推导边界（如实声明）：非流敏感，不做控制流/赋值序列分析；不解析 `@` 预处理指令；成员访问链与下标之后 unknown；Integer/Real 无成员表，回落词法补全。单次请求推理有 `MAX_INFER_STEPS`=20000 步预算，超限宁可 unknown。

### rename / prepareRename

`textDocument/prepareRename` 对可重命名标识符（`DefinitionIndex.resolve` 命中的绑定，或能解析到工作区唯一顶层声明的自由标识符）返回该词范围与占位名；关键字、`.member` 动态成员、字典键、内置类名、无法唯一解析的自由标识符返回 null。`textDocument/rename` 做绑定级精确改写：本文件用 `references_of_binding` 复用 resolve 的作用域逻辑收集同一绑定的全部引用点（作用域遮蔽场景只改命中绑定）；绑定位于顶层全局时，要求与 definition 一致的工作区唯一性（`unique_workspace_binding`，含“作用域未知文件即视为多义”的保守拒绝），唯一时用 `free_references_of_name` 收集其他文件中的自由标识符引用。新名必须匹配 `[\w$]+$` 且非数字开头；总编辑数上限 `MAX_RENAME_EDITS`=5000，多义/超限返回 null（`tools/lsp/cial_lsp.py` 的 `prepare_rename`、`rename`、`build_rename`）。

### 跨文件跳转定义

`textDocument/definition` 保留保守的作用域感知解析：`DefinitionIndex` 对平衡的、未使用预处理指令的源码建立词法绑定索引——函数/getter/setter/catch 作用域、class/property 体、for/with 头部与单语句体、参数与 var/const 声明（含初始化表达式可见性、类型注解、收拢参数），按最近嵌套作用域解析标识符；成员访问（`.x` 前置点号）、关键字与内置类名、含 `@` 指令或括号不平衡的源码一律拒绝（宁缺毋错）。本文件解析不到的全局名再查工作区：命中的全部顶层声明按（同文件优先，URI，位置）确定性排序后**全部返回**（`Location[]`），唯一时保持单 `Location` 向后兼容，无匹配才返回 null；存在无法确认作用域（`@`/括号不平衡）又词法声明了该名字的文件时仍整体放弃（`tools/lsp/cial_lsp.py` 的 `DefinitionIndex`、`resolve`、`definition`）。

### 测试

`tools/lsp/test_protocol.py` 通过真正的子进程 stdio 测试初始化、诊断与修复、关闭、补全、悬停、符号、未知请求、shutdown/exit，以及 Unicode 位置转换；另外覆盖未打开文件的工作区搜索、磁盘修改/删除、未保存缓冲更新与关闭恢复、工作区目录切换、结果数量限制、超限/编码非法文件跳过，以及带空格、百分号和中文的 URI。新增能力测试覆盖：内置类型推导成员补全（Array/String/Dictionary/RegExp、前缀过滤、Integer/unknown 回落词法）、工作区 class 成员与 extends 继承、函数返回值类型、跨文件唯一类与多义回落、`prepareRename` 接受/拒绝形态（关键字、内置类名、动态成员、字典键、自由标识符）、绑定级 rename 的作用域遮蔽精度与非法新名拒绝、跨文件唯一 rename 与多义 null、definition 多文件返回全部并按 URI 排序、唯一时单 Location 兼容。诊断协议测试覆盖：schema 2 半开范围与错误码透传（含中文行 UTF-16 换算、EOF 零宽插入点）、快速连续编辑防抖合并到最新版本、检查器不可用与旧版检查器输出的区分转换（`tools/lsp/test_protocol.py`）。Python 可用时由 CTest 注册（`CiallangLspProtocol`）。VSCode 客户端已做依赖安装与 JavaScript 语法检查，2026-10-31 已在 Windows 的真实 VSCode Extension Development Host 中验证成员补全、定义跳转、重命名并应用编辑；JetBrains 客户端仍未验证（`tools/lsp/test_protocol.py`、`tests/CMakeLists.txt`）。

协议参考：[LSP 3.17](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/)。

### VSCode 宿主验收

`editors/vscode/test/smoke.js` 使用真实 VSCode API 打开编辑器，调用补全/跳转/重命名提供器，
断言 Array.push、函数声明位置及两处重命名编辑并应用到文档，再引入语法错误并修复，验证实际编译器诊断。测试会覆盖工作区的
`smoke.tjs`，请使用独立的临时工作区。该工作区 `.vscode/settings.json` 可为
`ciallang.python`、`ciallang.executable` 配置绝对路径；工作区位于工程子目录时可省略解释器配置以验收自动查找。先执行 `node editors/vscode/prepare.js`，
再以 VSCode 可执行文件启动：

```text
Code --extensionDevelopmentPath=<仓库>/editors/vscode --extensionTestsPath=<仓库>/editors/vscode/test/smoke.js --user-data-dir=<临时用户目录> <临时工作区>
```

Windows 盘符 URI 可能为 `file:///d%3A/...`。服务端仅先还原盘符冒号，再由
`url2pathname` 解码其余部分，避免相对路径误判与文件名二次解码；协议测试另覆盖
带空格、字面 `%20` 和中文的路径（`tools/lsp/cial_lsp.py`、`test_protocol.py`）。
此验收通过 VSCode 提供器命令执行，未覆盖全部快捷键、主题与弹窗布局。
