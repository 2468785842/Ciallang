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


## 2026-10-30 原生接收者拷贝候选验证（未采用）

基线为 `7ad212a`，Windows x64、Clang 19.1.5、`-O3 -DNDEBUG`、静态 CRT；
JIT、尾递归与执行记录关闭。候选只把 `src/types/NativeFunDetail.hpp` 的
`invokeCallable` / `invokeCallableImpl` 接收者改为 `const Value &`，消除转换层的
两次中间拷贝；`src/types/Function.hpp` 的 `NativeFn` 按值协议与入口快照保持不变。

停止构建后，每项预热一次，交替运行前/后 Release 二进制九轮，逐轮反转先后顺序，
记录含进程启动的 wall-time 中位数；第二批反转起始顺序。所有脚本均通过自校验。
`native_stress` 为 `bench/native_methods.tjs` 的一百万次迭代变体，校验和改为
499999500000；其余脚本直接使用 `bench/*.tjs`。

| 脚本 | 第一批前 ms | 第一批后 ms | 后/前 | 第二批前 ms | 第二批后 ms | 后/前 |
|---|---:|---:|---:|---:|---:|---:|
| class_instantiate | 10.541 | 10.681 | 101.3% | 10.704 | 10.210 | 95.4% |
| fib | 43.597 | 46.467 | 106.6% | 45.709 | 46.038 | 100.7% |
| loop_int | 358.858 | 377.552 | 105.2% | 351.054 | 369.020 | 105.1% |
| native_methods | 23.857 | 26.426 | 110.8% | 23.553 | 22.814 | 96.9% |
| string_concat | 16.102 | 15.851 | 98.4% | 16.265 | 16.070 | 98.8% |
| native_stress | 384.356 | 348.367 | 90.6% | 361.627 | 334.843 | 92.6% |

百万次原生调用耗时下降 7.4%～9.4%，五万次原生调用则跨批波动，无法给出稳定收益。
整数循环在两批中均慢约 5.1%～5.2%，因此本轮候选未采用，生产代码恢复基线；
表中是被撤回候选的数据，不代表当前提交的性能。没有进行新的 TJS2 对照，整体
“原版性能追平”任务保持未完成。下一步优先评估保持回调协议的函数指针快路径，
并继续覆盖这五项基准，不能仅凭原生调用单项收益接受默认执行回退。

新增 `tests/native_function_test.cpp` 接收者快照测试：长串 this 在重新绑定及嵌套
`callProc` 后保持原值，按值宿主回调修改自身副本不改变绑定；测试在原有实现上通过。
候选阶段 Windows Debug、Release 的完整 CTest 均通过，C++ 套件为 214 个用例、
1550 条断言；撤回后 Debug、Release 完整 CTest（C++、CLI、LSP）再次全部通过。

完整样本及二进制 SHA-256 位于本机忽略目录 `bench/results/` 下的
`20261030-native-receiver-final1.json`、`20261030-native-receiver.json`。

## 2026-10-30 NativeFn 函数指针化批次

环境：Windows x64、Clang 19.1.5、`-O3 -DNDEBUG`、静态 CRT；JIT、尾递归与执行记录关闭。
基线为批次前 Release 二进制（改动前 `git` 工作树构建，留档 `bench/results/cial-baseline.exe`）。
方法：每项预热 1 轮后交替运行前/后二进制 7 轮、逐轮反转先后顺序，统计含进程启动的
wall-time 中位数；比值 = 改动前中位数 / 改动后中位数（>100% 即改动后更快）。样本落盘
`bench/results/20261030-*.json`。

改动（语义零变化，全部注册面不受影响）：

1. **无捕获回调函数指针化**（`src/types/Function.hpp`、`src/types/NativeFunDetail.hpp`、
   `src/types/Function.cpp`）：NativeFunction 增加 small-closure 快路径
   `{NativeThinFn _thin, void *_thunkCtx}`——构造点按 callable 具体类型静态实例化
   `native::nativeThinTrampoline<F>`，stdlib 注册面（`&stdlib::fn` 函数指针与无捕获
   lambda，覆盖 Array/String/Math/Date/RegExp/Serialize/Random 全部方法）构造时不再
   包装 std::function；调用经两指针直派发，接收者以 `const Value &` 跨 ABI 直传
   （零拷贝）。有捕获回调保持 `NativeFn`（std::function 按值协议）不变；`invokeCallable`
   共用，实参提取/void 返回适配/按值接收者副本语义一致。
