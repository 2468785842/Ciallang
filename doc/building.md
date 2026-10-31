# 构建与验证

要求 CMake 3.25+、C++20 编译器、Ninja（预设使用）与 vcpkg。预设从 `VCPKG_ROOT` 读取工具链；Linux 预设使用 clang/clang++。已有依赖也可通过 `CMAKE_PREFIX_PATH` 提供，无需 vcpkg（`CMakeLists.txt`、`CMakePresets.json`）。

```sh
export VCPKG_ROOT=/path/to/vcpkg
cmake --preset "Linux Debug"
cmake --build --preset "Linux Debug"
ctest --preset "Linux Debug"
```

平台预设为 `Linux`、`MacOS`、`Windows` 的 Debug/Release。Windows 使用静态 CRT 与 `x64-windows-static`。也可直接指定生成器与编译器：

```sh
cmake -S . -B build/local -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build/local --parallel 4
ctest --test-dir build/local --output-on-failure
```

## 可选目标与依赖

| CMake 开关 | 独立构建默认 | `add_subdirectory` 默认 | vcpkg feature / 额外依赖 |
|---|---|---|---|
| `CIAL_BUILD_CLI` | ON | OFF | `cli` / argparse |
| `CIAL_BUILD_TESTS` | ON | OFF | `tests` / Catch2（不启用其默认 feature） |
| `CIAL_ENABLE_JIT` | OFF | OFF | `jit` / asmjit，仅 x64 |
| `CIAL_BUILD_FUZZ` | OFF | OFF | 无；需 Clang/libFuzzer |
| `CIAL_BUILD_BENCH` | OFF | OFF | 无；要求 CLI 开启 |
| `CIAL_WARNINGS_AS_ERRORS` | ON | OFF | 无；只影响本项目目标 |

基础依赖为 fmt、frozen、utf8proc。开关在 `project()` 之前映射到 `VCPKG_MANIFEST_FEATURES`，避免库模式安装 CLI/测试/JIT 依赖；spdlog 未被代码使用，已移除。独立构建时，原 `BUILD_EXECUTABLE`、`ENABLE_TESTS`、`ENABLE_FUZZ` 在新开关未设置时作为初始兼容值读取；两者都设置时以 `CIAL_*` 为准；子项目不读取父项目的这些通用旧开关（`CMakeLists.txt`、`vcpkg.json`）。

```sh
# 只构建库
cmake -S . -B build/library -DCIAL_BUILD_CLI=OFF -DCIAL_BUILD_TESTS=OFF
# 显式启用 JIT
cmake --preset "Linux Release" -DCIAL_ENABLE_JIT=ON
```

已有 CMake 缓存会保留开关值；切换模式时显式指定或使用新构建目录。多配置生成器用 `cmake --build ... --config Release` 和 `ctest ... -C Release`。

## 测试与基准

C++ 测试由 Catch2 注册；Python 可用且 CLI 开启时，CTest 还运行 `tests/cli_test.py` 和 `tools/lsp/test_protocol.py`。测试从顶层启用，可在整个构建目录运行（`tests/CMakeLists.txt`）。

语言和标准库行为已迁到 [TJS 脚本单元测试](../tests/tjs/README.md)，启用 CLI 和 Python 时
逐文件注册为 CTest；原生绑定、AST、GC 及宿主接口仍使用 Catch2。脚本可直接交给
Ciallang 或 TJSCompiler，也可用 `python tests/run_tjs_tests.py --engine <executable>` 批量运行。
可选 `CIAL_TJS_TEST_REFERENCE` 注册参考引擎测试；参考侧失败会如实使 CTest 失败，不默认启用。

```sh
cmake -S . -B build/bench -DCMAKE_BUILD_TYPE=Release -DCIAL_BUILD_BENCH=ON \
  -DCIAL_TJS_EXE=/path/to/TJSCompiler
cmake --build build/bench --target bench
cmake --build build/bench --target conformance-report
```

参考引擎默认从 `../TJSCompiler` 探测，也可设置 `CIAL_TJS_ROOT` 环境变量或 `TJS_ROOT` 缓存项；报告放在 `bench/results/`。性能记录与统计口径见 [performance.md](performance.md)。

