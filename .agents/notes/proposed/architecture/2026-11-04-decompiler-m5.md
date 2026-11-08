# 反编译器 M5 与移交遗留

Status: proposed

## 当前状态（2026-11-08，接续先读此节）

M5 仍在进行；当前能力与剩余工作以
[方案当前状态表](../../../../doc/bytecode/decompiler-plan.md#m5-当前状态2026-11-05)为准。
异常区域已完成状态机首版（2026-11-07 验收）、结构化 try/catch 首版、catch
数据流存储提升与无名循环携带提升（2026-11-08 三批），实现与边界见
[实现记录](../../implemented/architecture/2026-11-07-decompiler-exception-regions.md)：
**异常语料 9/9 全部结构化 try/catch**（无名寄存器与无名循环携带变量提升为合
成存储），具名局部被 catch 读取（声明落点未证明）保持诚实回退，9 组语料
darwin 参考对拍一致。switch 主线的调用标签表达式（`case f():`，自产与参考
模板）已验收，新增 4 组 calllabel 语料。状态机调用族白名单扩展（含接收者内
联 `o.m(...)`）与 try 写入侧 join 合流提升已验收，新增 `m5_exception_call_in_try`
语料，异常语料 10/10 结构化。
下面各批次保留当时的拒绝与验证事实，后续修复不覆盖历史失败记录。
特别是早期“嵌套 switch 一律拒绝”和“赋值右值 FuncLit 待实现”不再是现状：
自产双层/三层与参考函数内双层 switch 已验收；赋值右值 FuncLit 的源码
恢复已复核，参考字节码路径仍待验收。参考嵌套返回与三层合流的固定样本现已通过；对象成员 FuncLit 的 NEW/this 差异仍未闭环。

## 问题

反编译器进入 M5（硬核兜底）前的未闭环项：M5 本体的 switch 链、try/catch 嵌套兜底、
with 与 eval 名字依赖；M4→M5 移交的 bc 路径验收扩展（super 构造 prologue、
`CallMethodV`/`testDeref` 形态）；表达式位置匿名函数字面量；以及一批顶层物化
相关形态的最终裁定。

## 已知事实与证据

- 设计与批次见[反编译器方案 §6](../../../../doc/bytecode/decompiler-plan.md#6-实施批次)；
  M0–M4 已完成并逐批记录在该表（M4 收尾 2026-11-04，Linux CTest 431 全绿）。
- **顶层物化项已闭环（2026-11-04 本机验证）**：`f() + g() + f()`、`o.a + o.b + o.m()`
  （存储与裸语句形态）、多次成员读混合链在 HEAD 全部恢复成功——M4 的落点传播与
  "同树内联"豁免已覆盖该形态；顶层非纯多使用维持拒绝（合成全局名有方案 §3
  动态名查找风险），不再需要新物化策略。
  注意：早先"pc 12 跨脏区间"失败来自 11-03 旧二进制（M3 时代产物）——当时
  `src/format/Files.cpp` 缺 `<io.h>`/`<fcntl.h>`（clang-cl 无法编译）使增量构建
  静默失败、探测跑在旧 exe 上。已补显式 include（本机 clang-cl 构建恢复）。
  证据核对产物身份的教训与 [cial-verify](../../skills/cial-verify/SKILL.md) 一致。
- **类体逗号声明链修复（2026-11-04）**：`class C { var a, b; }` 此前只注册首名
  （`VarDeclNode::next` 链未走，`Parser.cpp` 类体循环只收头节点；IRGen 每条目
  单名），实例化不预注册后续成员、反编译如实输出 `var a;`。修复后逐名注册
  （`varDeclVec` 改存 const 指针 + 遍历链），含初始化器链（`var a = 1, b = 2;`
  逐名 DThis）端到端验证。回归：`tests/tjs/oop/field-comma-declaration.tjs`、
  `cli_test.py` 逗号字段文本断言与 3 个混合链 round-trip 用例；
  Windows Debug CTest 241/242 通过。
- **s7_ext 行表失败已修复（2026-11-04，根因在参考导出器）**：
  `test_decompile_outputdebug_source_lines` 失败的根因是参考编译器
  `ExportByteCode` 把 count 写成**字节数**（`SourcePosArraySize*8`）却按条目
  循环写 `SourcePosArray[i]`——越界自读 8 倍条目，导出进程的堆垃圾被冻结进
  fixture；读取端把字节数当条目数全收，垃圾条目（`src@0`、`src@-2147483648`）
  污染行表继承，且与各平台标准库的不稳定排序平局差异叠加，产出随平台漂移
  （Linux"全绿"是垃圾+平局运气，期望值整体被污染）。修复：读取端按
  count/8 截断真实条目并跳过垃圾尾（`TjsBytecodeReader.cpp`），origin 排序
  改稳定序（`TjsTranslator.cpp`），manifest 期望按真实条目重新生成（全部
  `0`/`2` 类垃圾行消失，`describe` 收敛到真实的行 4）。附带修复：throw 语句
  位置回归（报错列从语句起点漂到表达式起点，取回 throw 关键字位置）。
  Windows Debug **CTest 243/243 全绿**（首次）。
- 表达式位置匿名函数字面量仍拒绝：`o.m = function(){...}`（赋值右值，
  非 M4 已支持的实参位置）报"表达式位置的匿名函数字面量恢复属后续批次"
  （`out/m5-probe/probe.tjs` 形态）。M4 的 FuncLit 恢复只覆盖实参位置。

## 本轮推进（2026-11-04，dirty 工作区）

接续工作区已有的 `ExprBuilder.cpp` switch 草稿，移除无条件调试输出并补齐：

- 自产 `CompareTest/CompareTestImm + JmpE` 前向派发链识别、case 体封界、
  出口与跳转边界检查；多值 case、贯穿、break、尾部 default 与无匹配恢复。
- 判别值按 `resolveCopyNode` 核对，允许优化器将首次 GLocal 拷贝和后续源寄存器
  混用；不同源节点拒绝。派发比较登记 `_condPcs`，避免含副作用的比较降为
  普通语句、卡住发射游标而漏掉体内和后续输出（新增调用计数回归）。
- `tests/cli_test.py` 增加 6 个源码 round-trip 样本与循环 continue 严格拒绝回归；
  原 switch 拒绝占位测试改为 try/catch 拒绝。
- 构建身份：Windows Debug，CLI/TESTS/FORMATTER ON，JIT OFF，clang++，
  x64-windows-static，vcpkg 根 `D:/cpp_tool/vcpkg`。
- 首次全量 CTest 为 242/243：唯一失败是 Catch2 中旧 switch 必须拒绝的断言，
  已同步改为 try/catch；日志 `out/m5-ctest.log`。
  最终构建退出码 0；`ctest --test-dir build/Windows/Debug -R
  "^(CiallangSuite|CiallangCli)$" --output-on-failure --no-tests=error` 为 **2/2 通过**
  （`out/m5-final-ctest.log`），其余 241 项沿用首轮通过证据。
  修改的 C++ 已用 clang-format **20.1.0** 格式化并 dry-run 校验，`git diff --check` 通过。
- 本轮仅验证自产路径，未执行参考 TJS2 四环，不能据此宣称 switch 的 bc 路径完成。
  循环 continue 组合探测明确拒绝（体尾跳转目标不一致），不视为已支持。

## 后续派发与外围循环（2026-11-04，dirty 工作区）

- `scanDispatch` 接纳派发比较之间与后续派发头前的 `Load/LoadImm`，因此字符串多值
  标签、未折成立即数比较的后续整数标签均能恢复；其它有副作用前缀仍拒绝。
  初始前缀限定在所在基本块，避免扫描从循环前跨入 do-while 体首。
- switch 出口推断排除最内层外围循环的 continue/锁存目标，case 跳转边界检查
  允许这两个已证明的目标；发射时继承外围循环 continue 上下文，保留 switch 自己的 break。
- 修正双块 do-while 条件入口：条件块起点含求值前缀，应为 `cond.start`；
  continue 指向 `condStart`，而不是回边指向的体首。while 与 do-while 中的
  switch continue 均 round-trip 通过。for 更新段组合仍拒绝，新增独立拒绝回归。
- CLI switch round-trip 现有 9 个样本（本批新增字符串链、while continue、do continue）。
  构建命令 `cmd /c out\m5-build.cmd` 退出 0；`ctest --test-dir build/Windows/Debug -N`
  确认 243 项；`ctest --test-dir build/Windows/Debug --output-on-failure --no-tests=error -j 10`
  **243/243 通过**（22.73s，`out/m5-next/ctest.log`）。配置沿用上一批。
  clang-format 20.1.0 dry-run 与 `git diff --check` 通过。

参考侧探测与对拍（`out/m5-next/reference-check.py`、`reference-report.json`）：

- 参考仓库 HEAD `40a736c48f40dff7c737e9c5dc9c0e46f145e9e5`，执行文件
  `../TJSCompiler/bin/Windows/TJSCompiler.exe` SHA-256
  `8f370f2bb7460c8b7b8090a8bc71b7d0fb94a25e98ee2ff20d01bf3df358c60b`。
  `Debug.message("M5_REF_OK")` 实际执行成功，`--dump <script> <out>` 实际导出成功。
- `strings`、`continue`、`do` 三样本自产 round-trip stdout 逐字节一致；
  参考原源码 vs **Ciallang 自产恢复源码**的输出记录序列 3/3 一致，跨引擎结果 3/3 一致。
  参考输出包装器定义 println 调用 Debug.message，以 `M5_RESULT:` 标记提取有效负载，
  剔除时间戳和参考程序自动 context dump；原始 stdout/stderr 与输入/产物哈希均保存在 JSON。
- 三份参考字节码在 Ciallang 转译执行的 stdout 与自产源码执行逐字节一致；
  三份字节码反编译均退出 2，首个原因 `_zf 直写比较族属 M5`，严格模式无文本。
  此次没有实现或宣称 bc switch 恢复。
- 尝试参考 `Scripts.execStorage` 读取这三份字节码均报
  `syntax error, unexpected end of file, expecting ;` 后退出 **3221225477 (0xC0000005)**。
  本地 `src/syntax/library/ScriptsInterface.cpp` 读取文本并调用 `ExecScript`，不提供二进制
  加载入口；因此字节码真实性这一环尚未验证，不能把源码结果一致当作四环全部通过。

## `_zf` 条件恢复（2026-11-04，dirty 工作区）

- 参考 switch 的真实模板是 `CmpEQ + JmpNE`，不像自产的 `CompareTest + JmpE`。
  本批先完成相邻 `CmpEQ/CmpDEQ/CmpLT/CmpGT + JmpNE` 的条件节点恢复：
  两个寄存器计入使用链，flag 本身不伪造寄存器定义；严格相等映射 `AbsEQ`。
  非相邻 flag 消费或 `JmpE` 消费仍在预检拒绝，不推测 flag 生命周期。
- Catch2 新增四比较族的 TAC 变体 round-trip 与错误消费方向拒绝测试，
  同时加入固定参考 fixture `tests/test_files/tjsbc/decompile/m5_flag_conditions.{tjs,tjsbc}`；
  同目录 manifest 保存源码/字节码/参考二进制 SHA-256 和 `--dump` 导出参数。
  CLI 比对原源码、参考字节码转译执行、字节码恢复源码三路 stdout；
  含严格相等混合数字/字符串输入，两侧分支均有覆盖。
- 参考二进制身份沿用上一批（SHA-256 `8f370f2b…358c60b`）；
  参考原源码 vs **参考字节码的恢复源码** 16 条 `M5_RESULT:` 记录一致、两进程退出 0。
  原始输出、完整哈希与脚本在 `out/m5-zf/reference-report.json` / `reference-check.py`。
  这次已实际覆盖字节码恢复来源；参考原字节码直接执行入口仍缺，未宣称四环全部通过。
- 构建测试曾因新增测试漏写 `inter::Register/Label` 命名空间而失败；其后旧测试二进制
  的绿色结果不计验收。修正限定名后重新构建成功（退出 0），最终
  `ctest --test-dir build/Windows/Debug --output-on-failure --no-tests=error -j 10`
  **243/243 通过**，21.92s，日志 `out/m5-zf/ctest.log`。配置与上一批一致。
  clang-format 20.1.0 dry-run 与 `git diff --check` 通过。
- 原 switch 样本拒绝点已越过比较预检，现为共享判别值顶层多使用、贯穿跳转等：
  `out/m5-zf/switch.tjsbc` 报 `pc 5 的临时值多使用且顶层复制会改变可观察行为`；
  原 `strings.tjsbc` 报 `pc 17 的跳转目标 35 不属于可结构化形态`。
  不能通过允许复制 getter/调用来绕开；下一批应识别参考派发链并输出 switch。

## 参考 switch 首批（2026-11-04，dirty 工作区）

- 新增 `precomputeReferenceSwitches`：识别至少两个单值标签的
  `Load/LoadImm + CmpEQ + JmpNE` 前向派发链；后续派发前缀仅含常量装入，
  判别寄存器一致且发射时复核版本源节点。恢复为 switch，因此调用/getter 判别值
  不因多个比较而重复求值。支持 break、尾部 default、字符串、返回与无 default；
  显式全局/属性存储的贯穿保持执行顺序。
- 参考编译器保留 break/return 后不可达的回跳。基于 CFG 从入口计算可达性，
  仅折叠不可达 Jmp，并清除由这些回跳误识别的 while/do 循环；普通可达跳转不删除。
  派发头不参与三元折叠，避免后处理改写其条件节点。
- 探测曾出现成功状态下语义错误：`var r=0; switch(x){case 1:r+=1;case 2:r+=10;break;...}`
  的无名寄存器版本被线性拼接，恢复源码结果不符。已加入 case/default/出口区域
  的 use-before-def 与写集合交叉检查，存在跨区域寄存器合流即严格拒绝。
  该形态保存为 `m5_switch_register_merge`，不能把拒绝称为已修复局部变量恢复。
  本批亦明确拒绝 case 内嵌套条件与复杂 default 控制流，等待进一步区域数据流支持。
- 固定语料位于 `tests/test_files/tjsbc/decompile/`，manifest 新增五组源/字节码哈希
  与预期状态。四个成功样本：calls（调用次数）、strings（两标签+default）、
  returns（无 default）、fallthrough（显式全局存储贯穿）；一个 register_merge 拒绝样本。
  CLI 经 CTest 实际运行全部五组。
- `out/m5-reference-switch/reference-report.json` 记录：4/4 原源码、参考字节码转译执行、
  字节码恢复源码执行 stdout 逐字节一致；4/4 参考原源码与**参考字节码恢复源码**的
  `M5_RESULT:` 负载序列一致（参考进程均退出 0）；合流样本退出 2 且无恢复文本。
  参考 SHA-256/导出参数沿用 fixture manifest（`8f370f2b…358c60b`、`--dump`）。
  参考字节码直接执行这一环仍受上一批所记参考入口限制，未宣称完整四环通过。
- 最终构建 `cmd /c out\m5-build.cmd` 退出 0；`ctest --test-dir build/Windows/Debug -N`
  确认 243 项；全量 `ctest --test-dir build/Windows/Debug --output-on-failure --no-tests=error -j 10`
  **243/243 通过**，24.81s，日志 `out/m5-reference-switch/ctest.log`。构建配置未变。
  clang-format 20.1.0 dry-run 与 `git diff --check` 通过。

## 参考多值标签与局部合流（2026-11-04，dirty 工作区）

- 纠正前批待办中的模板判断：本机参考编译器的多值标签仍使用
  `CmpEQ + JmpNE`，匹配后以单条 `Jmp` 中转到共享体，并非必须使用 `CmpEQ + JmpE`。
  首批算法已将中转视为空 case 的贯穿，新增 `m5_switch_multilabel` 固定语料，
  覆盖两个多值标签组及五种输入；其它 flag 真跳仍维持预检拒绝。
- 跨 case/出口寄存器的写集合与 live-in 检查现驱动函数局部存储恢复：
  仅函数内、且具有入口块或当前派发头块的共同初始化点才创建合成局部名。
  名字避开单元的参数、原有局部与所有 Atom；从初始化指令执行后的 pc 起生效，
  不影响此前用于参数搬运的同号物理寄存器。后续 switch 复用已建立的存储。
  `m5_switch_register_merge` 从拒绝转为成功；新增 snapshot、name_collision、double
  分别覆盖分支前值快照、全局 v0 名冲突、同一函数连续两个 switch。
  顶层 `m5_switch_top_merge` 保持退出 2、无恢复文本，避免引入可观察的合成全局名。
- 首轮局部恢复探测曾错误地让合成名字从函数入口生效，污染参数备份并漏掉局部 var
  声明；已按上述 pc 生命周期修正。未修正时 local/collision/double 的差异不计通过。
- 同时发现并修复 `TjsTranslator.cpp` 返回值问题：旧 `srvTjsReg >= 0` 把负编号的
  实参/局部寄存器误作“没有 SRV”，导致 `return r` 被转译成 void；改为 `Opt<i32>`
  显式记录有无 SRV。独立 `m5_return_local` 覆盖 6、5、-1 三个结果，CLI 同时检查
  源码执行、参考字节码转译执行与恢复源码执行。转译事实同步到 mapping 文档。
- 固定 manifest 现含 12 组（条件 1、switch 成功 9、顶层合流拒绝 1、局部返回 1），
  新增六组文件均保存原源码与字节码 SHA-256，参考身份和 `--dump` 参数不变。
  `out/m5-local-merge/reference-report.json`：9/9 switch + 1/1 局部返回的三路 stdout
  逐字节一致；同十组参考原源码与**参考字节码恢复源码**的标记输出序列全部一致，
  参考进程退出 0；顶层合流按预期拒绝。参考原字节码直接执行环仍未补齐。
- 构建 `cmd /c out\m5-build.cmd` 退出 0；`ctest -N` 确认 243 项；全量
  `ctest --test-dir build/Windows/Debug --output-on-failure --no-tests=error -j 10`
  **243/243 通过**，31.06s，日志 `out/m5-local-merge/ctest.log`。配置沿用上一批。
  clang-format 20.1.0 dry-run 与 `git diff --check` 通过。

## case/default 体内嵌套条件分支（2026-11-05，darwin 本机）

- `precomputeReferenceSwitches` 的 case/default 体校验重写为共享 `validateBody`：
  活 `JmpNE` 经 `scanBranch` 形态校验（封闭于体区域、合并点 ≤ bodyEnd）后按嵌套
  if 恢复；发射端 `emitSwitch → emitRegion → emitIf` 本就通用，无需改动。参考
  编译器跳转线程化使多数 if-else 的 then 尾直接 `Jmp→出口`，恢复为
  if + 直线 else 体（语义等价）；else 含 break 的真 if-else 形态保持显式 if-else。
- 嵌套子区域（then/else/合并后）纳入既有 writes/liveIn 合流检查与函数局部存储
  物化；default 体活 `JmpNE` 同规则（旧 "default 控制流尚未恢复" 仅对 Jmp 目标
  越界保留）。不可达跳转一律折叠（旧实现对不可达 `JmpNE` 误硬拒，已顺带修正）。
- **嵌套 switch（case 内同判别值派发链）保守拒绝**：两轮探测发现函数级会把标签
  常量寄存器跨 case 合流物化成合成局部名，`case 1/2` 被渲染成 `case v1`（语义
  错误恢复）；顶层则因合流寄存器无头块/入口块初始化点被既有守卫拒绝。据此
  validateBody 拒绝 then/else 子区域含更深条件跳转的复合分支（`嵌套复合分支
  尚未恢复（M5）`）；头块在已认领外层 case/default 体区域且出口越界时不认领，
  退回嵌套 if 路径。同判别值 else-if 链在 break 线程化后经嵌套 if 恢复（语义
  等价，非 switch 形态）。
- 新增 5 组 darwin 参考语料（`m5_switch_nested_{if,default,elsebreak,elseif,
  switch}`，末组 expectedRecovery=unsupported + rejectReason）；
  `referenceDarwin` 记录本机参考身份：commit `d8044c3`（本地构建，main.cpp 有
  未提交改动）、SHA-256 `478eab17…b4471b1`、`--dump` 导出。执行/导出实测可用；
  与既有 Windows 语料（`8f370f2b…`，commit `40a736c`）分列。
- 自产路径新增 3 个 CLI roundtrip 样本（default 内嵌套 if、case 内顺序 if、
  else 含 break）；`test_decompile_reference_switches` 支持按样本 `rejectReason`
  断言；新增 `test_decompile_reference_switch_nested_branches`（4 组三路 stdout
  一致 + `switch (`/`if (` 结构断言）。
- 本机验证：6 组探测形态自产/参考三路 stdout 逐字节一致；嵌套 switch 两种层级
  均诚实拒绝。macOS Debug 全量 CTest **434/434 通过**（本轮新增用例后）；
  clang-format 20.1.0 dry-run 与 `git diff --check` 通过。
- **新发现（未修复，均属既有路径）**：
  1. 单 case + default 的参考开关不满足 `cases.size() >= 2` 走通用恢复，default
     体含嵌套 if 时通用路径误恢复出 `while (true)`（静默错误输出，非拒绝；
     `out/m5-nested/probe_defif.tjs`）。通用路径对参考派发尾形态的结构识别待补。
  2. 顶层参考 switch 嵌套在 while 体内被临时值多使用守卫拒绝（与嵌套 if 无关，
     无嵌套分支同拒；`out/m5-nested/probe_loop*.tjs`）。Windows 侧既往验收的
     循环组合均为自产路径，参考路径循环组合未验收。

## 单 case + default 误恢复修复（2026-11-05，Windows）

- 基线 `607638e`、起始工作区干净。新增固定样本 `m5_switch_single_default`
  在原实现成功输出 `while (true)`，属于已复现的静默错误。
- 移除“至少两标签”的前置限制；单标签仅在 case 跳转证明存在尾部 default
  区域时进入 switch 恢复，继续执行已有区域封闭、嵌套条件和寄存器合流校验。
  单个等值 if-else 可输出语义等价的 switch；无尾部区域仍走通用恢复。
  首次尝试依赖不可达回跳的守卫未命中语料，针对性测试失败，已替换。
- 固定语料和 manifest 保存源/字节码 SHA-256；参考身份沿用 Windows
  `40a736c` / `8f370f2b…358c60b`，实测 `--dump <source> <bytecode>` 成功。
  CLI 嵌套分支回归纳入该样本；输入覆盖 case 命中与 default 条件两侧。
- `out/m5-single/reference-report.json`：源码、参考字节码转译执行、恢复源码
  执行三路输出一致（1、1、2、3）；参考原源码与恢复源码标记序列一致，
  两进程退出 0。参考原字节码直接执行入口仍缺，未宣称四环全部通过。
- 最终 `cmd /c out\m5-build.cmd` 构建退出 0（`build4.log`）；Windows Debug，
  CLI/TESTS/FORMATTER ON、JIT OFF、clang++、x64-windows-static，vcpkg
  `D:/cpp_tool/vcpkg`。中途并发构建曾报 ninja 日志锁失败，待前次构建完成后重跑成功。
- `ctest -N` 确认 243 项；全量 `ctest --test-dir build/Windows/Debug
  --output-on-failure --no-tests=error -j 10` **243/243 通过**（27.38s），
  日志 `out/m5-single/ctest.log`；clang-format 20.1.0 dry-run、diff 检查通过。
- 上节“新发现”第 1 项已解决；第 2 项参考 switch 外围循环拒绝及嵌套 switch
  保持待办，M5 整体仍未完成。

## 参考 while/switch 的条件常量寄存器复用（2026-11-05，Windows）

- 接续 `607638e` 上一批 dirty 工作区，保留单 case 修复。原 darwin
  `out/m5-nested/probe_loop*` 本机不存在，重新生成顶层 while/switch 样本。
  实际首个拒绝为“循环条件读取的寄存器在体内重定义且绑定字面量”，
  与旧记录的多使用拒绝不同；不据此宣布旧样本同因或全部循环组合完成。
- TAC 显示 while 条件块每轮加载 `%4 = 4`，switch 标签/实参复用 `%4`。
  `guardLoopCond` 现在区分条件块内定义与循环外初值：前者每轮重新执行，
  无需循环携带守卫；后者仍执行原检查。既有顶层 for 无名携带变量拒绝测试保留通过。
- 新固定样本 `m5_switch_while` / `m5_switch_while_calls` 保存源码、字节码与
  manifest 哈希，由现有 CLI manifest 回归自动执行。覆盖两个 case、default、
  多轮迭代及调用判别值每轮恰好一次（总计 4）。
- 参考 exe SHA-256 `8f370f2b…358c60b`、仓库 `40a736c`，实际 `--dump`
  导出成功；两样本三路执行 stdout 一致，参考原源码与字节码恢复源码的
  标记序列一致，两侧退出 0。报告 `out/m5-loop/reference-report.json`；
  参考原字节码直接执行环仍未覆盖。
- Windows Debug 构建沿用 CLI/TESTS/FORMATTER ON、JIT OFF、clang++、
  x64-windows-static、vcpkg `D:/cpp_tool/vcpkg`；`cmd /c out\m5-build.cmd`
  退出 0（`out/m5-loop/build.log`）。`ctest -N` 确认 243 项；全量
  `ctest --test-dir build/Windows/Debug --output-on-failure --no-tests=error -j 10`
  **243/243 通过**（23.54s，`out/m5-loop/ctest.log`）。clang-format 20.1.0
  dry-run 与 git diff 检查通过。
- 待办仍包括参考循环 continue/do/for 组合、旧多使用拒绝样本重新复现、
  嵌套 switch 与有副作用派发前缀。

## switch 与循环组合验收（2026-11-05，Windows）

- 接续前两批 dirty 工作区，基线仍为 `607638e`。本批恢复参考 while/do/for
  内 switch 的 break/continue、default continue、嵌套外围循环，以及 switch
  case/default 内的 while/do/for（含循环内条件 continue）。不代表任意 CFG 或
  嵌套 switch 已完成。
- 循环预计算先排除 CFG 不可达块；同一头选择最晚的正常回边，避免较早的
  continue 抢占锁存点；剔除外围 while 内 if/continue 误认出的 do 尾模板。
- 接纳相邻比较 + 后向 JmpE 的 do 条件真回跳；条件区包括所需名字/常量
  装入前缀，尾部已有调用语句仍留在循环体内，避免 break 后漏输出。
- for 更新块由前向入锁存块的边确定。恢复成 while 时，continue 分支显式
  发射更新段再 continue，普通路径仍执行原更新段；保存/恢复语句游标，避免
  提前发射吞掉后续正文。带副作用 step(i) 的样本确认更新恰好执行四次。
- switch 出口推断区分外围条件/锁存与更新段，取已证明的较早公共出口；
  case/default 内已识别循环使用自身上下文。参考跳转线程化使循环条件直达
  switch 出口时，循环后显式补 break。单标签恢复额外要求 case 有参考导出
  保留的不可达跳转，避免把循环内普通 if/continue 误认成 switch。
- 过程中执行回放捕获并修复：更新段发射漏语句、公共尾部被误排除导致丢
  default/假无限循环、do 条件块误吞尾部 println、if/continue 被误识别为 do。
  早期成功反编译但超时/输出错误的探测不计验收；日志与探测位于
  `out/m5-combinations/`，最终采用 `build8.log` 与 `reference-report.json`。
- 固定语料 `m5_switch_combo_*` 共 12 组：while_continue、do_continue、
  do_break、for_continue、for_break、for_effect_update、default_continue、
  nested_loops、loop_in_case、loop_in_default、do_in_case、for_in_default。
  manifest 保存全部源/字节码 SHA-256；参考 Windows 二进制和导出身份沿用
  `40a736c` / `8f370f2b…358c60b`，实际 --dump 成功。
- CLI 新增组合测试，24 条自产/参考字节码恢复路径执行结果均与原源码一致；
  manifest 测试另核对参考字节码转译执行。12/12 参考原源码 vs 字节码恢复
  源码的标记序列一致，各参考进程退出 0。参考原字节码直接加载环仍缺入口。
- 具名循环变量或显式全局存储覆盖本批组合；自产顶层 `for(var i=...)` 的
  无名寄存器携带初值仍严格拒绝，旧测试改为断言真实的“循环携带变量”原因。
  本批没有以合成全局名绕开该限制。

- 最终 Windows Debug 配置仍为 CLI/TESTS/FORMATTER ON、JIT OFF、clang++、
  x64-windows-static、vcpkg `D:/cpp_tool/vcpkg`。`cmd /c out\m5-build.cmd`
  退出 0；CTest 注册数 243，全量 `ctest --test-dir build/Windows/Debug
  --output-on-failure --no-tests=error -j 10` **243/243 通过**（24.48s），
  日志 `out/m5-combinations/ctest-final.log`；clang-format 20.1.0 dry-run 与
  git diff 检查通过。此结论包含前两批未提交改动。

## 深层条件分支与嵌套 switch 复核（2026-11-05，Windows）

- 延续前批 dirty 工作区。本轮先探测嵌套 switch：去掉深层条件守卫并把
  case 标签从原始常量装入渲染后，既有 `m5_switch_nested_switch` 虽输出了
  正确的 case 1/2，却漏掉外层第二 case 的赋值和函数 return，四次调用均
  返回 void。该实验已全部撤回，不将成功退出当作支持；原 fixture 保持拒绝。
  原始实验输出 `out/m5-nested-next/restored.tjs`；后续须同时处理合成寄存器
  存储与嵌套派发段跳过后语句游标滞留，单修标签不足以闭环。
- 可独立推进的深层 if 已落地：旧守卫拒绝 then/else 内任意更深条件跳转，
  现仅对疑似常量等值派发链保守拒绝；普通多层 if 继续逐个执行 scanBranch
  区域封闭校验，并收集各层 then/else/合并后区域的写集合/live-in，沿用
  函数局部合流存储恢复。常量等值 if 可能保守落入派发形态，不宣称全量接纳。
- 新固定 Windows 语料 `m5_switch_deep_if` / `m5_switch_deep_calls` 保存
  源/字节码 SHA-256，覆盖 case 与 default 内两层 if 的全部分支；后者将
  条件改为 sign 调用，确认只沿实际路径调用，总计 10 次。参考导出身份
  仍为 `40a736c` / SHA-256 `8f370f2b…358c60b`，实际 --dump 成功。
- CLI 嵌套分支测试与 manifest 测试纳入两样本；原源码、字节码转译执行、
  字节码恢复执行三路结果一致，两样本自产 round-trip 亦一致。参考原源码
  与字节码恢复源码标记序列 2/2 一致，原进程退出 0；报告
  `out/m5-nested-next/reference-report.json`。参考原字节码加载环仍缺入口。
- 最终 `cmd /c out\m5-build.cmd` 退出 0（build3.log）；Windows Debug
  配置延续 CLI/TESTS/FORMATTER ON、JIT OFF、clang++、x64-windows-static、
  vcpkg `D:/cpp_tool/vcpkg`。原嵌套 switch 拒绝回归保留，未弱化期望。

- CTest 注册数 243；全量 `ctest --test-dir build/Windows/Debug
  --output-on-failure --no-tests=error -j 10` **243/243 通过**（24.97s），
  日志 `out/m5-nested-next/ctest.log`。clang-format 20.1.0 dry-run 与
  git diff 检查通过，包含先前所有未提交修改。

## 参考函数内嵌套 switch 首批闭环（2026-11-05，Windows）

- 基线 `607638e` + 前批 dirty 工作区。前批确认的“标签显示错误 + 后续语句
  丢失”根因是同一物理寄存器被合流恢复为合成存储后，仅供派发比较使用的
  标签 Load 也被 noteNamedDef 改绑为合成名字并生成 StoreLocal；后续派发段
  不逐条发射这些赋值，语句游标滞留在旧 pc，外层赋值/return 不再消费。
- 现在登记参考标签装入 pc；仅在该定义版本的全部使用都为已认领派发比较时，
  保留其常量节点、不生成合成存储赋值。标签版本存在其它用途且需局部物化时
  明确拒绝。移除深层条件的派发形态硬拒后，仍逐个执行区域/合流校验。
  没有通过任意推进语句游标跳过未知语句来掩盖遗漏。
- 第一轮 Ciallang 回放 4/4 一致，但参考恢复源码在兄弟 case 使用 v1 时崩溃，
  退出 `3221226505`，原源码退出 0；参考反汇编显示 v1 被编译为全局属性访问。
  根因为合成局部第一次在 case 的词法域声明，对兄弟 case 不可见。现将合成
  存储声明统一放在函数体入口，所有初始化/赋值保持原 pc 和执行路径。
  原失败报告已由最终报告替代，关键证据和错误码在此保留。
- 既有 darwin `m5_switch_nested_switch` 从拒绝改为成功，源/字节码哈希不变，
  历史源码首行的“本批拒绝”注释属于导出时背景。新增 Windows 语料
  `m5_switch_nested_same` / `m5_switch_nested_calls` /
  `m5_switch_nested_default_inner`，分别覆盖同判别值、调用判别值及 default
  内嵌套。每个样本都包含外层其它 case 与 return，防止漏语句回归。
  Windows 导出身份仍为 `40a736c` / `8f370f2b…358c60b`，实际 --dump 成功。
- CLI 增加函数入口声明与至少两个 switch 的结构断言；manifest 回放核对
  三路执行。4/4 新增/升级样本参考对拍通过。声明提升影响既有合流恢复，故
  扩展复核全部成功 switch 语料：**34/34** 三路输出及参考原源码/恢复源码
  标记序列一致，参考进程退出 0；顶层合流仍拒绝。报告
  `out/m5-nested-fix/all-switches/reference-report.json`。全部 37 组 manifest
  的源/字节码哈希核对通过，参考原字节码直接加载环仍未覆盖。
- 最终构建 `cmd /c out\m5-build.cmd` 退出 0（build3.log）；Windows Debug
  CLI/TESTS/FORMATTER ON、JIT OFF、clang++、x64-windows-static，vcpkg
  `D:/cpp_tool/vcpkg`。CLI **49 项通过**；CTest 注册数 243，全量
  `ctest --test-dir build/Windows/Debug --output-on-failure --no-tests=error -j 10`
  **243/243 通过**（24.12s，`out/m5-nested-fix/ctest-final.log`）。
  clang-format 20.1.0 dry-run、git diff 检查通过。
- 限制：自产嵌套 switch 仍在派发链预检拒绝；顶层需合成全局名的嵌套 switch
  仍拒绝；参考函数内更广泛的贯穿、返回线程化与三层嵌套还需扩展语料。

## 自产嵌套 switch、贯穿与返回扩展（2026-11-05，Windows）

- 续接 `607638e` + 前批 dirty 工作区。自产派发改为由内向外归约，先认领
  子 switch，再校验外层；既有 JmpE 硬拒仅针对未被任何结构认领的跳转。
  区域封闭和出口检查保留，未对未知跳转任意放行。
- 直接反向遍历块曾误将后续 case 派发头/多值标签的内部比较块各自恢复成
  switch，CLI 首轮 11 项失败，补后续链头识别后仍有字符串多值标签 1 项失败。
  现预扫描完整派发跨度与仅含常量前缀的后续链，标记其内部块为续接点，
  不作为独立 switch 根；原多值字符串、循环组合回归重新通过。
- 自产函数内 7 组样本通过回放与参考 TJS2 源码对拍：既有 nested_switch、
  nested_same、nested_calls、nested_default_inner，以及新 nested_fallthrough、
  nested_returns、nested_three。后 3 组保存 Windows 原源码/字节码哈希，
  参考导出身份仍为 `40a736c` / `8f370f2b…358c60b`，实际 --dump 成功。
  贯穿覆盖内/外两层，返回覆盖每个 case/default，三层覆盖所有层的不同入口。
- CLI 新增自产嵌套测试，断言至少两层/三层 switch 及输出逐字节一致。
  7/7 原源码与自产恢复源码在参考引擎的标记序列一致，进程退出 0；报告
  `out/m5-source-nested/source-reference-report.json`。参考字节码贯穿样本
  也成功，全部成功 switch 字节码语料 **35/35** 三路 stdout 与参考标记序列
  一致（`reference-report.json`）。40 组 manifest 源/字节码哈希全部核对。
- 不同路径的能力分别登记：`nested_returns.tjsbc` 仍报 pc 39 跳转目标 41
  不可结构化；`nested_three.tjsbc` 仍报不可局部物化的寄存器合流。manifest
  明确将这两组字节码登记为 unsupported，并断言真实拒绝原因；源码恢复成功
  不能替代字节码能力验收。顶层合流仍拒绝，参考原字节码直接加载环仍缺入口。
- 最终 `cmd /c out\m5-build.cmd` 退出 0（build3.log）；配置沿用 Windows
  Debug、clang++、CLI/TESTS/FORMATTER ON、JIT OFF、x64-windows-static、vcpkg
  `D:/cpp_tool/vcpkg`。CLI **50 项通过**，CTest 注册 243 项，全量
  `ctest --test-dir build/Windows/Debug --output-on-failure --no-tests=error -j 10`
  **243/243 通过**（23.89s，`out/m5-source-nested/ctest.log`）。
  clang-format 20.1.0 dry-run 与 git diff 检查通过。

## 状态核对与测试脚本整理（2026-11-05，Windows）

- 当前状态集中到方案 §6 的能力/剩余工作表，note 顶部增加接续入口；历史失败
  和旧命令保留原路径，不将早期拒绝描述当作现状。M5 整体仍为 proposed。
- 复核 `o.m = function(x){...}` 的自产恢复已成功，新增固定输出和调用计数
  回归（3、5、2），从“待实现”改为“自产已验收、参考字节码路径待验收”。
- 原 tests/cli_test.py 的 50 个测试方法按 AST 清单逐个核对并迁移：
  tests/cli/test_cli.py（32）、test_decompile.py（原 11 + 新增 FuncLit 1）、
  test_switch.py（7）。support.py 共用参数、独立临时目录、进程运行与语料路径，
  导入时不解析参数。未减少原用例或放宽其断言；原始方法清单存于
  out/test-organization/test-inventory.json。新增 tests/README.md 说明各类脚本
  与固定语料的边界，当前文档和 cial-verify 的路径已更新。
- CTest 保留 CiallangCli，新增 CiallangDecompile/CiallangSwitch，因此 Windows
  注册项由 243 变为 **245**，Python 用例共 **51**。CMake 自动重新生成后构建
  退出 0（out/test-organization/build.log）；配置仍为 Windows Debug、clang++、
  CLI/TESTS/FORMATTER ON、JIT OFF、x64-windows-static、D:/cpp_tool/vcpkg。
- 全量 ctest --test-dir build/Windows/Debug --output-on-failure --no-tests=error
  -j 10：**245/245 通过**（23.84s，out/test-organization/ctest.log）。
  随后删除未使用 import，再执行三组进程 CTest **3/3 通过**
  （cli-groups.log）。git diff --check 通过。本轮没有修改 C++ 实现或字节码语料。

## 测试执行去重（2026-11-05，Windows）

- 用户要求精简重复测试，本轮仅整理测试与语料引用，未改反编译实现。
  原 switch 基础遍历与循环/嵌套/结构检查重复恢复同一份字节码，5 个语料测试
  合为一次 test_switch_corpus；独立源码形态与寄存器携带拒绝测试保留。
- manifest 作为单一预期来源：sourceChecks 覆盖原 19 份源码恢复；
  bytecodeChecks 保留 4 处函数入口声明和 7 处嵌套 if 检查、嵌套层数要求。
  每份字节码只恢复一次，移除 **23 次重复恢复**；源码与字节码的预期仍独立。
  与整理前清单核对：原样本、源/字节码哈希、成功/拒绝预期及拒绝原因均未变化。
  新增运行时哈希核对、非空语料/重复名字/未知结构检查字段校验。
- tests/test_files/tjsbc/debug/s7_ext.tjs 与上层 s7_ext.tjs 逐字节相同，删除
  debug 目录的重复源码，通过 debug manifest 的 source 字段引用上层；
  outputdebug 与普通字节码不相同，均保留。debug/decompile 全部输入哈希通过。
- runner 自测、SDK 安装与打包检查职责不同，未为减少文件数合并。测试导航
  已说明职责和新增样本方式。Python 测试方法从 51 合为 **47**（32/12/3），
  变化来自重复遍历合并，原结构与行为断言保留；CTest 仍注册 **245 项**。
- 全量 ctest --test-dir build/Windows/Debug --output-on-failure --no-tests=error
  -j 10：**245/245 通过**（22.49s，out/test-dedup/ctest.log）。
  覆盖核对摘要在 out/test-dedup/coverage.json，git diff --check 通过。

## 参考返回/三层合流与 ExprFunction（2026-11-05，Windows）

- 上一批累计提交已保存并同步为 `ce18c00`（master 与 origin/master 一致），
  本批在其上继续，修改尚未提交。
- 嵌套返回失败是 default/尾部 return 后不可达跳转没有落在已认领 switch
  区域内，仍被尝试发射。现在按 CFG 可达性折叠所有不可达块的跳转，
  不依赖 switch 是否认领该区域。没有删除可达跳转。
- 三层合流失败是重叠子区域把内层判别值 r9 当作跨区域存储，外层头之前又
  没有 r9 初始化。无回边 CFG 中逐读取核对其线性前驱定义的支配关系，
  全部支配时从合流候选排除；存在回边、隐式 receiver 写入或无法证明时
  保留原守卫。共享初值及真分支合流不因此放宽。
- 固定 `m5_switch_nested_returns` / `m5_switch_nested_three` 从 unsupported
  转为 success，保留所有源/字节码哈希，新增 bytecodeChecks 的 2/3 层断言。
  两组原源码、转译执行、恢复执行一致，参考原源码与恢复源码标记序列一致，
  原/恢复参考进程退出 0。`out/m5-next-close/reference-targeted.json` 留存证据。
- 赋值右值/实参 FuncLit 的参考字节码失败原因：参考文法 func_expr_def 创建
  ctExprFunction，显示名固定为 (anonymous)，非空名字并不代表具名表达式。
  DeclHost 新增来源判定，由 ShellAssembler 根据 TjsTranslatedContext::type
  判断 ExprFunction；这类常量不走具名声明安装链，按匿名表达式恢复。
  缺少此来源证明的非空具名表达式仍拒绝，不依赖显示名字字符串特判。
  依据：参考 src/syntax/bison/tjs.y func_expr_def、现有 translator context.type。
- 新固定 `m5_funclit_assignment`（变量赋值）与 `m5_funclit_argument`，Windows
  --dump 导出，manifest 保存哈希及参考身份（40a736c / 8f370f2b…358c60b）。
  CLI 新增三路固定输出测试（3/5/2 与 5），两组参考源码/恢复源码对拍通过，
  `out/m5-next-close/funclit-reference.json`。
- 对象成员赋值探测并未闭环：参考原程序 calls 为 0，Ciallang 为 2；恢复
  源码用 global.Dictionary() 构造空字典，参考返回 void，随后 spde 报错，
  退出 3221226505。该失败涉及 NEW 构造恢复与接收者/this 语义，独立保留
  `funclit-object-assignment-failure.json`；变量赋值的成功不能替代这项验收。
- 扩展全 switch 对拍时参考二进制出现间歇失败：一次原 do_in_case 退出
  3221225477，恢复 deep_calls 退出 3221226505；两份相同输入随后各重跑
  3/3 退出 0（reference-retries.json）。第二次扩展运行另有 nested_elseif
  失败。失败对应的既有恢复源码与此前通过报告逐字节一致，暂未归因，不能
  把重跑成功覆盖失败。两组新 switch 的针对性对拍通过；此次没有宣称新一轮
  37 组参考进程全部稳定通过。原始扩展报告 reference-report.json/reference-final.json。

- 最终构建 build-final.log 退出 0；Windows Debug，clang++、CLI/TESTS/FORMATTER
  ON、JIT OFF、x64-windows-static、vcpkg D:/cpp_tool/vcpkg。新增 1 个 FuncLit
  测试方法，Python CLI 共 48 项；CTest 注册数仍 245，全量
  `ctest --test-dir build/Windows/Debug --output-on-failure --no-tests=error -j 10`
  **245/245 通过**（22.45s，ctest-accepted.log）。全部 42 组语料哈希核对，
  clang-format 20.1.0 dry-run 与 git diff 检查通过。M5 总体仍未完成。

## CFG 安全降级首版完成（2026-11-05）

函数内 51 类受支持 TAC 的不可约 CFG 降级已实现并验收。实现不变量、拒绝边界、
首轮失败、原始 TAC/恢复源码/独立模型比较与参考验证见
[已实现记录](../../implemented/architecture/2026-11-05-decompiler-cfg-fallback.md)。
M5 专项 552 assertions / 4 cases 通过，最终全量 CTest 245/245（22.00s）；
4 份实际状态机函数的参考执行一致。当时记录的“下一大项异常区域与重抛”已在
`edbf3ae` 落地并于 2026-11-07 完成验收，见下节；不能把 CFG
拓扑支持等同于已经覆盖异常边、调用隐式写入、闭包或对象常量。

## 异常区域状态机首版验收（2026-11-07，darwin）

实现随 `edbf3ae`（2026-11-05）提交：状态机逐 pc 包 try、条件求值固定在原
比较 pc、catch 写异常槽后跳 `tryEnd`；Optimizer catch 入口不继承前驱拷贝事实、
转译器 ENTRY 内层优先登记配套修复。实现不变量与拒绝边界见
[异常区域实现记录](../../implemented/architecture/2026-11-07-decompiler-exception-regions.md)。
本批补齐的验收与修复：

- 基线复现 CiallangSwitch 33 个子失败：`decompile/` 语料源码哈希全部平台行尾
  依赖（manifest 按 Windows CRLF 检出计算、blob 为 LF、`.gitattributes` 漏了
  该目录），另 `m5_switch_single_default.tjs` 哈希与任何行尾变体不符（源码后改
  哈希未刷新；磁盘源/字节码对三路执行验证一致）。修复：`.gitattributes` 补
  `decompile/*.tjs text eol=lf` 与 `*.tjsbc -text`，manifest 42 组 sourceSha256
  按实际 LF 字节重生成。**Windows 侧拉取后须重检出该目录**。
- 参考二进制 `bin/darwin/TJSCompiler` SHA-256 `478eab17…b4471b1` 与 manifest
  `referenceDarwin` 一致。新增 4 组 darwin 语料（try_switch、catch_switch、
  nested_three、catch_reads_try_local，样本标 `reference: darwin`），异常语料
  共 9 组。
- `out/m5-exception/reference-check.py` / `reference-report.json`：9/9 哈希、
  三路 stdout、恢复回放逐字节一致；参考原源码 vs 字节码恢复源码标记序列
  9/9 一致、参考进程退出 0。参考原字节码直接加载环仍缺入口，不宣称四环
  全部通过。
- 边界探测 `out/m5-exception/probe_*.tjs`：try 体内含调用诚实拒绝（结构化
  M5 原因 + 状态机调用协议原因复合）；try+while+break/continue 穿越、catch 内
  switch、三层嵌套、catch 读 try 体局部经状态机成功（try_while 探针未固化）。
- `test_exception_regions_roundtrip` 补 manifest 哈希校验；断言保持形态无关。
  macOS Debug 全量 CTest **443/443 通过**（17.93s）。

## 结构化 try/catch 首版（2026-11-08，darwin）

基线 `9b819be`。结构化路径放开 handler 表整单拒绝（顶层仍拒）：`precomputeTryRegions`
认领 try 区域（区间/嵌套/出口 Jmp/唯一入口证明），`emitRegion` 优先认领 try 头，
体区与 catch 体递归发射；异常槽 Pass A/B 双游标重绑定；无绑定槽输出裸 catch。
实现不变量、首轮失败（同起点嵌套 map 碰撞、Pass B 游标重置、存储提升回退、
声明落点守卫）与验收证据见
[实现记录"结构化区域恢复首版"](../../implemented/architecture/2026-11-07-decompiler-exception-regions.md)。

- 能力：bc 路径 return/catch_switch 走结构化（manifest `structured: true`）；
  自产 catch 仅读异常槽/裸 catch/嵌套重抛结构化输出经 4 个新 Catch2 用例断言
  与回放验证。被拒形态自动落回状态机。
- 诚实拒绝保持：catch 读体寄存器（数据流）、声明落点内移（块作用域）、try 体
  内调用（复合原因）。`out/m5-exception/` 探针复核 11/11 行为符合登记。
- 9 组语料参考对拍复跑 9/9 一致；macOS Debug 全量 CTest **447/447**（含 4 个
  新用例）；clang-format 20.1.0 与 git diff --check 通过。

## catch 数据流存储提升（2026-11-08，darwin，基线 f347ff9）

catch 体读取的体定义无名寄存器提升为函数局部合成存储（`_tryStorageNames`）。
首批回退的三个根因按挂点解决：spanClean 跨提升存储重定义判脏（正确性关键，
先于 `_impureOrStore` 短路）、guardLoopCond 字面量判定前 resolveCopyNode 下钻
（修复 `0 < a0` 无名循环携带逃逸的独立漏洞）、noteNamedDef 补 Inc/Dec 与
NewArray/NewDict（IncDec 渲染 `name++;` 语句形态）。参数槽附入口拷贝并预登记
`_declaredLocals` 保证唯一 `var`。**修正记录**：localVars 的 pc 索引已随
pcAnchors 平移（Optimizer 压缩重映射 + parseAst 回填），此前"不重映射"表述
不准；真实缺口是死初始化删除后声明落点语义不保。

- 覆盖面：bc 语料 **8/9** 结构化（nested/empty/return/getter/try_switch/
  catch_switch/nested_three/catch_reads_try_local，manifest `structured: true`
  对 .tjsbc 输入断言无 `while (true)`）；`m5_exception_loop` 因无名循环携带 `i`
  被修复后的 guardLoopCond 正确拒绝，保持状态机回退。具名局部被 catch 读取
  （声明落点未证明）与 try 体内含调用保持诚实拒绝。
- 9 组语料参考对拍复跑 9/9 一致；macOS Debug 全量 CTest **447/447**；
  clang-format 20.1.0、git diff --check 通过。

## 无名循环携带变量提升（2026-11-08，darwin，基线 c36a27e）

提升机制泛化到循环：成员更名 `_promotedStorage`/`_promotedParamCopies`（不再
限于 try），抽 `storageReady`/`promoteRegister` 共用 helper；新增
`precomputeLoopStorage` 扫描已识别 while/do 循环的条件区——条件读取、体内
（含 for 更新区与锁存）重定义、无名绑定的寄存器提升为合成存储。条件渲染变量
名保住迭代语义，自增经 IncDec 语句形态渲染（批次 3 的 noteNamedDef 补路径），
跨迭代读取即变量当前值；`guardLoopCond` 对提升寄存器经 LocalName 早退自然放
行。顶层单元（无函数作用域）不提升，"循环携带变量"拒绝仅保留在顶层路径
（既有两个顶层拒绝测试保持通过）。

- 覆盖面：异常语料 **9/9 全部结构化**（`m5_exception_loop` 的 for + try +
  catch 读体寄存器 + catch 内 continue 翻转成功，manifest `structured: true`）。
  9 组语料参考对拍复跑 9/9 一致；macOS Debug 全量 CTest **447/447**；
  clang-format 20.1.0、git diff --check 通过。

## 调用标签表达式（2026-11-08，darwin，基线 29a11fd）

`case f():` 恢复。探针定位：判别值调用（`switch(d(x))`）与首组调用标签此前
已支持；后续分组调用标签被两条"仅常量装入"约束拒绝。放宽为**标签定义链**证
明（`isLabelDefChain`：前缀每条指令有定义、定义版本的全部使用落在分组派发区
内、无外溢——标签表达式按派发顺序求值一次，结果不外溢；结果无人消费的默认
体副作用不得移入标签）：

- 自产 `scanDispatch`：组间材料先纳入待校验，扫描结束统一校验；尾部未被比较
  闭合的材料保持仅常量的旧行为（链上下一分组的标签常量经版本链渲染，避免
  回归既有语料）。
- 参考模板 `dispatch`：块尾标签值判定从"常量装入"放宽为"定义比较右值"
  （`definesRegister(end - 3, match.rhs)`，调用 dst 在专用字段，op1 检查不再
  适用）；组间前缀同用 `isLabelDefChain`。
- 过程中发现两轮探针结果假阴性：工作目录漂移使 cmake 静默失败，测的是旧二
  进制（与 cial-verify 的产物身份核对教训一致）。
- 新增 4 组 darwin 语料 `m5_switch_calllabel_{first,later,discriminant,
  multigroup}`；无 break 模板下 default 落为 switch 后直线语句（语义等价，
  回放逐字节一致）。全量 CTest 447/447；clang-format、diff --check 通过。

## 状态机调用族与 try 写入侧合流（2026-11-08，darwin，基线 a9ffb22）

状态机白名单扩展（调研确认 Pass B tile 与 Push/Call 线性配对与块无关，状态机
按 pc 重放机器变量即可承载）：调用族 Push/Call/CallV/CallMethodV/CallOmit、
成员与容器语句族（GProp 族、NewArray/NewDict、Get/SetIndex、DProp 族、
DelProp 族、DerefGet/DerefSet、ToString/Inv/Debugger）、Load 的 Object 与
FuncMeta 常量。Call 实参 Push 与 Call 跨块时防御性拒绝（防未来发射器变化）。

- 接收者内联 `memberCalleeInlineOk`：GMethodC/GPropC 的 Member 节点唯一使用
  是同块调用的 callee 且基址（机器寄存器）未被重定义时，跳过逐 pc 物化、内联
  渲染 `o.m(...)`——getter 求值时机与异常归属随块保持；跨块回落物化。首版实
  现挂在 noteNamedDef 兜底处但 GMethodC 的构建点已先物化，旁路前移到
  buildInstruction 的 GPropC/GMethodC case 后生效。
- try 写入侧合流：体/catch 区定义、join 后读取的寄存器提升（此前只覆盖 catch
  读取方向）。`try{r=g();}catch(e){r=-1;} return r;` 的 bc 恢复曾恒渲染 -1
  （no-throw 路径值丢失），提升后 `v0 = g(); … v0 = -1; … return v0;` 回放一致。
- 函数字面量在状态机路径需装配宿主（`rebuildFunctionProgram` 不传宿主如实拒
  绝），CLI 路径经 ShellAssembler 提供宿主。每次 Load 独立创建闭包，不提升。
- 新增语料 `m5_exception_call_in_try`（bc 结构化 + 调用族，structured 标记）；
  Catch2 新增"状态机方法调用内联接收者"用例（Math.abs 形态）、旧"Call 必须拒
  绝"断言改为成功断言。异常语料 10/10 结构化。全量 CTest **448/448**；
  clang-format、diff --check 通过。

## 对象成员 FuncLit 与类字段初始化（2026-11-08，darwin，基线 481461a）

重建 2026-11-05 记录的 NEW/this 探针（Windows 原始产物不在本机）。当前 HEAD
下字典/全局成员/`new Dictionary()` 的 FuncLit 赋值自产与 bc 双路径均已通过
（批次 5/6 的调用族与成员语句修复的连带收益）；`this` 在函数字面量体内引用
全局定义处上下文报 Not found property 是正确的 TJS 语义（函数字面量捕获定义
处上下文），非缺陷。

闭环的真缺陷：类实例成员 FuncLit 赋值（`var o=new C(); o.m=function...;`，
类含无初始化器字段 `var v;`）的 bc 恢复把类字段渲染成 `var v = v0;`——v0 是
initDefaultVal 单元的物化临时（单元尾 `ret %0` 使 void 初值多使用而物化），
字段装配只取 DThis 语句、丢弃定义语句，类定义时求值即报 Not found property。
修复：collectFieldInits 把被字段初始化引用的 `var <名> = <值>;` 内联回初始
化值（`var v = void;`，语义与裸字段等价；临时名避开 Atom，全局同名不误替换；
链式临时迭代替换）。注意语句文本带尾分号，查表前剥离。

- 新增 2 组 darwin 语料 `m5_funclit_object_{newdict,classmember}`；全量 CTest
  **448/448**；clang-format、diff --check 通过。探针 out/m5-newthis/。

## 模糊覆盖落地与三个修复（2026-11-08，darwin，基线 c576dd5）

方案 §6 最终验收的"模糊覆盖"项落地：`tests/cli/test_fuzz.py`（CiallangFuzz，
120 个种子固定程序，受支持语句模板随机组合——表达式/赋值/容器/if/while/for/
do/switch/try/catch/调用，环①逐程序比对；棘轮式 KNOWN_FAILURES 登记——已知
失败程序显式跳过并打印，修复后不移除即断言失败）。多调用标签链
（`case f1(): case f2():` 混合常量组）探针全过，新增 2 组语料
`m5_switch_calllabel_{multi2,mixed}`。

模糊首跑抓到三个真缺陷，全部修复：

1. **条件区内定义的寄存器被循环携带提升**（`while (i < 0)` 的常量 0 装入寄
   存器被提升为 `v0`，恢复文本引用未定义变量）：precomputeLoopStorage 排除
   条件区内定义的寄存器，交回 guardLoopCond 既有判定。
2. **同名局部遮蔽塌缩**：同一寄存器索引在不相交 pc 段承载两个同名局部（作
   用域结束后的寄存器复用），`_declaredLocals` 按 (reg,name) 去重吞掉第二个
   作用域的 `var` 声明，两个运行时槽塌缩成一个名字导致死循环。改为按定义点
   所属作用域起点判定（`localScopeStart(reg,name,pc)`：endPC > pc 中 startPC
   最小者），新增 `_declaredScopeStarts`。
3. **恢复源码回放挂起**（1、2 的叠加形态）。

仍登记的已知缺口（棘轮）：**嵌套 do-while + 死初始化消除**的声明落点与区域
归约变形（program 49，repro 存 out/fuzz-known-issues/）——外层 do 体对局部 a
的赋值被渲染成块内 `var a = 2;`（顶层 a 初始化被优化器删除后，声明落点未锚
定到 localVars 语义位置）+ 内层 do 的常量条件提升泄漏，属同一声明落点协议缺
口。模糊测试生成器相应规避同层同名循环变量（注册于 KNOWN_FAILURES 并可棘轮
收紧）。全量 CTest **449/449**（新增 CiallangFuzz）。

## M4 移交验收：deref 修复 + super prologue（2026-11-08，darwin，基线 b2d2291）

- **deref bc 恢复缺陷修复**：`*p` 读取 tile 在 Pass B 捕获操作数节点，不感知
  后续物化改名——property 引用多使用物化后，读取渲染成 `*&Cell().n`（& 表达
  式每次重新求值，receiver/计数漂移）。修复：DerefGet 的解引用节点携带版本槽
  （srcVersion），渲染走 renderVersion（`*(v2)`）。新增语料
  `m5_deref_property`（property 引用 + *p 读写的 bc 恢复）+ CLI 断言。
- **super 构造 prologue**：自产路径恢复验收通过（派生构造器内基类构造调用
  `B(7);` 正确恢复，CLI roundtrip 断言 `B(7);` 结构）。bc 路径被参考编译器
  **段错误阻断**（exit 139；单行类形态参考运行期亦异常）——参考侧平台缺陷，
  按cial-diff 纪律记录绕开。
- **独立语言层发现（非反编译器）**：`&o.成员` 在**顶层上下文**走 getter 取值
  （*p 报 not a property），**函数内**同形态走引用协议——IRGen 的 & 协议上下
  文差异；`&` 对普通 var 成员同样被 deref 拒绝（VM 仅接受 Property 对象）。
  需单独开问题调查（proposed/bug-fix）。
- 全量 CTest **449/449**；参考对拍 10/10 一致；clang-format、diff --check 通过。

## 建议下一步

1. 嵌套 do-while + 死初始化消除的声明落点协议（模糊已登记 repro）。
2. switch 后续批次：参考更复杂合流形态；按方案 §7 继续参考 TJS2 四环验收。
2. `CallMethodV` 行表收敛：先在 Linux 侧复核 M4 记录与本机差异，再修
   origin 分段继承。
3. 具名局部被 catch 读取的声明落点协议（需 IRGen 侧把 `var` 落点锚定到
   localVars 语义位置），优先级低于上述主线。

## 完成条件

方案 §6 M5 行的验收达成：能力矩阵逐项验收、Unsupported 集合可量化；
移交的验收扩展：deref（testDeref 记法所指的 &/* 形态）bc 恢复已修复并验收、
super 构造 prologue 自产路径已验收（bc 侧被参考编译器崩溃阻断，见 2026-11-08
批次）；`CallMethodV` 行表需 Linux 侧；
s7_ext 差异归因并修复；表达式位置 FuncLit round-trip 通过。

## 风险或备选方案

任意 CFG 降级状态机可能显著扩大 Unsupported 拒绝面；switch 链恢复必须
证明比较顺序与贯穿语义等价才输出，宁可拒绝不可猜。`CallMethodV` 行表修复
不得为凑期望值而特判语料，应以 origin 条目语义为准。