2. **`VM::callValue` 原生分支栈缓冲**（`src/vm/VM.hpp`）：宿主回调实参从每次
   `Vec<Value>` 堆分配改为 placement new 栈直传（≤64 实参，缺省补 void，超宽回落堆
   缓冲）；GC 根仍由 `argumentRoots` 承接。基准语料不覆盖此路径（受益者为
   Array.sort 比较器、RegExp.replace 回调等宿主回调）。

同机对照（预热 1 轮 + 交替 7 轮取中位数比值；轮 1–3 为批次中间态=仅改动 1，
轮 4 为最终态=改动 1+2，五项基准在两态下的基准路径相同）：

| 脚本 | 轮1 | 轮2 | 轮3 | 轮4（最终态） | 结论 |
|---|---:|---:|---:|---:|---|
| native_methods | 108.7% | 110.0% | 108.8% | 111.3% | 稳定 +9~11%，远超噪声 |
| class_instantiate | 102.6% | 100.2% | 101.8% | 97.0% | 噪声带内 |
| fib | 104.8% | 101.8% | 103.7% | 101.7% | 噪声带内（基准不含原生调用） |
| loop_int | 100.2% | 95.2% | 98.8% | 102.7% | 噪声带内（见下 A/A 校准） |
| string_concat | 97.3% | 102.0% | 102.1% | 102.5% | 噪声带内 |

