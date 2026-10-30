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

### 2026-10-29 拷贝传播批次

新增 CopyPropagationOptimizer（GLocal/DLocal 纯拷贝在使用点改写为源寄存器 +
DEL 扩展死拷贝清除，管线首位）后同机三轮：**fib 从 122~132% 降至 108~120%**
（fib 函数块 14→10 条指令，每次调用省 3 条变量读拷贝，`--dump-cfg` 可复核）；
其余四项均在既有噪声带内（class 96~102% / loop 47~49% / native 148~149% /
string 84~85%），无回退。至此 fib 与 TJS2 同机比值首次进入 1.1× 以内，
剩余差距在调用帧建立与参数窗口路径（CallV 实参直写已做，下一步可评估
调用点参数寄存器直映）。

## 2026-10-29 保留字语法批次后的 macOS 复核

语言批次（`in`/类型注解/`new` 检查均为解析期改动，执行路径不变）后同机复测：
fib 126~132% / native_methods 174~183% / loop_int 68% / string_concat ~100% /
class_instantiate ~98%（预热 1 轮 + 交替 5~11 轮取 min，含进程启动），与
2026-10-28 批次一致，无回退。native_methods 仍为最大缺口；采样（`sample`，5M
迭代变体，861 样本）显示：解释器 dispatch 约 66%、`NativeFunction::callWithThis`
调用链约 21%、Value 拷贝/析构约 12%；`asArray` 的 `dynamic_cast`（libc++abi
RTTI 遍历）约 2%。据此把 Array/Dictionary/RegExp 三处接收者辅助改为 `typeId()`
直判 + `static_cast`（final 类，二者等价，`src/stdlib/ArrayLib.cpp` 等）——
严格更优但端到端在噪声带内（177% vs 182%），证实剩余差距在调用帧建立与
实参窗口路径而非类型判断，与下文"下一步可评估调用点参数寄存器直映"结论一致。

### 2026-10-29 调用路径批次：fib 首次追平

采样定位（`sample`，native_methods 5M 迭代变体）出三大开销并逐项削减：

1. **原生同步调用的主循环重取**：`DispatchRet::Call` 一律 `goto FETCH_NEW_FRAME`
   重算 chunk/regs/ip/toBytecode 检查，但 NativeFunction 回调在 dispatch 内同步
   完成、帧未变——新增免池同步快路径返回 `Continue` 直接续内层取指
   （`src/vm/VMState.cpp` invokeCallV）。
2. **热路径 RTTI 遍历**：`loadGlobal` 对每个对象型全局读、`propObjectGet/Set` 对
   每次成员访问都做 `dynamic_cast<Property*>`（libc++abi 遍历）。TypeId 增加
   `NativeFunction`/`Property` 专有值（`src/types/TypeId.hpp`），六处 dynamic_cast
   换一次虚调用 typeId 直判（final 类等价）。
3. **实参池中转**：原生调用实参 N×push 压池 → getArgs 读回；改为 placement new
   到调用点栈缓冲直传回调（`NativeFunction::callDirect`，`src/types/Function.*`），
   缺省参数补 void 布局与池顶约定等价。注意 Value 具有非平凡构造/析构，栈数组
   必须只构造实际数量（初版 `Value localArgs[64]` 默认初始化全槽曾致回退 256%）。
   实参对象的 GC 根仍在调用者寄存器槽（回调期间不被覆盖），栈副本无需注册根。

同机对照（预热 1 轮 + 交替 5 轮取 min，含进程启动）：

| 脚本 | 改前 | 改后 | TJS2 | 后/TJS2 |
|---|---:|---:|---:|---:|
| fib | 126~132% | — | — | **94~97%（首次 ≤100%）** |
| native_methods | 174~183% | — | — | **135~140%** |
| loop_int | 68% | 68% | — | 无回退 |
| string_concat | ~100% | ~100% | — | 无回退 |
| class_instantiate | ~98% | ~96% | — | 噪声带内 |

fib 收益主要来自 loadGlobal 的 RTTI 消除（每次递归读全局 fib 名字）；native_methods
剩余缺口的大头在 NativeFn 回调协议（std::function 间接调用 + Value 拷贝语义），
结构性改造（函数指针化/寄存器直读协议）记入 TODO.md 待后续。

实现与回归来源：`src/gen/Optimizer.cpp`（比较融合）、`src/vm/VMState.cpp`（缓存和整型槽读写）、
`src/types/Value.cpp` / `String.hpp`（紧凑短串）、`src/types/Function.cpp`（objthis 分离）、
`tests/optimizer_test.cpp` / `native_function_test.cpp` / `value_storage_test.cpp`。

验证：macOS Debug、Release、UBSan 的 CTest 各 120/120 通过（2026-10-28 批次时点）；
Windows Debug CTest 143/143 通过（2026-10-29，4 个历史测试文件回归主线后）；12 个字节码样本全部成功；从其他工作目录配置的 TJSCompiler 相对路径检查通过；Release 空优化标志配置被拒绝。
ASan 在测试进程进入 main 前卡于系统运行库初始化，采样链为 AsanInitInternal →
InitializeShadowMemory → malloc → AsanInitFromRtl 自旋，因此未获得 ASan 测试结果。
Windows 静态 triplet 与 Linux/Windows CI 尚未在本机执行，工作流配置见 `.github/workflows/ci.yml`。


## 2026-10-29 README 开发批次的默认路径复核

Windows x64、Clang 19.1.5、`-O3 -DNDEBUG`、静态 CRT；可选 JIT、尾递归与执行记录均关闭。
对照是本次开发中加入尾递归/JIT 之前保存的 Release 二进制（已经包含执行记录功能），
不是本轮全部改动前的 Git 提交。每项预热一次，随后交替运行前/后两个二进制五轮，
每轮反转先后顺序，统计含进程启动的 wall-time 中位数。脚本 `bench/*.tjs` 均通过自校验。
测量前确认本机没有正在构建的编译器进程，clangd 连续三秒无 CPU 活动；后台繁忙期间的样本不用于下表。

| 脚本 | 对照 ms | 当前 ms | 当前/对照 |
|---|---:|---:|---:|
| class_instantiate | 12.003 | 12.297 | 102.4% |
| fib | 48.079 | 45.414 | 94.5% |
| loop_int | 387.752 | 381.368 | 98.4% |
| native_methods | 31.434 | 33.552 | 106.7% |
| string_concat | 17.791 | 18.594 | 104.5% |

结果仍不能支持“默认执行全面无回退”或“已追平 TJS2”的结论；原生调用项慢约 6.7%，
字符串项慢约 4.5%，需后续定位和复测；fib 与整数循环本轮较快。完整样本保留于本机
`bench/results/20261029-readme-default.json`，二进制 SHA-256 记录于同名 `.metadata.json`
（该目录按既有约定不入库）。本表只比较默认执行，不代表可选 JIT 或尾递归的加速比。
为了避免每条指令和每次普通调用承担开关判断，`VMState::run()` 在进入时选择模板化
解释循环；尾递归仅显式启用，可选模式状态放在类末尾以保留已有热字段布局。
实现见 `src/vm/VMState.cpp`、`VMState.hpp`；可选后端范围见 `doc/jit.md`、`doc/tail-recursion.md`。