参考程序必须支持脚本路径和所需导出协议，使用前按 [FAQ](faq.md#对拍前怎样确认参考引擎可用) 确认入口能力。入库字节码 fixture 不依赖参考引擎。

严格源码/字节码对拍可独立运行，无需启用 bench 构建选项或安装参考引擎：

```sh
python bench/bytecode_conformance.py --cial build/Windows/Debug/cial.exe \
  --report bench/results/bytecode-conformance.json
python bench/test_bytecode_conformance.py --cial build/Windows/Debug/cial.exe
```

工具逐字节比较两条 Ciallang 路径的 stdout，失败、超时或输出差异返回 1；已有
`test_misc` fixture 的诊断格式与行号差异也会如实报告，不能把该命令默认等同于“全绿”。
`--case` 可筛选用例；fixture 必须有源码（s1–s5 无源码样本已清理，缺源码报
`TOOL_ERROR`），`--reference` 可使用经随机脚本及导出执行探测的参考 CLI 从源码生成临时
字节码，入库 fixture 不受影响。JSON 含退出码、无损输出与 SHA-256；详见
[字节码转译 §9](bytecode/mapping.md#9-对拍与工具链) 和 `bench/bytecode_conformance.py`。

## ASan（Windows MSVC）

Windows 下可用的 ASan 组合是 MSVC 前端 `/fsanitize=address`（其运行库完整支持
SEH/C++ 异常）；构建目录 `build/Windows/ASanMsvc`，须在 vcvars 环境里配置：

```bat
call vcvars64.bat
cmake -B build/Windows/ASanMsvc -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
  -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows-static ^
  -DVCPKG_INSTALLED_DIR=<repo>/build/Windows/ASan/vcpkg_installed ^
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded ^
  -DCMAKE_CXX_FLAGS="/fsanitize=address /Zi /utf-8 /D_DISABLE_STRING_ANNOTATION=1 /D_DISABLE_VECTOR_ANNOTATION=1"
cmake --build build/Windows/ASanMsvc -j
```

要点：运行库必须与 vcpkg 依赖一致（静态 MT + `_DISABLE_{STRING,VECTOR}_ANNOTATION=1`
对齐未插桩依赖的容器注解）；部署需把
`<VC>/Tools/MSVC/<ver>/bin/Hostx64/x64/clang_rt.asan_dynamic-x86_64.dll`
复制到可执行文件旁。

Windows Clang ASan 与 C++ 异常的已知工具链问题见 [FAQ](faq.md#windows-asan-与异常处理)。

macOS ARM64 的 ASan 启动故障排查见 [FAQ](faq.md#asan-在-macos-arm64-上无法启动)。

## 有界 fuzz

使用 Clang 与 libFuzzer，单独建立带覆盖率、ASan/UBSan 的构建目录；核心库与入口均插桩，不影响其他构建。入口仅做词法/语法解析，不执行脚本（`fuzz/CMakeLists.txt`、`fuzz/fuzz_*.cc`）。

```sh
CC=clang CXX=clang++ cmake -S . -B build/fuzz -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCIAL_BUILD_FUZZ=ON \
  -DCIAL_BUILD_CLI=OFF -DCIAL_BUILD_TESTS=OFF
cmake --build build/fuzz --parallel 2
bash fuzz/run_fuzz_lexer.sh
bash fuzz/run_fuzz_parser.sh
```

运行脚本默认单进程、60 秒、单输入超时 5 秒、RSS 上限 1024 MB、输入上限 4096 字节；可在末尾传 libFuzzer 参数覆盖。RSS 检查不是操作系统硬限制；长期运行建议再使用容器内存/CPU 配额。`CIAL_FUZZ_BUILD` 可指定其他构建目录。语料与异常输入写入构建目录的 `corpus/` 和 `artifacts/`，不会覆盖源码。

```sh
# 重放一个异常输入；保留检测器报告
build/fuzz/fuzz/fuzz_parser build/fuzz/artifacts/parser/crash-<hash> -runs=1
# 更长的本地运行（10 分钟）
bash fuzz/run_fuzz_parser.sh -max_total_time=600
```

修复后应保存最小输入到 `tests/fuzz_regress/*.tjs`，并用 Catch2 回归测试锁定。短时间没有发现异常不代表输入安全性已被证明。
