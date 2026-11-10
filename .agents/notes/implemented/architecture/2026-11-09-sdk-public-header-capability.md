# SDK 公共头布局收敛与能力查询

Status: implemented

## 问题

[路线图](../../../../doc/roadmap.md)批次 1 剩余"公共头布局收敛、兼容承诺、能力查询"。
SDK 此前安装整个 `src/`（仅排除 cli/format/dbg），公共接口的事实边界未定义；
`SDK_API_VERSION` 定义在 Engine.hpp 而非版本头；宿主与工具没有能力查询约定。
2026-11-09 提案并实施，本文记录落地决定与验证证据。

## 决定与实现

### 两层承诺模型

| 层 | 内容 | 承诺 |
|---|---|---|
| Tier 1 公共头 | `cial/Engine.hpp`、`cial/Storage.hpp`、`Binding.hpp`、生成的 `cial/Version.hpp`、`cial/Capability.hpp` | 源码兼容；破坏性修改提升 `CIAL_SDK_API_VERSION` |
| 签名闭包 | `common/Result.hpp`、`common/ResultMessage.hpp`、`types/Value.hpp`、`vm/VM.hpp`（Handle）、`runtime/Runtime.hpp`、`runtime/Context.hpp` | 与 Tier 1 同承诺 |
| Tier 2 其余传递头 | 闭包内其余头（入口闭包 59 个项目头，扫描见 `out/include_closure.py`） | 随 SDK 分发以便编译，宿主不应直接包含 |

放弃 pimpl 的原因保留：`Binding.hpp` 纯模板实现、`Engine` 按值持有 VM 全栈，
内部头无法从安装中移除；分层表写入 `doc/embedding.md`，安装范围注释见 `cmake/SDK.cmake`。

### 落地的修改

- `tools/generate_version.py`：`Version.hpp` 增加 `CIAL_SDK_API_VERSION`（模块常量，
  破坏性修改时提升）。
- `cmake/Capability.hpp.in` → 生成的 `cial/Capability.hpp`（`CMakeLists.txt`
  configure_file，option 处理后；`CONFIGURE_DEPENDS` 登记）：`CIAL_CAP_JIT`
  （来自 `CIAL_ENABLE_JIT`）、`CIAL_CAPABILITY_SCHEMA_VERSION=1` 与模块/调试/声明
  元数据预留位（保持 0）；`cial::capabilities::` constexpr 查询函数。Version.hpp
  保持 `project()` 前生成，职责分离：版本=包身份，能力=构建配置。
- `src/cial/Engine.hpp`：`SDK_API_VERSION` 改为 `capabilities::sdkApiVersion()` 别名
  （0.2 移除），包含 `cial/Capability.hpp`。
- `cmake/SDK.cmake`：安装两个生成头；EXCLUDE 增加 `tjsbc`、`decompile`（已验证不在
  公共闭包内）。
- `src/cli/Options.hpp/.cpp`、`Application.cpp`：`Mode::Capabilities` + `--capabilities`
  互斥模式；无输入、拒绝执行选项；`runApplication` 在创建 Runtime 前输出单行 JSON
  `{"schemaVersion":1,"sdkApiVersion":1,"version":"...","capabilities":{"jit":false}}`。
  版本串经 generate_version.py 语义校验，无需转义。
- `tests/installed/main.cpp`：`hasJit()` 与宏一致、`sdkApiVersion()`/schema 断言、
  别名一致。
- `tests/cli/test_cli.py`：`test_capabilities_json` 覆盖 JSON 字段与互斥/输入拒绝。
  后续修正（2026-11-10）：初版硬编码 `jit == False`，CI 的 Windows Release JIT-ON
  job 必然失败（run 42652843648）。现由 CTest 注册按 `CIAL_ENABLE_JIT` 传入
  `--expected-jit`（support.py 可选参数，未告知时跳过断言），
  `test_execution_options_compose_and_jit_is_optional` 同步按构建事实收紧为确定分支。
- 文档：`doc/embedding.md` 公共接口分层表 + 能力查询段；`doc/roadmap.md` §1 宿主集成
  尚缺项与 §3.1 状态更新。
