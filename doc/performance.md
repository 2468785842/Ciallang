# 性能记录

按批次保留测量环境、基线与结论；逐轮样本与详细过程见 git 历史与本机忽略目录
`bench/results/`。基准语料为 `bench/*.tjs`（自校验，非零退出码即失败），复测可用
`cmake --build build/MacOS/Release --target bench`（`cmake/Bench.cmake` 的 min/mean
口径）。各批次计时口径（中位数/min、轮数）以当节记录为准；不同机器、编译器与口径
的历史比例不可直接比较。

## 历史批次摘要（2026-10-28 ~ 2026-10-31）

- 2026-10-28 TODO 批次（macOS arm64、AppleClang 17、`-O3 -DNDEBUG`、中位数）：对
  TJS2 为 loop_int 68.8%、string_concat 98.0%、class 104.0%、fib 147.7%、
  native 185.5%；排除合成空 finalize（`FuncMeta::syntheticFinalizer`），短串内联
  因 Value 16B 布局未采纳。
- 2026-10-29 Windows 首次实测（MSVC 14.44、min 口径、提交 `7fe3f84`）：3/5 反超
  TJS2（loop 46%、string 85%、class 98%），fib 114%、native 142%。TJS2 CLI 的
  "Time taken by function: Nms" 标签实为微秒，对照勿按字面取值。
- 2026-10-29 调用路径批次：采样定位三项开销并削减——原生同步调用免 FETCH 重取
  （invokeCallV 的 Continue 快路径）、六处热路径 dynamic_cast 改 typeId 直判
  （`TypeId.hpp` 专有值）、实参栈缓冲直传（`callDirect`）；fib 首次 ≤100%
  （94~97%）、native 135~140%。注意 Value 栈数组只可构造实际数量（全槽默认
  初始化曾致 256% 回退）。
- 2026-10-29 常量传播跨块扩展与拷贝传播批次：跨块常量继承在现有 IR 下收益有限
  （局部变量每次使用经 GLocal 重载）；拷贝传播（纯拷贝改写 + DEL 清除）使 fib
  122~132%→108~120%，首次进入 1.1× 内。
- 2026-10-29 README 默认路径复核（Clang 19.1.5、Windows）：native +6.7%、
  string +4.5% 待定位，其余持平（后续批次逐步收窄）。
- 2026-10-30 原生接收者引用化候选：未采用——16B 接收者拷贝本身廉价，引用化的
  间接加载抵消收益（百万次原生调用 −7~9% 但 loop_int +5%）。
- 2026-10-30 NativeFn 函数指针化：small-closure `{_thin,_thunkCtx}` 两指针直派发 +
  `VM::callValue` 实参栈缓冲，native_methods 稳定 −9~11%，其余噪声带内，接受。
- 2026-10-30 GMethodC+CallMethodV 零实参融合（新指令 CallMethod0）：整体回退，
  五项基准无信号、放大语料慢 1~3%。教训：仅向 VMState.cpp 新增不执行的 dispatch
  函数即损失 ~2.4%（布局效应）；评估新指令必须做"保留代码、禁用发射"的同源对照，
  dispatch 所在翻译单元对代码布局极敏感。
- 2026-10-30 同场 TJS2 三方对照（当日重建参考二进制）：class/loop/string 反超
  （88.9%/54.9%/74.6%），native 90.0%，fib 落后 ~13%。
- 2026-10-30 原生绑定层重构对照：loop/string −2.4~3.4% 回退记录（次日定位轮归因
  布局效应）；同日整理将 `findMember` 三次类型表定位合并为一次（97.5~101.3%）。
- 2026-10-31 调用参数索引清零候选：未采纳，两轮均无稳定收益信号。
- 2026-10-31 绑定层回退定位（macOS）：二分至 `2a1d684`——recordFault 调用点改变
  runLoop 块布局（非算法成本），`CIAL_COLD_NOINLINE` 修复；残余 ~3% 为后续提交
  的链接布局漂移，不再追逐。方法：基线锚点重建到精确提交 + 提交级 A/B + 采样。
- 2026-10-31 Value 生命周期内联（macOS）：画像示 Value 生命周期 ~54% 样本、
  stdlib 实际工作仅 ~3%；生命周期成员类内定义 + `CIAL_INLINE` 后 native 78.2%、
  fib 87.4%、loop 94.1%，代价二进制 +19%。追加 asInteger/mod 快慢拆分，loop_int
  再 −6%；属性须写作 `[[nodiscard]] CIAL_INLINE`（顺序颠倒非法）。剩余候选见 TODO。

## 2026-11-01 macOS 双引擎对照（TJS2 d8044c3 vs HEAD，含 fib 差距画像）

