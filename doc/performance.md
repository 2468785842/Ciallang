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

## 2026-10-29 Windows 首次实测

环境：Windows x64、MSVC 14.44 `-O3 -DNDEBUG`，对照同机 TJSCompiler `windows-release`
构建（2026-10-26）。`cmake/Bench.cmake` 编排：预热 1 轮 + 交替 7 轮取 **min**（含进程启动
wall-time），与本文件上表中位数口径不同，两引擎口径一致。提交 7fe3f84。

| 脚本 | TJS2 min | Ciallang min | Ciallang/TJS2 |
|---|---:|---:|---:|
| class_instantiate | 13.771 | 13.629 | 98% |
| fib | 70.129 | 80.441 | 114% |
| loop_int | 853.943 | 401.106 | 46% |
| native_methods | 21.108 | 30.169 | 142% |
| string_concat | 17.665 | 15.250 | 85% |

同机对照下 5 项中 3 项反超 TJS2（整数循环 2.2×、字符串拼接、类实例化），fib 与原生方法
仍落后（1.14× / 1.42×）。不同机器、编译器与口径的历史比例不可直接比较；上表 macOS 数据
为 wall-time 中位数。另：TJS2 CLI 的内部计时标签 "Time taken by function: Nms" 实为
微秒（fib(25) 标 31586"ms"，外部实测 52ms 含转储输出），对照时勿按标签字面取值。

### 2026-10-29 常量传播扩展后的复核

整数常量 pass 升级为块间状态分析（唯一前驱块继承出口状态）后同机复测三轮，比值
class 99~108% / fib 122~132% / loop 50~53% / native 148~164% / string 80~88%，均在
本机噪声带内：未改动的 TJS2 二进制 fib min 跨批次在 35.8~70.1ms 间摆动（2 倍），
绝对值与单轮比值不可比。`CIAL_OPTIMIZER_STATS=1` 证实 pass 对 fib 块 transforms=0
（代码路径未变），基准差异均为机器负载噪声。注意：当前 IR 形态下局部变量每次使用
经 GLocal 重载（VM_CP 语义），常量极少以寄存器形态跨块存活——跨块继承对现有脚本
收益有限，为后续槽位提升类优化打底（单元测试已锁定传播/多前驱/回边语义）。

实现与回归来源：`src/gen/Optimizer.cpp`（比较融合）、`src/vm/VMState.cpp`（缓存和整型槽读写）、
`src/types/Value.cpp` / `String.hpp`（紧凑短串）、`src/types/Function.cpp`（objthis 分离）、
`tests/optimizer_test.cpp` / `native_function_test.cpp` / `value_storage_test.cpp`。

验证：macOS Debug、Release、UBSan 的 CTest 各 120/120 通过（2026-10-28 批次时点）；
Windows Debug CTest 143/143 通过（2026-10-29，4 个历史测试文件回归主线后）；12 个字节码样本全部成功；从其他工作目录配置的 TJSCompiler 相对路径检查通过；Release 空优化标志配置被拒绝。
ASan 在测试进程进入 main 前卡于系统运行库初始化，采样链为 AsanInitInternal →
InitializeShadowMemory → malloc → AsanInitFromRtl 自旋，因此未获得 ASan 测试结果。
Windows 静态 triplet 与 Linux/Windows CI 尚未在本机执行，工作流配置见 `.github/workflows/ci.yml`。