- 后续（同日）：`src/config.h` 改名 `src/common/Config.hpp`（4 个引用点同步；初拟 CompilerDefs.hpp，
  按用户决定定名 Config.hpp）。
  原因：裸 `#include "config.h"` 在安装分发下会被宿主更靠前的 -I 路径上的同名文件
  遮蔽，`common/` 目录前缀消除该冲突；Tier 2 无兼容承诺，改名不影响两层模型。
  三个宏头（Version/Capability/Config）维持分离：生成机制（pre-project
  Python / post-option configure_file / 检入库）与语义（包身份 / 构建配置 / 编译器
  可移植宏 + 按包含 TU 计算的 `CIAL_DEBUG`）不同，前缀已统一为 `CIAL_*`，
  `doc/releasing.md` 的 `config.h` 引用同步更新。

## 验证证据（2026-11-09，macOS arm64，本机）

- `cmake --preset "MacOS Debug"` + build：生成的
  `build/MacOS/Debug/generated/cial/Capability.hpp` 内容正确（`CIAL_CAP_JIT 0`）。
- `build/MacOS/Debug/cial --capabilities` →
  `{"schemaVersion":1,"sdkApiVersion":1,"version":"0.1.0-preview.1","capabilities":{"jit":false}}`，rc=0。
- `ctest -R "CiallangCli|CiallangSwitch"`：2/2 通过；`tests/cli/test_cli.py
  CliTest.test_capabilities_json` 单独运行 OK（需 `--cial` 传二进制路径）。
- SDK 安装验收：`cmake -S . -B build/sdk`（Release、无 CLI/tests）构建通过后
  `python3.13 -I tests/sdk_install_test.py --build build/sdk`：空格+中文临时前缀、
  迁移后树外构建，`installed_host` 通过（含能力断言，rc 3/4 路径未触发）。
  注意：本机默认 python3 为 3.9.6，脚本 `ignore_cleanup_errors` 需 3.10+，
  实际用 homebrew python3.13。
- 安装头清单：`out/sdk-prefix/include/ciallang/cial/` 恰为 Capability.hpp、
  Engine.hpp、Storage.hpp、Version.hpp；`tjsbc/`、`decompile/`、`cli/` 未安装。
- `tools/check-format.sh` 口径的 clang-format 20.1.0：修改的 5 个 C++ 文件全部通过
  （Options.cpp、tests/installed/main.cpp 经 `-i` 修正后复验 OK）。
- 全量 `ctest --preset "MacOS Debug"`（450 项）：仅 3 个 decompile 用例失败
  （M2 leftovers、M5 irreducible、CiallangDecompile）。归因：并行 WIP 正在编辑
  `src/decompile/ExprBuilder.cpp`（工作树 +128 行未提交，验证期间仍在变动；新增
  `EvalPostfix` 渲染而 `tests/decompile_test.cpp:728` 仍断言"动态求值"不支持文案）。
  与本次改动无交集（本次 diff 不含 decompile）；重编后复跑仍失败，属并行会话
  中间状态，由该会话收口。

## 遗留与下一步

- 三平台远端安装验收以 CI 结果为准（路线图批次 1 既定）——**已收口
  （2026-11-10，run 42775661634）**："Test relocated installed SDK" 在
  Linux/macOS/Windows Release 及 Windows Release JIT-ON 全部通过，状态回写
  [路线图批次 note](../../proposed/architecture/2026-11-04-sdk-tooling-roadmap.md)。
- 2026-11-10 补充验证（Windows x64 本机，两份 Release 构建）：
  JIT-ON（`build/Windows/Jit`）与 JIT-OFF（`build/Windows/Release`）各重配置 +
  增量构建 `cial` 后，`--capabilities` 分别报告 `"jit":true`/`"jit":false`，
  `ctest -R CiallangCli` 在两份构建下均通过（注册分别带 `--expected-jit true|false`）。
- `CIAL_CAP_MODULES`/`CIAL_CAP_SCRIPT_DEBUG`/`CIAL_CAP_DECL_METADATA` 随批次 2/3/4
  落地接通；`SDK_API_VERSION` 别名计划 0.2 移除。
- 仓库拆分与独立发行（`ciallang-tools`）是批次 1 剩余主体，见
  [路线图批次 note](../proposed/architecture/2026-11-04-sdk-tooling-roadmap.md)。