macOS arm64；参考 `bin/darwin/TJSCompiler`（d8044c3，SHA `35169f56…537`，bench 语料
零输出，wall-time 纯执行）；本项目 HEAD（含 2026-10-31 全部内联批次）。交替配对
中位数 9 对 ×2 轮。与 2026-10-30 Windows 三方数字不可直接对齐（平台与参考构建不同）。

| 语料 | 参考 ms | 本项目 ms | 本项目/参考 | 备注 |
|---|---:|---:|---:|---|
| loop_int | 255.0–256.9 | 160.2–161.3 | **62.8%**（两轮一致） | |
| string_concat | 8.08–8.17 | 7.62–7.87 | 94.2%/96.4% | |
| native_methods | 8.53–9.92 | 8.25–9.81 | 96.7%/98.9% | 内联批次后基本追平（Windows 场为 90.0%） |
| class_instantiate | 3.64–3.80 | 3.54–3.57 | 93.9%/97.3% | 启动敏感，噪声带内 |
| fib | 18.91–18.92 | 23.19–23.39 | **122.7%/123.6%** | 唯一落后项 |

**fib 差距画像**（fib(35) 变体 `sample`）：libc++abi RTTI 链合计 ~32% 样本
（`type_info::operator==`、`__si_class_type_info::search_*`、`dyn_cast_*`、
`strcmp`）——派发循环内存在被内联的 `dynamic_cast` 调用点，fib 的纯整数递归
每层都触达；`CallFrame` 生命周期 ~9%（`setThisObj(Value)` 按值传递 55 样本、
构造/析构/释放 83）；`Ret` 返回整数直写已在位。RTTI 具体源点需 `-g` 采样定位
（候选：ChkInv/ChgThis 的 `dynamic_cast<DataObject*>` 或调用链其他点）；
下一批次方向：调用路径 RTTI 消除（TypeId 直判化剩余点）+ `setThisObj` 引用化。
（源点已采样定位并落地修复，见下文[thisData 缓存继承补丁](#2026-11-01-thisdata-缓存继承补丁macos-复测并落地)：
必败转换在绑定 this 分支，非 ChkInv/ChgThis。）

## 2026-11-01 语义对齐与 SDK 基础批次

同机 Linux x64、GCC 14.2，基线 `04dbd2e`，两侧 Release `-O3 -DNDEBUG`（JIT/尾递归
关闭，核心不强制 PIC），预热后交替 11 轮取含启动 wall-time 中位数。

| 用例 | 基线 ms | 本批 ms | 变化 |
|---|---:|---:|---:|
| fib | 27.051 | 27.216 | +0.6% |
| loop_int | 216.962 | 238.724 | +10.0% |
| native_methods | 9.706 | 10.005 | +3.1% |
| string_concat | 9.041 | 9.404 | +4.0% |
| class_instantiate | 2.991 | 3.093 | +3.4% |

本批以 null、UTF-16/Oniguruma 与宿主接口正确性为目标。loop_int +10% 未归因：
生成 TAC 与缓存读取函数反汇编一致，仅主循环布局变化；去掉强制 PIC 后回退仍存在，
Unicode 冷路径外提实验无稳定收益已撤回，保留非空对象读取内联、错误路径外联与
全局整数读取提前返回；SDK 链接进共享库时由宿主显式启用 PIC。原始样本、环境和
二进制 SHA-256 见[测量数据](../.agents/reports/2026-11-01-sdk-alignment-benchmark.json)。

## 2026-11-01 运行期诊断源码快照

Windows x64、Clang 19.1.5、Release `-O3 -DNDEBUG` 静态 CRT，基线为上一批最终
可执行文件；预热后交替 11 轮取含启动 wall-time 中位数。七项基准 −7.6%~+4.3%
（class −0.1%、fib +0.2%、loop_int −7.6%、native +4.3%、string +2.0%、字符串/
原生压力 +0.1%/+1.5%），短用例波动不外推。正文快照保留成本：2099200 字节源码
（2048 行注释）峰值工作集 +2.38 MiB；CLI 编译/执行期间同时持有原缓冲与快照，
嵌套 Chunk 共享快照不按函数复制。Catch2 验证重装载、延迟函数、GC 回收、故障
重置与关闭保留；Windows Debug/Release/Release+JIT 各 235 项 CTest 通过。原始
样本见[测量数据](../.agents/reports/2026-11-01-source-snapshot-benchmark.json)。

## 2026-11-01 结构化异常栈与 getter 异常隔离

环境同上节，基线为源码快照批次最终可执行文件。九项基准：class −0.7%、fib −2.3%、
loop_int −3.9%、native +7.0%、string −2.9%、字符串/原生/getter 压力 −2.7%/+0.8%/−1.1%；
疑似回退单独交替复测 11 轮：stack-fib-stress −3.5%（两轮方向相反，不作结论）、
**stack-class-stress +7.6%（两次均约 +8%，尚未归因，TODO 保留复核）**。getter
哨兵每 VM 惰性创建并缓存已编码空 Chunk，嵌套访问只创建独立调用帧；指令边界表
每 Chunk 一个 Vec、每条 TAC 约 4 字节，异常栈仅错误路径分配（≤64 帧）。Windows
Debug/Release/Release+JIT 各 235 项 CTest 通过；实现见 `src/vm/VMState.*`、
`Chunk.hpp`、`CallFrame.hpp`、`src/types/Property.hpp`，原始样本见
[测量数据](../.agents/reports/2026-11-01-runtime-stack-benchmark.json)。

## 2026-11-01 fib 调用路径批次（A 帧级字节码缓存 / D 立即数融合 / B 释放帧免重建）

Windows x64、Clang 19.1.5、`-O3 -DNDEBUG` 静态 CRT，基线为干净 HEAD `2fe2d59`，
单变量逐项叠加；负载期口径为预热后交替 15~21 轮取 min，A/A 噪声底：fib ±10%、
fib(30) ±2%、loop_int ±1.5%。调用微基准差分：调用机械 133.7 vs TJS2 93.6 ns/次、
纯派发 7.4 vs 3.6 ns/条，fib 差距全在调用机械而非函数体。

- A 帧级字节码缓存（`CallFrame::bcBase/bcSize/bcConstants/bcRegs`）：A1 内联分支
  （loop_int +19% 派发环布局病理）与 A2 建帧即编码（破坏惰性编码不变量）否决，
  A3 惰性填充 + 冷路径 `CIAL_COLD_NOINLINE` 外提接受。
- D 立即数融合（`BinaryImm`/`CompareTestImm`，活跃性共用 `computeBlockLiveness`）：
  fib 两批一致向好，接受；新指令 95/96 同步 X-macro/encode/dispatch/dump/SSA/JIT
  与字节码文档；JIT 拒编门槛改按等效解释步数计（融合指令计 2）。
- B 释放槽只析构不重建（`freeCallFrame`）：严格更少工作且各批无回退，接受。

整栈 A3+D+B 相对 HEAD（min-of-21）：

| 脚本 | HEAD min ms | 整栈 min ms | 变化 |
|---|---:|---:|---:|
| fib(30) 压力 | 412.11 | 384.10 | **−6.8%** |
| fib | 41.29 | 38.81 | −6.0% |
| loop_int | 348.80 | 309.07 | **−11.4%** |
| 调用微基准 | 888.19 | 811.03 | **−8.7%** |
| native_methods | 19.38 | 18.78 | −3.1% |
| string_concat | 17.23 | 16.66 | −3.3% |
| class_instantiate | 10.57 | 10.28 | −2.8% |

验证：Windows Release 与 Release+JIT CTest 各 236/236（含融合判定用例与
`tests/tjs/expression/binary/immediate-fusion.tjs`）。被否决的后续候选 F（终结检查
外提）两批一致回退后归因：每指令骨架开销（终结检查、`_pending`、记账、循环条件）
是多个分散小项，搬运单项不划算，收此块应整体改骨架；顺带核实 Release 下 96 个
dispatch handler 全部内联进 runLoop（仅 dispatchTailCallV 存活为独立符号）。
同日 `thisData` 缓存继承补丁未纳入本栈，已于 macOS 复测后落地（见下节）；原始
样本、A/B 脚本与各形态补丁留档本机忽略目录 `bench/results/20261101-fib-call-path/`。

## 2026-11-01 thisData 缓存继承补丁（macOS 复测并落地）

macOS arm64、Release `-O3 -DNDEBUG`（无 LTO），基线 `b26a547`；基线/补丁二进制
交替批次（各 3 批 × 预热 + 11 轮取 min）。fib 的逐调用开销来自绑定 this 分支的
`setThisObj(Value{GlobalObject})`：顶层 `chg_this` 把全局函数 `Function::thisObj`
永久绑为 GlobalObject，此后每次 `call_v` 都做一次必败 `dynamic_cast<DataObject*>`
（fib(35) 采样 ~20% 样本）。

补丁为 `CallFrame::inheritThisObj`（`src/vm/CallFrame.hpp`）：绑定 this 与调用者
thisObj 同对象时沿用调用者帧的 thisData 缓存（thisData 是 thisObj 的纯函数缓存，
同一对象必得同值，严格等价）；`invokeCallV`（`VMState.cpp`）与 `Function::call` 的
绑定 this、调用者 this 两分支改走继承，跨对象 incontextof 等其余形态保持
`setThisObj` 原语义。补丁后采样 `setThisObj`/`dyn_cast_*` 归零。

| 脚本 | 基线 min | 补丁 min | 变化 |
|---|---:|---:|---:|
| fib | 20.94 | 12.97 | **−38.1%** |
| fib(30) 压力 | 202.41 | 123.23 | **−39.1%** |
| fib(35) 采样变体 | 2289.43 | 1342.00 | **−41.4%** |
| probe-call-in-fn（5M 空调用） | 407.58 | 262.42 | **−35.6%** |
| loop_int / native_methods / string_concat / class_instantiate | — | — | A/A 同幅噪声带 |

调用微基准 81.5 → 52.5 ns/次。同窗口双引擎对照（参考 TJS2 `d8044c3`，交替 11 轮取
min）：fib 12.44 vs 17.80 ms = **69%**（同表补丁前口径 122.7%）、fib(30) 69%、
fib(35) 66%、probe 91%；与调用路径无关的用例比值不变。Windows 上该补丁为中性
（MSVC ABI 失败转换廉价），是否回植本机由 Windows 侧按同口径另行复核。

验证：CTest MacOS Release/Debug 各 412/412；修改文件 clang-format 20.1.0 通过；
this 绑定语义（incontextof / super / 方法调用）由既有 tests/tjs 套件覆盖。原始批次
报告、采样与二进制 SHA-256 位于本机忽略目录 `bench/results/20261101-thisdata-macos/`。

## 2026-11-10 三项遗留归因与 GC 标记队列化

[测量报告](../.agents/reports/2026-11-10-perf-attribution.json)；GC 标记工作队列见
[实现记录](../.agents/notes/implemented/performance/2026-11-04-gc-mark-worklist.md)。

**Windows 类压力回退（归因并接受）**：VS 2022 LLVM clang 19.1.5 Release、20 万次迭代
x10 负载（原 2 万次负载约 41 ms 含启动，噪声占主导）。三方对照（before=源码快照
批次终态 / stack=2fe2d59 / head）：构造与方法调用路径批次后更快（ctoronly −3.6%/
−4.3%，nogetter −3.4%/−1.6%）；回退完全来自 getter 异常隔离哨兵帧——full−nogetter
差值 ≈ +19 ms/20 万次 ≈ 95 ns/次访问（head 91 ns）。旧路径为缺陷实现（循环内直接
访问 getter 崩溃，嵌套 run 重复执行父帧语句），接受该常量开销并保留"哨兵并入
getter 帧"为后续优化候选。

**GCC 整数循环回退（归因并消除）**：g++ 15.2.0/WSL2，loop_int 交替 11 轮两轮
独立测量。复现 +7.8%/+9.8%（原 GCC 14.2 记录 +10%）；git bisect（72 提交，
7 轮构建）定位首坏提交 `b9ae0db`（该点 +19.4%）。根因：该提交给 GThis/DThis
全局快路径守卫加入 `thisObj == context.global()` 比较，`Context::global()` 定义
在 Context.cpp（pimpl）无 LTO 不可内联，顶层脚本每次全局读写一次真实调用。
修复：VMState 缓存 `_globalObject` 指针（守卫与尾递归判定共用）。修复后
loop_int 相对 04dbd2e 基线 **−2.35%/−3.95%**——回归消除且后续批次（A3+D+B）
的优化不再被掩盖。Windows/WSL 全量 CTest 249/249、449/449。

**stdlib 临时值拷贝占比（不立批次）**：临时插桩 Value 拷贝构造计数 +
invokeCallV 原生作用域标记（测后还原，属性协议路径未计入）。native_methods
压力（100 万次原生调用）：每次调用总拷贝 8 次、stdlib 内 3 次，原生调用约
150 ns/次、内拷贝约 3-6 ns，上限约 2-4% 且为纯原生微基准；loop_int 全程仅
31 次拷贝。收益不足，不立批次；`NativeFn` 的 `Value thisVal` 传值形参是
最明显的单点（约 1 拷贝/次），如未来做调用机械批次可顺带改为 const 引用。

## 过程问题记录

- thisData 补丁初版按 Windows 轮记录的猜测改在调用者 this 继承分支，复测无收益，采样定位后确认必败 `dynamic_cast` 在绑定 this 分支——热路径源点必须实测，不能沿用上一平台的画像推断。
- 本机测量期间前台进程稳定占核，同二进制 A/A 批间漂移可达 ~4% 且波及无函数调用的 loop_int，先后批次不可直接对比，须基线/补丁二进制交替批次对消时间窗。
- Bench.cmake 的 `BENCH_DIR` 传相对路径时 glob 不到基准脚本，须用绝对路径。
