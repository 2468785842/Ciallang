# Windows LSP 路径修复与 VSCode 工具 SDK 分发

Status: implemented

## 问题与来源

基于 `d0eeeb5` 的 dirty 工作树。CI run `43039580180` 的 Windows Debug、Release、
Release/JIT 均在 `CiallangLspProtocol` 失败；Release run `43039579958` 的 Windows
作业同样失败。日志为 `test_definition_jumps_to_local_binding_workspace_and_declarations`
第 196 行：跨文件 definition 返回 null，触发 `TypeError: 'NoneType' object is not subscriptable`。
Linux/macOS 普通 CI 作业通过，格式检查通过。

## 实现与边界

- `Server.cpp` 的 URI 输入只做一次百分号解码，Windows 去除盘符前的 URI 斜杠，
  按 UTF-8 显式转换原生路径；输出使用 `generic_u8string` 编码，避免依赖 Windows 代码页。
  可执行文件定位改为 `GetModuleFileNameW`。系统声明位置复用 `pathToUri`，修复 POSIX
  多余斜杠与中文 SDK 目录的编码。
- 协议测试补充 UTF-8/百分号文件名、localhost、小写编码盘符，并清理子进程与管道。
  声明跳转检查使用标准 URI 拆分，保留 POSIX 绝对路径。
- VSCode 扩展 0.2.0：显式 executable → 显式 SDK 根目录 → 本地构建 → PATH →
  官方 Release SDK 缓存。下载指定完整版本的 ZIP/TAR.GZ 及校验文本，核对 SHA-256、
  版本、系统/架构、LSP 与声明布局后安装；支持离线缓存、损坏缓存重装、关闭下载。
  SDK 是含工具与声明的 CLI 包，C++ 开发 SDK 仍从源码安装。
- `package.py` 强制开启全部工具，并在独立解压目录执行完整 LSP 协议回归；
  `build-info.json` 新增 lsp 开关。CI/Release 加入客户端测试与 VSIX 打包，草稿汇总包含 VSIX。
  扩展默认 SDK 版本由 prepare 检查与 manifest 一致。
- 修正旧 smoke 测试中要求尚未实现 rename 的断言，保留实际支持的启动、补全、定义和诊断验收。
  使用指南见 `doc/lsp.md`、`doc/releasing.md`、`editors/vscode/README.md`。

## 验证证据（2026-11-12，Windows x64）

- 本机原始八项协议测试通过，未单独复现 CI 的非 UTF-8 系统代码页环境；原始失败证据来自上述 CI。
  新用例最初把等价 URI 的工作区同时 removed/added，服务端按当前顺序最终移除目录而失败；
  已改成先移除、再添加两条通知。该次用例设计错误不能作为原始故障的本机复现证据。
- VS Developer PowerShell 环境，复用已核对的 Windows Release 缓存、静态 CRT、
  `x64-windows-static`，JIT OFF：
  `python tools/package.py --build-dir build/Windows/Release --parallel 4` 最终退出 0；
  `ctest -N` 注册 250 项，最终 **250/250** 通过（105.52 秒），解压后的协议 **9/9** 通过。
  先前一次 249/250 的失败为上述用例设计错误，保留于 `out/package-windows.log`；
  最终日志 `out/package-windows-final.log`。
- 客户端 `npm ci`、`npm run check`、`npm test`：**27/27** 通过，覆盖激活优先级、
  下载开关、ZIP/TAR.GZ、校验失败、缺失服务端、缓存修复与离线复用。
- `npm run package` 生成 VSIX。首次重打包的 npm 内部 minipass 加载偶发失败，
  同一命令重跑成功，没有更改或跳过依赖清单。
- 真实 VSCode 宿主先用中文 SDK 路径验证，再解压最终 VSIX，从其中的 sdk.js 安装最终
  原生 ZIP（本地替代下载源）到扩展 globalStorage；无 SDK 路径设置、无可发现源码构建的
  临时工作区中，通过缓存自动启动服务器，诊断/补全/定义均通过，扩展宿主退出 0。
  日志：`out/vsix-user/logs/20261112T132735/window1/renderer.log` 与 `main.log`。
- 锁定 clang-format **20.1.0** 执行 `tools/check-format.sh` 全库通过；
  workflow YAML 解析、`git diff --check` 通过。

## 产物与发布下一步

产物位于忽略目录 `build/packages/`，均为 dirty 来源候选包：

- `ciallang-0.1.0-preview.2-windows-x64.zip`，SHA-256
  `3185c1d8ee3f5a6e23b429d5ab4f2940f99b8343e122573fb7794f3fdd235b25`。
- `ciallang-language-0.2.0.vsix`，SHA-256
  `2ca7bb5aab76c7b2817bf2c33e95a3e42902f33a171bf70736c4ad26b641212a`。
- 各自 `.sha256` 与 `SHA256SUMS-preview.2`。

