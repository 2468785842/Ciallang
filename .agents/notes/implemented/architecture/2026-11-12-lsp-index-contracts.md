# LSP 常驻索引、SDK 缺失通知与工程声明契约

Status: implemented

## 问题与完成范围

用户反馈插件缺少 SDK 时没有明确下载入口、声明导航慢、Exception 同时出现类和构造器，
并要求具体参数类型及工程化声明文档。本次基于 e0779df 的 dirty 工作树完成本地实现，
包含此前 [真实工作区导航修复](../bug-fix/2026-11-12-lsp-qualified-member-navigation.md)。
未提交或更新远端 preview.2 附件；公开 SDK 仍是 ee1a60b。

## 实现与取舍

- 客户端先发现本地服务和有效缓存；缺失时通知提供“下载 SDK / 打开设置”，下载进度和
  校验沿用现有安装流程。关闭通知不下载。有效缓存即使关闭自动下载仍可离线复用。
- Server 初始化建立有界磁盘/符号索引，按路径、mtime、大小复用正文，声明按 URI/正文复用
  解析，打开文档缓存词法绑定；每次查询重置推导预算。VSCode 大小写兼容文件监听通知
  创建/修改/删除，缺少事件时每 2 秒核对快照。目录/文件/正文/词法缓存均有上限。
- 自动排除构建目录和重复生成声明，显式引用与打开缓冲仍生效。工作区符号先筛选再限制
  200 项，定义查询使用完整有界索引。构造器不作为顶层符号，唯一类引用直接返回 class。
  限定类型引用先查完整限定名，避免 Host.Exception 错跳全局 Exception。
- 声明模式新增有限联合、限定类型和数组元素语法；标准库注册点补齐具名参数、具体类型、
  默认值和语义注释。String/RegExp 的确定字符串数组返回为 String[]。真正动态的值保持
  any；变长参数的星号表示收拢。未实现泛型、类型别名或完整静态类型检查。
- 导出器追加参数/返回契约，悬停渲染参数、返回、异常与示例；SDK 声明目录新增 README
  和版本/路径/SHA-256 清单。详细规范见 doc/declarations.md 与 packaging/declarations/README.md。

## 验证证据

Windows x64，Clang 19.1.5，x64-windows-static；Release -O3 -DNDEBUG，JIT OFF。
CIAL_BUILD_TESTS/CLI/LSP/TOOLS 均开启，格式化使用 clang-format 20.1.0。

- Debug/Release 构建成功；最新 Debug 协议测试 16/16 通过，Release CLI 与协议 CTest
  通过。完整 Release CTest 250/250 通过（127.33 秒，out/index-final-all-ctest.log）；
  最后仅补齐 String.split 的 String[] 返回元数据，重建后重新运行 CLI/协议验收。
- 客户端 npm run check 与 npm test 成功，34/34 测试，覆盖下载通知关闭、缓存、F5 路径。
- 协议覆盖连续 30 次查询不重复读取/解析、单声明变更仅新增一次解析、删除失效、
  大写 .D.TJS、超过 200 项仍能导航、重复声明排除、Exception 类唯一目标及限定类型。
- 最新真实 VSCode Extension Host 在仓库根目录打开 tests/test_files/tjs2/test.tjs，
  调用 editor.action.revealDefinition 实际打开 Scripts/Array.d.tjs；另验 Array.push。
  文件监听创建/修改后悬停由 String 变为 Integer，Exception 仅一个 class 目标，
  Scripts 参数和示例 Markdown 均通过。自建探针目录已清理，用户源文件只读。
  日志 out/index-final-host-user/logs/20261112T190005/window1/renderer.log，
  最终记录 Index host: watcher create/change, precise contracts, formatted docs and unique Exception class target passed。
  VSCode 内置 agentHost 有启动超时日志，但测试扩展与 LSP 验收完成。
- Debug/Release 声明清单各 11 文件，逐项 SHA-256 验证通过。格式与 git diff --check 通过。
- 同机公开 preview.2 对照，263 文件样本预热 definition 中位数 3.45 → 0.74ms，
  initialize 24.08 → 301.09ms；启动建索引有成本，不外推比例。见 doc/performance.md。

原始构建、完整 CTest、协议与宿主脚本在忽略目录 out/。下一步如发布，需重新构建原生 SDK
及 VSIX 并跑远端平台矩阵；不得把本地 Debug 的通过当作已下载 SDK 的版本更新。

## 同步替换已完成

用户随后明确要求“同步替换”。实现提交 d7dd1c0328dadc94da29984971f143627cc2d801
已推送 master；插件独立升级为 0.2.1，并使用 sdk/contracts-v1 缓存目录，避免升级后
继续使用原 preview.2 自动下载缓存。旧缓存保留，手动配置的离线路径仍由用户维护。

- 普通 CI 43211369924：9/9 成功，含三平台 Debug/Release、Windows JIT、安装 SDK 与格式。
- 发布验收 43211370817：validate、VSIX、三平台原生包均成功；手动入口不创建草稿，
  draft-release 跳过。每个平台完整测试及独立目录解压验收通过。
- 三平台归档均核对 revision=d7dd1c0、dirty=false；每包 11 个声明文件的清单 SHA-256
  验证通过。VSIX 版本 0.2.1、默认 SDK preview.2、新缓存目录与依赖验证通过。
- 已覆盖公开 preview.2 的三平台归档/各自 sha256、SHA256SUMS，上传 0.2.1 VSIX 并移除
  旧 0.2.0 附件；更新发布说明。标签从 ee1a60b 移到 d7dd1c0，推送使用旧 SHA lease。
  标签引起的重复任务 43214045768/43214045615 已主动取消，复用上述同 SHA 完整验收。
- 公开 8 个附件重新下载后逐字节哈希一致；从公开 VSIX 提取 sdk.js，不传凭据下载到
  中文/百分号路径，核对新提交和离线复用，实际下载 exe 16/16 协议测试成功。

附件 SHA-256：Windows c0e3078fc8f53b099404518cb3b7f075a79218278690616772a01142ad68b20d；
Linux 9e04ae48e9b4d88c3a4a1ab386b498828d25d325d8882009fab609ea0e3f6ab2；
macOS 8edd373d028d028f5e0877c0d7ba4758474e9d09198f533e5261af51f6e468a6；
VSIX 91afff9f6d75eca97082d78b2618afa2b2238386c4dec23d53e2a280918f905f。
备份原附件在 out/preview2-original-assets；新包、发布说明、原生日志和公开验证日志均在
out/preview2-refreshed-* 与 out/preview2-refresh-*。本节替代前文“未提交/未更新附件”的初始状态。
