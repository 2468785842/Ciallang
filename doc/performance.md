# 2026-10-28 TODO 批次性能与验证

测试环境：macOS arm64、AppleClang 17，两个引擎均为 `-O3 -DNDEBUG` Release。
对照基线为本轮 TODO 改动前的本机 Ciallang（已修复 macOS 编译入口），参考实现为兄弟目录
TJSCompiler，新 CLI 使用命令行脚本路径，不再依赖写死的本机路径。

每个脚本预热 1 轮，再交替执行三个引擎 7 轮，逐轮反转执行顺序，统计包含进程启动的 wall-time
中位数。所有基准脚本自校验，非零退出码即失败。本表反映整批改动，不能单独归因于某一优化。

| 脚本 | 改动前 ms | 改动后 ms | TJS2 ms | 后/前 | 后/TJS2 |
|---|---:|---:|---:|---:|---:|
| class_instantiate | 4.166 | 4.184 | 4.024 | 100.4% | 104.0% |
| fib | 29.565 | 27.711 | 18.761 | 93.7% | 147.7% |
| loop_int | 340.645 | 173.580 | 252.214 | 51.0% | 68.8% |
| native_methods | 21.845 | 15.890 | 8.564 | 72.7% | 185.5% |
| string_concat | 11.307 | 7.972 | 8.139 | 70.5% | 98.0% |

类实例化在进程启动噪声内持平，fib 和原生方法与参考实现仍有差距。采样曾发现合成空 finalize
导致大量无意义回调，现由 `FuncMeta::syntheticFinalizer` 排除；短串内联最终没有扩大 Value 的 16B 布局。

基准语料见 `bench/*.tjs`，原生调用密集语料为 `bench/native_methods.tjs`。
当前引擎与 TJS2 可用 `cmake --build build/MacOS/Release --target bench` 复测；该目标使用
`cmake/Bench.cmake` 的 min/mean 口径，与本表中位数口径不同。

实现与回归来源：`src/gen/Optimizer.cpp`（比较融合）、`src/vm/VMState.cpp`（缓存和整型槽读写）、
`src/types/Value.cpp` / `String.hpp`（紧凑短串）、`src/types/Function.cpp`（objthis 分离）、
`tests/optimizer_test.cpp` / `native_function_test.cpp` / `value_storage_test.cpp`。

验证：Debug、Release、UBSan 的 CTest 各 120/120 通过；12 个字节码样本全部成功；从其他工作目录配置的 TJSCompiler 相对路径检查通过；Release 空优化标志配置被拒绝。
ASan 在测试进程进入 main 前卡于系统运行库初始化，采样链为 AsanInitInternal →
InitializeShadowMemory → malloc → AsanInitFromRtl 自旋，因此未获得 ASan 测试结果。
Windows 静态 triplet 与 Linux/Windows CI 尚未在本机执行，工作流配置见 `.github/workflows/ci.yml`。
