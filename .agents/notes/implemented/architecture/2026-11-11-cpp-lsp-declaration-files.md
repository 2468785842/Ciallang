# 声明按类分文件与 C++ LSP（cial-lsp 内化）

Status: implemented

## 问题与决定

基于 `2026-11-11-declaration-files-distribution`（构建期 stdlib.d.tjs 单文件 + Python LSP 读取），
用户反馈五点：①插件没有自动加载标准库接口；②`cial_lsp.py` 仍硬编码 API 描述（KEYWORDS 的
Array/String/Math/Scripts 等）；③系统声明应每类一个 `.d.tjs` 而非单一 stdlib 文件；④系统函数
签名不应是清一色未命名 `any*`，类也应有文档注释，LSP 从声明文档注释取文档而非 py 定义；
⑤Python 换成 C++，feature 开关（如未来的网络库）对应的声明自动进退。

决定（经确认）：C++ LSP 首批做核心集（诊断/补全/悬停/定义/文档符号/工作区符号，rename 暂缓）、
Python LSP 直接删除、feature 开关做机制+文档（不搭网络库骨架）。

## 实际实现

### 声明基础设施

- `ApiCatalog` 增加 `typeDocs`；`Context::recordApiTypeDoc` + `ClassBuilder::doc()` /
  `NativeTypeBuilder::doc()` 登记类型级文档；`declarationsFromCatalog` 填充类符号 doc，
  namespace 全局（ApiKind::Namespace）的 meta.doc 不再被丢弃。
- `decl::generateDeclarationFiles`：按顶层 class/namespace 每符号一个 `<名>.d.tjs`，
  顶层自由函数/变量归入 `globals.d.tjs`。
- CLI `--api-decl-dir <dir>`（与 `--api-decl` 同用）按符号写文件；`--api-decl` 单文件 stdout 保留。
- POST_BUILD 改为生成 `declarations/` 目录并复制 script-builtins.d.tjs
  （`cmake/GenerateApiDeclarations.cmake`）；SDK/Runtime 安装 `install(DIRECTORY)` 整目录。
- stdlib 签名充实（两个并行代理完成）：Math(18 方法+8 属性)/Date(17)/RandomGenerator(6)/
  Dictionary(3)/RegExp(6)/Serialize(12，Array/Dictionary/Octet 成员)/String 缺口(2)全部改为
  `api::signature` 具名签名；9 个类加 `.doc()`。按实现事实修正了任务简报的错误猜测
  （random→Real、serialize→Dictionary、dict assign/clear→void、Date.parse→void 等）。

### C++ LSP（src/lsp/，cial-lsp）

- `Json.hpp/.cpp`：LSP 所需最小 JSON（解析/序列化，无第三方依赖）。
- `Lexical.hpp/.cpp`：Python DefinitionIndex 的移植（掩码、括号平衡、函数/类/属性/catch/
  for/with 作用域、参数与 var/const 绑定、可见性遮蔽解析、预算化类型推导 20000 步/深度 3）。
  关键字表只含纯语言关键字——API 名称一律不在此（用户要求②）。
- `Declarations.hpp/.cpp`：decl_index.py 的内化——系统声明目录（可执行文件旁 declarations/
  逐级祖先 + share/ciallang/declarations + `--declarations`）、显式文件、工作区/缓冲 .d.tjs，
  全部经 `decl::parseDeclarations` 进程内解析；qualified 条目、extends 链 membersOf、
  value/call/return 推导、补全/悬停/定位项。
- `Server.hpp/.cpp`：stdio JSON-RPC（Windows 下 stdin/stdout 二进制模式——文本模式会把
  `\n` 译成 `\r\n` 破坏帧头）、后台诊断线程（150ms 防抖、最新版本优先、进程内
  `cli::compile` / 声明模式诊断）、补全（声明成员 + 工作区 class 回退 + 关键字/文档/顶层）、
  悬停（声明签名+文档注释）、定义（本文件绑定 → 声明条目 → 工作区顶层，全部候选排序返回）、
  documentSymbol/workspaceSymbol、多工作区、UTF-16 列换算。通知方法永不响应（即使客户端违规带 id）。
