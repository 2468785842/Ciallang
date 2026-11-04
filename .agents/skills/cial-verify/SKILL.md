---
name: cial-verify
description: 为 Ciallang 的 C++、字节码或文档改动选择并执行构建与测试验收，核对实际注册用例、预设开关与结果证据；汇报验证结果或调查构建失败时使用。
---

# Ciallang 构建与验收

先读本次变更涉及的[构建指南](../../../doc/building.md)与模块 CMake，结合 git diff 选择范围。
已有构建目录是本机状态；运行前核对 CMakeCache 的预设、`CIAL_BUILD_*` 开关与 `VCPKG_ROOT`，
不要假设现有 build 目录就是完整验收配置。

## 选择范围

- 常规改动按平台预设配置、构建并回归：`cmake --preset "Windows Debug"`、
  `cmake --build --preset "Windows Debug"`、`ctest --preset "Windows Debug"`
  （Linux/MacOS 同名预设）。
- 语言与标准库行为放 `tests/tjs/`：启用 `CIAL_BUILD_TESTS`、`CIAL_BUILD_CLI` 且 Python
  可用时自动注册为 `CiallangTjs.*`，用 `ctest --test-dir <build> -L tjs --output-on-failure`
  运行；也可 `python tests/run_tjs_tests.py --engine <cial 路径> [文件或目录]` 单独运行。
  新增脚本无需修改 C++ 清单，但目录归属按功能分类，文件首行说明测试意图。
- C++ 内部状态（AST、编译诊断、Value API、GC/调用帧/优化器、原生注册与宿主回调）
  保留 Catch2（`tests/*_test.cpp`）；新测试必须显式登记进 `tests/CMakeLists.txt`，
  放入目录不会自动成为测试。
- 改 CLI、cial-dbg、cial-fmt 或字节码工具时运行对应 CTest（`cli_test.py` 等）。
  有界 fuzz 只在 Linux（含 WSL）运行；Windows 链接 `clang_rt` 后具名捕获
  `std::runtime_error` 会崩溃，属已验证工具链问题，见
  [FAQ](../../../doc/faq.md#windows-上为什么跑不了-libfuzzer-有界-fuzz)。
- 文档或技能调整只做路径、锚点与 diff 检查，不触发完整依赖构建。
- 性能修改需同机 Release 对照基线（[性能记录](../../../doc/performance.md)），
  并覆盖异常恢复、可观察结果与 GC 生命周期。

## 结果判定

先 `ctest --test-dir <build> -N` 核对预期用例，再用针对性 `-R` 与
`--output-on-failure --no-tests=error` 执行。
tests/tjs 的通过标准是进程成功退出、`__TJS_TEST_PASS__:断言数` 标记唯一且断言数大于零；
runner 如实报告差异，不自动跳过、不归一化断言结果。
构建失败后运行旧 exe 的 CTest 即使全绿也不算本次验证；核对日志与产物时间。
提交前 `tools/check-format.sh`（`-i` 就地格式化）只处理修改的文件；
CI 锁定 clang-format 20.1.0，勿用未锁定版本重排全库。

## 交付证据

记录实际命令、退出码、运行数量与筛选范围，说明未执行目标。
性能对照记录环境、基线与逐次测量；日志放 Git 忽略的 `out/`，
仍未解决的问题与下一步更新到对应 [note](../../notes/README.md)。
检查通过后，只有新改动、失败或未解疑点才重复或扩大测试。