噪声校准：同二进制 A/A 对照两轮为 class 98.9%/100.1%、fib 99.0%/98.1%、
loop_int 99.0%/**93.8%**、native_methods 98.3%/97.9%、string 100.6%/100.0%——本机
单批中位数噪声可达 ±6%（loop_int），故轮 2 的 95.2% 不足以判回退；native_methods
的 +9~11% 在全部轮次（含后台繁忙批次的 106.5%/108.6%）一致复现，接受改动。
绝对值口径：改动前基线（安静批次）native_methods 25.136/24.936 ms → 最终态 23.074 ms
（由轮 4 比值折算）；跨批次绝对值受负载漂移影响，以上述交替比值为准。

**被否决的中间方案**：`invokeCallV` 原生分支接收者改引用绑定（`const Value &recv`
三元左值，消一次 16B 拷贝）。与改动 1 叠加后直接对拍（两改动态互为 A/B，7 轮）：
native_methods 100.9%（拷贝版反而略快，噪声带内），未复现收益——与上一节被否决的
转换层借用快照同因：接收者拷贝本身廉价（对象值 16B 指针拷贝），引用化引入的间接
加载抵消收益。已回退为按值拷贝原形。

**GMethodC+CallMethodV 合并单指令（未实施）**：核结是发射形态不支持无条件融合——
`IRGenerator.cpp` 的方法调用先改写 GPropC→GMethodC（方法加载），随后**逐个实参
表达式求值**，最后才发射 CallMethodV（`src/gen/IRGenerator.cpp:699-770`），二者之间
存在任意脚本代码（副作用/异常/方法表替换），融合需跨任意语句的副作用分析或仅限
零实参调用点的专用指令（预计收益 ~5%，成本为指令集四处同步+异常点重验证）。
自然后续是把零实参情形折入现有 GPropC→GMethodC 就地改写模式，另批验证。

验证：Windows Debug/Release 全量构建通过；完整 CTest 4 套件全绿
（C++ 224 用例/2168 断言，与批次前一致）。样本与工具位于本机忽略目录
`bench/results/`（`bench_median.py` 为本轮使用的 A/B 中位数编排脚本）。

## 2026-10-30 GMethodC+CallMethodV 零实参融合批次（未采纳，已回退）

环境与上批一致：Windows x64、Clang 19.1.5、`-O3 -DNDEBUG`、静态 CRT；JIT、尾递归与
执行记录关闭。基线为 `86a5b11`（HEAD）Release 构建（留档
`bench/results/cial-fusion-before.exe`，SHA-256 e3d6d896…）。

实现形态（已整体回退，此处留档供后续批参考）：新指令 `CallMethod0`（TAC/扁平字节码
第 96 条，`op + u16 obj + u32 atom + u16 func + u16 receiver + u16 dst`），发射期在
ProcCallExprNode 实参循环结束后若 `argRegs` 为空且尾指令仍是 GMethodC 则就地改写——
零实参调用点两条指令之间无任何用户代码。语义逐项核对：方法缺失报错文本/行号、异常
传播、GC 保根（方法值与接收者先落寄存器槽再进 invokeCallV，与两指令序列同协议）、
Rlc 语句级行号（改写不位移指令下标）、Optimizer forEachRegisterOf/拷贝传播 def 杀灭、
SSA def-use 登记全部同步；tjsbc 转译面不发射该指令（grep 证实），字节码样本回归不受
影响。测试/语料在融合状态下亦全绿。

同机 A/A 校准（同二进制自对照）：两批 94.7%~117.5%（string_concat 单批 117.5% 为
当机漂移），安静时段补测 100.2%。本机单批噪声 ±6% 起，跨批 ±10%；同日无方法调用
的 fib/loop_int 在 A/B 批次中摆动 92.5%~112.2%，即漂移界。

五项基准 A/B 三批（前/后交替 7 轮取中位）：native_methods 101.7%/98.9%/96.8%、
fib 104.6%/100.4%/112.2%、loop_int 105.6%/102.5%/98.7%、class 99.5%/98.3%/111.3%、
string 101.2%/114.3%/99.0%——全部在噪声带内且与对照脚本同摆，无可归因信号。

受控归因（放大语料 `bench/results/native_stress.tjs`，1e6 次 push/pop，pop 为零实参
融合路径；script_method_stress 为 2e6 次零实参脚本方法）：

1. 同源对照 C（融合指令保留、仅发射面禁用就地改写）vs 融合 B：13 轮 B/C=105.7%——
   初版 dispatch（布尔标志合并两个 invokeCallV 调用点）存在真实代码生成病理。
2. dispatch 改写为 GMethodC 同款早返回双调用点形态后 B2/C=98.7%（13 轮 10 胜），
   病理消除。
3. A/B2/C 三方同场：**C/A=102.4%——仅向 VMState.cpp 新增 dispatch 函数（不执行）
   就使主循环热路径布局损失 ~2.4%**；单次调用省下的一个主循环迭代（估 5~8ns，
   <170ns/次调用的 5%）不足以覆盖。
4. 安静时段决胜（中位 + 逐轮胜率）：native_methods B2/A=102.7%（B2 胜 6/15）、
   native_stress 101.0%（7/13），min 口径三方持平（±0.3%）；script_method_stress
   B/A=99.3%（回落路径融合亦无可测收益）。

结论：**零实参融合在五项基准无可测收益，放大语料相对基线慢 1~3%，按预登记规则
整体回退**（指令、编码、dispatch、发射改写与 Opt/SSA 同步全部撤销），工作树回到
`86a5b11`。被否决的中间方案：①布尔标志合并形态的 dispatch（+5.7% 病理，早返回
形态修复 7 个百分点，说明融合指令的分支形态本身比指令数更关键）；②"复用现有指令+
调整 invokeCallV 快路径"路线未展开——invokeCallV 的 Function 直写/Native 免池快路径
上批已就位，剩余开销在调用帧建立而非指令派发。经验：评估新增指令必须做"保留代码、
禁用发射"的同源对照，dispatch 主循环所在翻译单元对代码布局极敏感。

### 同场 TJS2 三方对照（独立交付物）

TJS2 为当日 `windows-release` 重建（工作树含本地适配，clang++ -O3，非 2026-10-26 旧
二进制）。口径：预热 1 轮 + 三引擎交替 7 轮取中位，含进程启动与 TJS2 每脚本固定的
上下文转储输出（其 stderr "Time taken by function" 标签实为微秒，不采用）。

| 脚本 | HEAD 中位 ms | 融合构建中位 ms | TJS2 中位 ms | HEAD/TJS2 中位 | HEAD/TJS2 min 口径 |
|---|---:|---:|---:|---:|---:|
| class_instantiate | 14.95 | 14.64 | 16.81 | 88.9% | 81.4% |
| fib | 56.34 | 52.13 | 50.21 | 112.2% | 113.8% |
| loop_int | 423.70 | 385.16 | 771.92 | 54.9% | 51.6% |
| native_methods | 24.79 | 28.05 | 27.54 | 90.0% | 92.0% |
| string_concat | 19.52 | 20.59 | 26.15 | 74.6% | 74.4% |

融合/HEAD 列受同场漂移影响（无方法调用的 fib/loop_int 摆动 90.9%~112.2%，即单场
漂移界；融合批已被否决，该列仅留档）。HEAD/TJS2：class/loop/string/native 反超，
fib 仍落后 ~13%；与 2026-10-29 批次（native 142%）相比 native_methods 大幅收窄——
归因于 Ciallang NativeFn 批次收益与本场换用当日重建的 TJS2 二进制（其 native 耗时
较 10-26 版升高），两个变量同向，比值不可与历史批次直接对齐。

验证：回退后 Release/Debug 重建，CTest 4 套件全绿（230 用例/2159 断言与基线一致）；
tjs2 语料 7 脚本 + tjsbc 字节码样本（仓库现存 7 个）rc=0；ASan 全量（MSVC
/fsanitize=address，回退后 HEAD 状态重建）230/2159 全绿、语料零报告。样本 JSON：
`bench/results/20261030-*.json`（aa-fusion×2、ab-fusion×3、threeway-fusion-rejected）。

## 2026-10-30 原生绑定层重构对照

本轮基线为同机已构建的“开放 NativeTypeKey 分派第一阶段”Release CLI（保存在
`/tmp/cial-before-native-binding`），候选为新增声明式绑定、NativeAccessor、标准库统一
注册后的 Release CLI。不是相对于原始 HEAD 或 TJSCompiler 的比较。
macOS arm64，同一套 `bench/*.tjs`，每项预热 1 次、7 次交替执行取进程 wall-time
中位数，独立重复两轮；脚本自身校验均通过。比值为候选/基线，低于 100% 更快。

| 语料 | 第一轮 | 第二轮 |
|---|---:|---:|
| class_instantiate | 99.6% | 102.0% |
| loop_int | 103.4% | 102.6% |
| fib | 98.9% | 101.0% |
| native_methods | 101.0% | 99.1% |
| string_concat | 102.4% | 103.0% |

原生方法在两轮中近似持平；整数循环和字符串存在约 2.4%–3.4% 的耗时回退，尚未定位
原因，不作“性能无回退”结论。新增功能保留无捕获 NativeFunction 快路径和
GMethodC/CallMethodV 的接收者分离；访问器缓存增加 getter/setter 信息，实际影响需
后续 profile 区分布局变化与派发成本（`src/vm/VMState.hpp/.cpp`、
`src/stdlib/NativeRegister.hpp`、`src/types/Function.hpp`）。

### 同日 CLion 检查后的整理与查询合并

对照基线为完整绑定重构、CLion 整理之前的 Release CLI
（`/tmp/cial-before-ide`），候选为整理后的同机 Release。各项预热 1 次、交替执行
7 次取进程 wall-time 中位数；与上表的基线不同，不能直接拼接成累计收益。

| 语料 | 整理前 ms | 整理后 ms | 后/前 |
|---|---:|---:|---:|
| class_instantiate | 3.985 | 4.038 | 101.3% |
| loop_int | 179.146 | 174.699 | 97.5% |
| fib | 26.012 | 26.008 | 100.0% |
| native_methods | 10.868 | 10.784 | 99.2% |
| string_concat | 8.290 | 8.081 | 97.5% |

`NativeRegister::findMember` 将原生方法、只读 getter、访问器的三次类型表定位合并为
一次，供普通对象成员读取与 VM 调用点缓存填充使用；成员优先级与注册版本失效不变
（`src/stdlib/NativeRegister.hpp`、`src/runtime/Context.hpp`、`src/vm/VMState.cpp`）。
同时去除临时 Value 与重复函数适配代码（`src/types/RegExp.hpp`、
`src/stdlib/RegExpLib.cpp`、`src/Binding.hpp`）。本轮比值范围为 97.5%–101.3%；单轮
短脚本结果不用于声称稳定提速，也不撤销上一节尚待定位的回退记录。

## 2026-10-31 调用参数索引清零候选（未采纳）

Windows x64、Clang 19.1.5、`-O3 -DNDEBUG`、静态 CRT，JIT 关闭。
基线为本批 CHKINS、常量容器及汇合传播实现后的 Release CLI
（`bench/results/cial-todo-semantic.exe`）；候选仅取消 CallV/CallMethodV/TailCallV
参数索引数组的全槽清零，实际 argc 个索引仍逐个解码赋值。Value 参数副本及回调协议不变。

每项预热 1 次，11 轮交替、反转顺序，统计含启动 wall-time 中位数。
第二轮数据（候选/基线，小于 100% 更快）：

| 脚本 | 基线 ms | 候选 ms | 后/前 |
|---|---:|---:|---:|
| class_instantiate | 17.081 | 16.946 | 99.2% |
| fib | 43.208 | 43.539 | 100.8% |
| loop_int | 330.772 | 335.473 | 101.4% |
| native_methods | 21.488 | 21.582 | 100.4% |
| string_concat | 15.856 | 15.806 | 99.7% |
| native_todo_stress | 301.949 | 316.802 | 104.9% |

压力语料是 native_methods 的百万迭代变体，校验和 499999500000；所有脚本自校验通过。
首轮与参考侧复测重叠，仅作探索，不用作收益证据。第二轮没有稳定收益信号，候选已撤回，
生产 VMState.cpp 未保留此次改动。原始第二轮样本及双方 SHA-256 位于
`bench/results/20261031-todo-native.json`。这些数据不是本批最终实现相对原始 HEAD 的加速比，
也不是与 TJS2 的对照，不能据此宣布原生调用性能任务完成。

历史绑定层 2.4%–3.4% 回退发生于 macOS arm64，且文档基线为本机 `/tmp` 二进制；
本 Windows 工作区没有该二进制、原始采样或对应 macOS 执行环境。现有证据不足以将其归因于
布局或派发；任务继续保留，须取得原基线或可重建的精确提交，做同平台 A/A、A/B 与采样。

### 本批最终 Release 快照复核

全部构建及测试停止后，最终 Release 与任务开始前保留的现有 Release CLI 交替比较，
预热 1 次、11 轮反转顺序，含启动 wall-time 中位数；同上 Windows/Clang 19.1.5 配置。
基线是既有二进制快照，不宣称能从某个精确 Git 提交重建；双方 SHA-256 和原始样本见
`bench/results/20261031-todo-final.json`。压力语料同上，全部自校验通过。

| 脚本 | 前 ms | 后 ms | 后/前 |
|---|---:|---:|---:|
| class_instantiate | 11.328 | 11.309 | 99.8% |
| fib | 44.582 | 45.767 | 102.7% |
| loop_int | 346.859 | 340.180 | 98.1% |
| native_methods | 25.024 | 24.042 | 96.1% |
| string_concat | 16.608 | 16.428 | 98.9% |
| native_todo_stress | 313.618 | 315.017 | 100.4% |

此单批快照的比值范围约 96.1%–102.7%，没有做本轮独立 A/A 校准，不据此宣称稳定提速
或完全无回退。它也不能定位历史 macOS 绑定层回退。最终未保留调用索引清零候选。

最终验证：Windows Clang Debug/Release 与 MSVC ASan 的 CTest 均 230/230 通过，
包含 225 个脚本用例、C++、CLI、LSP 与字节码工具套件。MSVC 编译时将宿主绑定的
内部 invoke 调用显式限定为 `cial::binding::invoke`，避免 ADL 引入 `std::invoke`；
Tracer 增加空对象保护，并由 native_binding_test 的持有值/GC 用例验证。
