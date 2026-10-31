# CLI 预览版打包与发布

运行时版本只在 `vcpkg.json` 的 `version-string` 中维护，当前为 `0.1.0-preview.1`。
`tools/generate_version.py` 读取并校验版本，在构建目录生成：

- `generated/Version.cmake`：数值版本、预发布后缀和完整版本，供 `project(VERSION)` 与 CPack 使用。
- `generated/cial/Version.hpp`：主/次/修订版本宏及完整版本字符串，供 CLI 使用。

CMake 在 `project()` 前调用生成器；manifest 或生成器变化会触发重新配置。
内容未变化时不重写生成文件，避免无意义的重新编译。发布脚本直接复用生成器的版本读取逻辑，
不用正则解析 CMake 文件，也不维护第二份版本。只需修改 manifest，随后构建/打包即可。
支持 `major.minor.patch` 及可选的预发布后缀，例如 `1.2.3-rc.1`；
不接受数值字段的前导零或 build metadata 后缀。VSCode 扩展保持独立版本。

源码构建（包括 `add_subdirectory` 嵌入方式）因此需要 Python 3.10+；
已打包的 CLI 运行不需要 Python。生成文件不入库，也不覆盖 `src/config.h`。

本阶段交付 CLI 压缩包，不安装公共 SDK，也不承诺稳定 C++ ABI；SDK、`.d.tjs`、模块和
DAP 的后续工作见[路线图](roadmap.md)。本机已验证 Windows x64 包；Linux/macOS 流程
尚需实际运行 GitHub Actions 验收，本次未创建远端标签或 Release。

## 本地打包

需要 Python 3.10+、CMake 3.25+、CPack 和[构建依赖](building.md)。设置 `VCPKG_ROOT` 后，
Windows 首次配置请使用 VS 的 x64 Native Tools / Developer PowerShell，确保编译器、SDK 和
资源编译器 `rc.exe` 可用。从仓库根目录执行，按系统选择预设：

```sh
python tools/package.py --preset "Windows Release"
python tools/package.py --preset "Linux Release"
python tools/package.py --preset "MacOS Release"
```

只执行对应本机平台的一条命令。可复用已有、工具链已经配置好的构建目录：

```sh
python tools/package.py --build-dir build/Windows/Release
```

入口依次配置 Release（CLI/测试开启、JIT 关闭）、构建、运行全部 CTest、调用 CPack，
最后在含空格的独立临时目录中解压并验证。脚本不执行上传或发布。
默认输出到 `build/packages/`，`--output` 可指定其他目录，`--parallel` 控制构建并行数。
只有验证成功的压缩包和对应 SHA-256 文件才复制到输出目录；每次使用新的暂存目录，
不收集旧构建产物。多配置生成器也显式构建/安装 Release。

CPack 的文件列表来自 `cmake/Packaging.cmake` 中的 `install(... COMPONENT Runtime)`。
可通过 `cmake --install <build> --config Release --component Runtime --prefix <目录>`
查看暂存布局，但正式归档仍走 Python 入口，保留测试和解压验收。
`CIAL_ENABLE_PACKAGING` 默认关闭，要求独立工程且 CLI 开启；不会给宿主的
`add_subdirectory` 消费者注册 CPack 目标。

## 包内容与依赖

Windows 生成 ZIP，Linux/macOS 生成 TAR.GZ，包名标明版本、系统和实际目标架构，例如
`ciallang-0.1.0-preview.1-windows-x64.zip`。当前不支持 universal 多架构包。

```text
ciallang-<version>-<system>-<arch>/
  bin/cial[.exe]
  LICENSE
  README.md
  build-info.json
  examples/hello.tjs
  licenses/<port>/copyright
  licenses/<port>/vcpkg.spdx.json  # 依赖提供该文件时一同收录
```

