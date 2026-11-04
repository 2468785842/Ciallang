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

## 建议下一步

1. switch 链（JmpE + `_zf` 直写比较族）：严格模式当前按 M5 批次号拒绝
   （`test_decompile_strict_rejects_structure_with_batch_reason` 锁定），
   按[方案 §6 M5 行](../../../../doc/bytecode/decompiler-plan.md#6-实施批次)
   与 §7 四环验收，是 M5 首个实现批次。
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
