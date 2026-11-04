# 反编译器 M5 与移交遗留

Status: proposed

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

## 建议下一步

1. switch 后续批次：扩展有副作用的 case 派发前缀、嵌套与 for 更新段 continue 组合；
   扩展参考 switch 的嵌套与 case 内条件分支；多值标签与具有共同初始化点的函数局部
   合流已恢复，顶层合流仍拒绝；按方案 §7 继续参考 TJS2 四环验收。
2. `CallMethodV` 行表收敛：先在 Linux 侧复核 M4 记录与本机差异，再修
   origin 分段继承。
3. try/catch 嵌套兜底（任意 CFG 降级状态机）、with、eval 名字依赖按方案批次推进。
4. 表达式位置 FuncLit（赋值右值）随壳装配批次落地。

## 完成条件

方案 §6 M5 行的验收达成：能力矩阵逐项验收、Unsupported 集合可量化；
移交的验收扩展（super prologue、`CallMethodV`/`testDeref`）逐个通过；
s7_ext 差异归因并修复；表达式位置 FuncLit round-trip 通过。

## 风险或备选方案

任意 CFG 降级状态机可能显著扩大 Unsupported 拒绝面；switch 链恢复必须
证明比较顺序与贯穿语义等价才输出，宁可拒绝不可猜。`CallMethodV` 行表修复
不得为凑期望值而特判语料，应以 origin 条目语义为准。