首版要求核心及 fmt/utf8proc 等链接库为静态目标，Windows 使用静态 CRT。
不自动捆绑未知动态依赖；不满足要求时配置失败。系统基础运行库仍由操作系统提供，
不宣称跨任意 Linux 发行版或 macOS 版本通用。Release CI 固定使用 `ubuntu-22.04`、
`windows-2022`、`macos-15`，macOS 部署下限继承预设；架构从实际 CMake 配置生成，
不能根据 runner 名称猜测或承诺 universal。

许可默认从 `${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share` 收集
fmt、frozen、utf8proc、argparse 的 copyright；缺失则配置失败。
通过其他方式提供依赖时，设置 `CIAL_LICENSE_SHARE_DIR`，保留 `<port>/copyright` 布局。
依赖提供 SPDX 文件时也会收录，供检查依赖版本与来源。测试专用 Catch2 不进入发行包。
可选 JIT 的手工 CPack 配置额外要求静态 asmjit 及其许可；标准发布入口固定关闭 JIT。

`build-info.json` 的 `schema_version` 为 1，包含完整版本、源码提交、工作树是否有改动、
系统/架构、配置、编译器及版本、编译选项、JIT、CRT 和 macOS 部署下限；
Linux 另记录发行版/glibc 信息，vcpkg Git 信息可用时记录其提交。
工作树状态采用字符串 `true` / `false` / `unknown`；Git 不可用的本机构建提交为 `unknown`。
本地允许打包未提交改动，但其来源不能等同于记录的干净 Git 提交，正式发布要求干净工作树。
这不是可重现构建承诺；依赖更新仍可能影响产物，发布时应保留实际元数据。

## 验证与版本检查

```sh
python tools/package.py --check-version
python tools/package.py --check-version --tag v0.1.0-preview.1
python tools/package.py --verify build/packages/ciallang-0.1.0-preview.1-windows-x64.zip
```

`--verify` 只运行对应本机平台的压缩包，核对元数据、目录、许可、程序版本、示例、
有效/无效输入的 JSON 编译诊断。执行时切换到独立工作目录，PATH 仅保留系统路径，
清除开发期 dump 开关及动态库搜索环境变量。归档路径穿越、链接和特殊文件会被拒绝。
发布后下载的附件应先按 `.sha256` 或 `SHA256SUMS` 校验，再进行解压验收。

`--tag` 要求严格等于 `v<完整版本>`。`--require-clean` 拒绝含已跟踪或未跟踪改动的工作树；
两者同时使用时还要求标签指向当前 HEAD。工具不会创建标签。版本/清理/归档及输出检查
回归位于 `tests/test_packaging.py`，通过 `CiallangPackaging` 注册 CTest；CLI 测试也核对
`--version` 的实际输出与生成时版本一致。

## CI 与 Release

`.github/workflows/ci.yml` 在普通 push/PR 上构建测试，并保存 CLI 构建产物和测试报告；
这些是 Actions 验收附件，不等于经过完整打包验收的公开发行包。

`.github/workflows/release.yml` 提供两种入口：

| 触发 | 行为 |
|---|---|
| `workflow_dispatch` | 校验版本/工作树，三平台构建、测试、打包及解压验收，上传验证产物；不创建 Release |
| 推送 `v*` 标签 | 额外校验标签版本及提交；所有平台成功后才上传到新建的 GitHub Release 草稿 |

发布作业校验各附件的 SHA-256，再生成汇总 `SHA256SUMS`。有预发布后缀时标记 prerelease；
首版因此是预览版草稿。只有创建草稿的汇总作业拥有 `contents: write` 权限，普通构建只读。
已存在同名 Release 时不自动覆盖。草稿中的初始说明来自 `packaging/RELEASE_NOTES.md`，
对外公开前核对已验证的平台、附件、限制和本次改动。

首次发布的推荐顺序：提交发布基础及路线图 → 手动运行三平台流程 → 检查产物与报告 →
更新发布说明 → 在确定的提交上创建并推送版本标签 → 核对生成的草稿后公开。
本地验证通过不代表未运行的远端矩阵已经通过。

机制参考：[CMake CPack](https://cmake.org/cmake/help/v3.25/module/CPack.html)、
[GitHub Releases](https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases)。