未提交、未推送、未创建公开 Release，未运行修复后的远端矩阵；不据本机结果宣称 CI 已绿。
公开 Release 当前仅 preview.1，不含所需 LSP，preview.2 下载 URL 尚不可用。
现有 `v0.1.0-preview.2` 标签指向修复前提交，不能把本地产物等同于该标签的干净来源；
正式发布需先确定修复提交和版本/标签策略，再完成三平台流程与公开附件。
其他架构、Linux/macOS 新代码与真实网络下载的最终验收仍由发布矩阵覆盖。

## 远端发布接续：代码页遗漏（2026-11-12）

用户随后授权重跑 CI 并公开 preview.2。修复提交 `8b7fbd3` 推送后，以旧 SHA lease
将尚未公开的 preview.2 标签从 `d0eeeb5` 更新；普通 CI `43161915716` 和发布流程
`43161920792` 实际重跑。Linux/macOS CI 和原生打包通过；Windows 在新增 Unicode
文件名回归中报 JSON-RPC -32603：
`No mapping for the Unicode character exists in the target multi-byte code page.`
Windows Debug job `130994876608` 的日志确认失败，发布汇总因此跳过。

这是实际代码遗漏：工作区初始化、项目声明发现、排除目录检查的三处
`filename().string()` 仍依赖本地代码页。现统一复用 `utf8Path`（`generic_u8string`）；
同一回归进一步覆盖中文子目录和文件名。本机增量构建成功，
`ctest --test-dir build/Windows/Release -R CiallangLspProtocol --output-on-failure --no-tests=error`
退出 0（1 项 CTest、内部 9 项协议测试）。clang-format 20.1.0 与 diff 检查通过。
原始日志保留于 `out/preview2-windows-debug-failed.log`。以新的修复提交重跑全矩阵和
三平台发布验收后再公开，不能复用上一提交的包冒充新来源。

## 最终发布验收（2026-11-12）

最终源码提交为 `ee1a60b329ec129aee4c94bb993106a41ab35a32`，未公开标签经旧 SHA lease
更新到该提交；[普通 CI 43163015675](https://github.com/2468785842/Ciallang/actions/runs/43163015675)
**9/9 作业通过**。三平台 Debug/Release 各通过完整 CTest（Windows 250 项，
Linux/macOS 462 项），Windows Release/JIT 亦通过 250 项；四组 Release 安装消费
SDK 验收通过，Linux 独立源码消费者通过。VSCode 27 项与格式检查通过。

[发布流程 43163021971](https://github.com/2468785842/Ciallang/actions/runs/43163021971)
**6/6 作业通过**。三平台全量 CTest 及各自解压后的 9 项协议回归通过，VSIX 打包成功。
实际下载草稿的 8 个附件，核对汇总/独立 SHA-256、干净来源提交、系统/架构、全部工具与
Array 声明布局；VSIX 默认 SDK 版本和运行依赖正确。官方附件摘要：

- Windows x64：`34e349a270e9d91d32d0fad09e6eb744ade1994517a952e83be880aa31c97fb9`。
- Linux x64：`92c6d6bebc058ad987ecbc74b2d648958ea2df5bb3610b4d44ba6f0d3e49ff49`。
- macOS ARM64：`1f8cbf6931fc6fc68dc96f763fe9c20f2dec537a1c009ece3938f7f57b59b627`。
- VSIX 0.2.0：`fe482a0ff560fcd9ddb0b22fc8ca9e3ecc10e465ff36140d8a20d1edac0b9a41`。

已按用户明确授权公开 [v0.1.0-preview.2](https://github.com/2468785842/Ciallang/releases/tag/v0.1.0-preview.2)，
发布时间为 2026-11-12 13:51:37（Asia/Shanghai），`isDraft=false`、`isPrerelease=true`。
公开后，从实际发布 VSIX 解压出的 sdk.js **不传认证凭据、不替换下载源**，自动下载官方
Windows SDK 及校验文件，安装到中文/百分号路径；确认来源为 ee1a60b、dirty=false，
随后拒绝网络回调验证离线缓存，并执行 9 项真实 LSP 协议测试，全部通过（退出 0）。
日志 `out/preview2-public-sdk.log`；原生 CI/发布原始日志、下载附件和检查脚本均在忽略的 out/。

上文的本机候选包哈希与“未推送/未公开”描述是发布前历史状态，以本节的官方附件和远端
验收为准；旧失败与用例修正记录保留，不覆盖历史结果。README 已更新到本版下载入口。

发布后的类名限定成员导航反馈另有实际缺口，见
[Array.push 声明跳转修复](2026-11-12-lsp-qualified-member-navigation.md)；
此记录的发布版本与验收范围不代表后续本地修复已进入正式附件。

用户随后要求同步替换，现已将 preview.2 的标签和全部附件更新到 d7dd1c0，插件 0.2.1
使用独立 SDK 缓存；远端矩阵、附件哈希与公开下载验证见
[索引与契约的同步替换记录](../architecture/2026-11-12-lsp-index-contracts.md#同步替换已完成)。
