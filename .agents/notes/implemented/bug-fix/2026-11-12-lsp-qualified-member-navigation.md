# 类名限定成员的声明跳转

Status: implemented

## 问题与复现

用户在 preview.2 发布后反馈 Array.push 不能跳转读取 `.d.tjs`。
基线为 `e0779df` 工作树，正式 SDK 来源为 `ee1a60b`。使用实际下载的 SDK 协议探测：
`var a = []; a.push(1);` 和 `var a = new Array(); a.push(1);` 的成员首/中/末字符
均能返回 Array.d.tjs；`Array.push` 全部返回 null，`Math.abs` 同样失败。
新增 `test_qualified_class_and_namespace_members_jump_to_declarations` 在正式 SDK 上
实际失败（Array/Math 两个 subTest 均为 None），日志 `out/qualified-member-baseline.log`。

另查明本机 VSCode 当前运行的是 2026-11-11 构建的源码树 Debug 服务器，不是新下载的 SDK。
该旧 Debug 可通过原九项协议测试，因此不能仅以旧二进制解释此次缺口。

## 实现

`DeclarationIndex::valueType` 原来只用符号返回类型推导接收者；类和命名空间没有返回类型，
所以类名限定成员的所有者丢失。现在类/命名空间返回其限定声明名，其他变量/函数处理保持原逻辑。
`Array.push`、`Math.abs` 因而可共用成员补全、悬停和定义查询；同名局部变量仍遵循词法绑定，
不会直接套用系统声明。导航不执行表达式。

真实 VSCode smoke 增加实例成员、类名成员、命名空间成员的 definition 查询，并调用
openTextDocument 读取目标 Array.d.tjs/Math.d.tjs、核对实际签名，补上此前只检查本地函数
定义跳转、实例补全的宿主验收覆盖缺口。专题事实同步到 doc/lsp.md。

## 验证

- Windows Release 增量构建 cial-lsp，CTest `CiallangLspProtocol` 1/1 通过；
  最终直接执行协议测试 10/10 通过，含同名未知局部变量不误导航的断言。
- Windows Debug 增量构建 ciallsp/cial-lsp，CTest 注册 1 项并通过；最终协议 10/10 通过。
  正在运行的旧服务器映像成功改名备份至 out/，随后在原 Debug 路径生成新 exe，未结束用户进程。
  当前用户窗口需重载以退出旧映像并使用新文件。
- `npm run check` 与 `npm test` 27/27 通过。锁定 clang-format 20.1.0 检查修改的 C++ 文件通过。
- 真实 VSCode 宿主分别验证显式 Release 与源码自动发现 Debug；最后一次自动发现验收退出 0，
  日志 `out/declaration-vsix-user/logs/20261112T141832/window1/renderer.log`：
  `startup, diagnostics, completion, definition and declaration file reading passed`。
- 使用正式 VSIX + 正式 SDK 的独立宿主也确认实例成员在多个光标位置能打开中文/百分号路径
  下的 Array.d.tjs，日志同目录 20261112T141418；这不能代替修复后的类名限定成员验收。

## 交付范围与下一步

本次修复和新回归位于本地工作树，Debug/Release 二进制已重建；未提交、未推送或替换公开
preview.2 标签/附件。发布版 SDK 的类名限定成员缺口仍在，下一次发布应包含此修复。
本机无需切换路径，重载 VSCode 后自动发现原 Debug 路径的新程序即可生效。

## F5 入口接续（2026-11-12）

用户继续反馈直接启动插件没有效果，询问 Debug 查找路径。原配置仅自动查找，F5 的
`npm: prepare` 依赖 npm 任务发现，准备脚本不检查声明，也未把查找结果绑定到子宿主。
现对源码调试显式固定：

- `.vscode/launch.json` 传入 `CIALLANG_DEBUG_ROOT=${workspaceFolder}/../..`；开发与 Test
  模式的客户端优先用该仓库 `build/<平台>/Debug/[tools/lsp/]cial-lsp`，显式传入
  `--declarations <Debug>/declarations`，不受旧 executable/SDK 设置影响。
  Production 模式忽略此环境变量。
- `.vscode/tasks.json` 显式登记 process 类型的 `Ciallang: prepare Debug`，执行
  `node prepare.js --debug`，检查 Debug exe 与 Array.d.tjs 缺失就失败，杜绝静默回落。
- 输出频道 Ciallang 记录实际 exe、cwd、声明目录与启动错误；两个 F5 入口均禁用其他已安装扩展。
- 默认 `Ciallang extension` 配置使用 `noDebug=true` 运行本地 Debug 语言服务器；
  另保留 `(JavaScript debugger)` 入口供 extension.js 断点使用。

