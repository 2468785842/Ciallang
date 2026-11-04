# 反编译器 M5 与移交遗留

Status: proposed

## 问题

反编译器进入 M5（硬核兜底）前有三类未闭环：M5 本体的 switch 链、try/catch 嵌套兜底、
with 与 eval 名字依赖；M4→M5 移交的 bc 路径验收扩展；以及顶层 getter+调用混合链的
临时值物化策略尚未决定。

## 已知事实与证据

- 设计与批次见[反编译器方案 §6](../../../../doc/bytecode/decompiler-plan.md#6-实施批次)；
  M0–M4 已完成并逐批记录在该表（M4 收尾 2026-11-04，CTest 431 全绿），不再重复维护流水。
- M5 范围：switch 链（JmpE + `_zf` 直写比较族）、try/catch 嵌套兜底（任意 CFG 降级
  状态机）、with、eval 名字依赖；按[方案 §7 四环](../../../../doc/bytecode/decompiler-plan.md#7-验证闭环)逐批验收。
- M4 移交的 bc 路径验收扩展：super 构造直调 context 的实参搬运 prologue
  （读未定义寄存器形态）、`CallMethodV`/`testDeref` 形态逐个落地后按四环对拍；
  语料见 `tests/test_files/tjsbc/debug/manifest.json` 的 pendingUnits。
- M3 遗留小项：顶层 `for` 初始化变量（寄存器携带无名字，如实拒绝）、
  `while` 条件内联存储歧义拒绝。
- 未决设计点：顶层 getter+调用混合链（`o.a + o.b + o.m()` 形态）的临时值跨脏区间
  需物化而顶层无局部作用域；合成局部名有方案 §3 动态名查找风险。

## 建议下一步

1. 先定顶层临时值物化策略（合成局部名与拒绝之间取舍），再进 M5 批次。
2. switch/try/with/eval 按方案 §6 M5 行拆小批，逐形态四环验收。
3. M4 移交形态随 bc 路径逐个落地，同步补 pendingUnits 语料。

## 完成条件

方案 §6 M5 行的验收达成：能力矩阵逐项验收、Unsupported 集合可量化；
移交的验收扩展逐个通过；物化策略形成决定并回写方案文档。

## 风险或备选方案

任意 CFG 降级状态机可能显著扩大 Unsupported 拒绝面；若选合成局部名物化，
需先核实方案 §3 动态名查找的实际触达范围。
