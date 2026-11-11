# 系统声明随构建/安装分发，LSP 改读 .d.tjs 文件

Status: implemented

## 问题与决定

基于 `2026-11-11-api-declarations-lsp`（LSP 声明消费），标准库声明此前由 LSP 在服务启动时
调用 `cial --api-decl` 现场生成到私有临时目录；SDK 安装也只分发 C++ 头文件，接口事实
仍要看注册源码。决定改为：构建期把 `--api-decl` 输出落盘为真实 `.d.tjs`，SDK 组件与
CLI 归档随安装分发，LSP 优先读取这些文件；接口与所用 cial 版本天然一致，
跳转落在持久文件上，脚本工具不再依赖 C++ 源码。

## 实际实现

- `cmake/GenerateApiDeclarations.cmake`：`--api-decl` → 输出文件（execute_process
  OUTPUT_FILE 原始字节落盘，不做换行转换），校验退出码与非空。
- `cial` 目标 POST_BUILD 把 `stdlib.d.tjs` 写到可执行文件旁（多配置与单配置均适用）。
- `cmake/SDK.cmake` / `cmake/Packaging.cmake`：把该产物与手写
  `tools/lsp/script-builtins.d.tjs` 安装到 `share/ciallang/declarations/`（SDK 组件 /
  Runtime 组件）。`install(FILES)` 的生成器表达式路径已用独立工程验证；无 CLI 的
  纯 SDK 配置没有产物，只安装手写声明（`if(TARGET cial)` 守卫）。
- `tools/lsp/decl_index.py`：`system_declaration_files` 依次发现 cial 旁的
  `stdlib.d.tjs`、`<prefix>/share/ciallang/declarations/*.d.tjs`（排序、去重、上限 16）；
  按工作区一致的 UTF-8 BOM 规则读取并经 `--decl-json` 解析，URI 指向真实文件。
  磁盘上没有系统声明文件时保留原 `--api-decl` 运行时生成作为回退
  （旧构建目录/裸 PATH 安装仍可用），回退代码移入 `generate_standard`。
- `tools/lsp/test_protocol.py`：原生声明导航测试断言跳转目标必须是 cial 旁或安装
  share 目录的真实文件（非临时目录）；新增发现顺序测试（构建产物优先于 share 目录，
  只看文件布局不执行编译器）与 `generate_standard` 回退的直接调用测试。

## 验证

本机 Windows 10.0.19045，Git Bash + Ninja，构建目录 `build/Windows/Debug`
（Debug，CIAL_BUILD_CLI/TESTS=ON，CIAL_INSTALL_SDK=ON，JIT 关）。

- `cmake --build build/Windows/Debug` 成功；POST_BUILD 生成
  `build/Windows/Debug/stdlib.d.tjs`（5908 字节，含 `class Array` 与文档注释）。
  vcpkg applocal 的 dumpbin 警告为既有噪音，与本次无关。
- `python tools/lsp/test_protocol.py --cial build/Windows/Debug/cial.exe`：29/29 OK。
- 全量 `ctest`：**250/250** 通过（105.8s，含 238 个 TJS、CiallangLspProtocol、
  CiallangCli、CiallangSuite、CiallangPackaging）。
- `cmake --install build/Windows/Debug --prefix out/sdk-install-check`：
  `share/ciallang/declarations/{stdlib.d.tjs, script-builtins.d.tjs}` 落盘，内容正确。
- 安装布局端到端：构造 `out/runtime-layout/{bin/cial.exe, share/ciallang/declarations/}`，
  `DeclarationIndex.load_standard` 发现 share 布局两个文件及捆绑 script-builtins，
  Array/Exception 等符号齐全且 URI 指向安装文件；产物已清理。
- 未执行：真实 VSCode Extension Development Host 验收（JS 无改动）；
  CPack CLI 归档打包（本构建目录 `CIAL_ENABLE_PACKAGING=OFF`，Runtime 组件安装语法
  与 SDK 相同且 genex/安装布局已验证）；Release/JIT 矩阵（无 C++ 改动，由远端 CI 覆盖）。
- 无 C++ 改动，`tools/check-format.sh` 不适用。

## 保留边界

`--api-decl` 回退路径保留，因此旧行为在新声明文件缺失时仍会出现（URI 为服务临时目录）；
声明/实现一致性检查、独立冲突诊断与重载选择仍未实现（见 roadmap §3.2 剩余项）。