这次实际嵌套调用 VSCode `debug.startDebugging` 进行验收，附加 JS 调试器时子宿主
确实发生 code 134 原生崩溃，发生在 cial-lsp 启动前；禁用其他扩展仍复现，不能把原因
归结为扩展冲突，也未宣称修复 VSCode 原生调试器。`noDebug=true` 的同一启动任务与
宿主环境绑定通过，作为默认 F5 的可用入口。

开发环境变量最初只识别 Development，但带 extensionTestsPath 的宿主属于 Test，
现两种模式均识别（Production 仍忽略）。客户端最终 32/32 测试通过，覆盖配置覆盖优先级、
Debug/声明缺失不回退、Production 环境隔离、Test 模式与输出。
独立真实宿主故意保存旧 SDK exe 和无效 SDK 根目录，仍选择当前 Debug exe 与显式声明目录，
Array.push/Math.abs 跳转及目标读取通过；日志 `out/f5-direct-user/logs/20261112T143827/`。
默认 noDebug 配置还通过真实 debug API、实际 preLaunchTask 与子宿主的完整 smoke，
并写出 `out/f5-child-result.json` 的 passed=true 与继承的 debugRoot；
验收日志 `out/f5-parent-config-nodebug/logs/20261112T144128/`。
最后对正式写入的默认 launch 配置再次验收，日志入口 `out/f5-final-user/logs/`。

## 真实仓库根目录中的重复声明（2026-11-12）

用户再次反馈仍不能跳转。读取实际用户窗口日志和进程，确认正在运行最新 Debug exe，
且 `--declarations .../Debug/declarations` 正确；实际 definition 请求来自
`tests/test_files/tjs2/test.tjs` 第 8 行 Scripts.execStorage，返回空。
此前只用隔离工作区验证，不能据此认定真实仓库工作区已经正常。

明确复现：同一 Release exe、同一 `var a = []; a.push(1);`，空目录作为 workspace root
返回 Array.d.tjs，仓库根目录作为 root 返回 null。新增产物目录用例在修复前失败，
日志 `out/generated-declarations-baseline.log`；对照脚本 `out/probe_workspace_declarations.py`。

根因是 `projectDeclarationSources` 递归扫描没有应用目录排除规则，把 build 中 Debug、
Release 等系统声明副本，以及 out 中暂存 SDK 一起当项目声明。多个 Array/Scripts
所有者覆盖系统声明后不再唯一，保守索引因此不提供成员。修复为声明扫描同样跳过现有
excludedDirectories，并为声明/工作区符号共同加入 out。显式 declarationFiles 和打开
的声明缓冲仍优先消费，不受自动扫描排除影响。

验证：Debug/Release 重建，Debug 最终 12/12 协议测试通过；修复后空目录和仓库根目录
对照都返回真实 Array.d.tjs。新增回归覆盖生成目录声明副本不污染系统/项目声明，
正常 types/host.d.tjs 仍覆盖系统，以及显式引用 out 下声明仍可用。
锁定 clang-format 20.1.0 与 diff 检查通过。

更强的真实编辑器验收：在**实际仓库根目录**打开用户实际文件 test.tjs，只读源文件，
自动调用 `editor.action.revealDefinition`（F12），核对活动编辑器实际切换为 Scripts.d.tjs、
Array.d.tjs 且目标文本可读；另在自建的临时 out 文件中验证 Array.push，同样实际打开
Array.d.tjs 后删除该临时文件。日志
`out/real-workspace-f12-user2/logs/20261112T145613/window1/renderer.log` 三次打开记录及
`Actual user file and actual project root F12 navigation passed`；不是仅检查 provider 返回值。
第一次实际 F12 已打开 Array.d.tjs，但脚本把 count 错写成 var 而非 property，导致内容
断言失败；依据实际声明修正断言后完整通过，未改导航逻辑来绕过测试。

用户明确要求自行操控后，已在新 Debug exe 就位后只结束身份核对的旧 LSP PID 23892
（父进程 3608），VSCode 客户端自动重连为 PID 20452，未关闭用户窗口或修改其源文件。
临时启用的工作区 trace 设置在保存必要日志后移除。无需用户再次修改路径或重启窗口。
此前 F5 绑定及类名成员补充仍保留，但不是此次真实工作区故障的完整解释；以本节复现
与验收范围为准。本次修改仍为本机工作树，未替换已发布 preview.2。

后续常驻索引、SDK 下载通知、Exception 单类导航及工程声明契约的实现和最终验收见
[LSP 索引与声明契约](../architecture/2026-11-12-lsp-index-contracts.md)。