- 诊断编译路径与 `--check-json` 一致：每次检查新建 Runtime/Context，不注册标准库，不执行脚本。

### 删除与扩展

- 删除 `tools/lsp/cial_lsp.py`、`decl_index.py`、`test_protocol.py`；扩展删除 `prepare.js`
  与 `server/` 副本，`package.json` 移除 `ciallang.python` 与 prepare 脚本。
- 扩展 `launch-options.js` 改为发现 `cial-lsp`（build/<平台>/<Debug|Release>/[tools/lsp/]），
  `ciallang.executable` 语义改为指向 cial-lsp；README/lsp.md 重写。
- CI 矩阵 `ci.yml` 加 `-DCIAL_BUILD_LSP=ON`（覆盖三平台的可执行路径发现）。

### feature 开关机制（用户要求⑤）

机制+文档：可选库以 CMake 开关门控注册——关闭的库不进 ApiCatalog，构建期声明产物自然不含
其接口，LSP 随安装产物自动进退；文档落在 doc/lsp.md §可选库与 feature 开关、
doc/declarations.md §3。网络库骨架未搭（按用户确认另开批次）。

## 验证

本机 Windows 10.0.19045，Ninja Debug `build/Windows/Debug`（含 CIAL_BUILD_LSP=ON），
clang-format 锁定 20.1.0（pip 安装；本地 20.1.8 与锁定版对全部改动文件输出一致）。

- 全量构建零错误；`declarations/` 11 个文件（9 类 + globals + script-builtins）。
- 新协议测试 `tests/cli/test_lsp.py --lsp ...`：**8/8 通过**（真实 stdio 子进程；诊断中文行
  UTF-16、修复清空、声明补全 + 文档、成员补全/悬停/跳转到真实 Array.d.tjs、跨文件定义、
  工作区 class 回退、documentSymbol/workspaceSymbol、退出码）。
- 编辑器 `npm run check` 通过，`npm test` **17/17**（高亮分词 + cial-lsp 启动路径发现）。
- 全量 CTest **250/250 通过**（80.6s，含 238 个 TJS、CiallangLspProtocol、CiallangCli、
  CiallangSuite、CiallangPackaging）。
- 格式：改动 C++ 文件以 20.1.0 校验通过（`src/Binding.hpp`、`src/cli/Application.cpp`、
  `tools/lsp/main.cpp` 经 -i 修正）。
- 全量验证中发现并修正两处：`SerializeLib` 的 saveStruct `mode` 曾标 optional 无默认字面量
  （声明语法不可表达，api-json 与 decl-json 一致性测试失败）→ 给出诚实默认 `""`；
  `test_api_json` 的 Math.abs variadic 断言随具名签名过期 → 更新为 `(n: Real)` 具名断言。
  另有事故记录：曾误对 `cmake/GenerateApiDeclarations.cmake` 跑 clang-format -i（C++ 规则
  搅乱 CMake 语法致 POST_BUILD 失败），已重写恢复——.cmake/.py/.js 不属于 clang-format 范围。

调试过程要点（供后续接续）：stdio 帧在 Windows 文本模式下 CRLF 二次转换是最初"无响应"的
根因；`_budget` 未初始化为 MAX_INFER_STEPS 曾让全部类型推导第一步即失败；可执行文件在构建树
`tools/lsp/` 子目录时声明目录需逐级祖先查找。

## 保留边界

rename/prepareRename 未移植（客户端得到"方法不支持"错误，doc/lsp.md §边界如实声明）；
工作区 class 成员回退只查已打开文档；诊断超时无杀进程语义（进程内编译，无子进程超时问题，
但坏输入的编译耗时未设上限）；未启动真实 VSCode Extension Development Host 验收（JS 层已回归，
宿主验收按 doc/lsp.md 步骤可随时执行）；远端 CI 矩阵以 ci.yml 新增的 CIAL_BUILD_LSP=ON 为准。
