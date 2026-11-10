# stdlib 内部临时值拷贝消除评估

Status: implemented（2026-11-10 决定：不立批次）

## 问题

标准库实现内部可能存在可消除的临时 Value 拷贝；该项为持续优化候补，
先测占比再决定是否立批次。

## 已知事实与证据

- 已有结论（调用布局、Value 生命周期与 asInteger 快路径）见
  [性能记录](../../../../doc/performance.md)；方法约定为同机 Release 对照及异常/GC 验证
  （根 [AGENTS.md](../../../../AGENTS.md) 构建与验证节）。
- 约束：字符串借用返回指针不能超过所属 Value 有效期（`src/types/Value.hpp` 的 asString），
  拷贝消除不得延长借用指针的生命周期。

## 测量与决定（2026-11-10，Windows x64，clang 19.1.5 Release）

- **方法**：临时插桩 Value 拷贝构造计数 + `invokeCallV` 原生作用域标记
  （NativeFunction::callDirect 与对象虚 call 内同步工作计为 stdlib 内；容器
  getProp/setProp 属性协议路径未计入），测后还原。
- **数据**：native_methods 压力（100 万次原生调用）：总拷贝 8.0 次/调用、
  stdlib 内 3.0 次/调用（占全部拷贝 37.5%）；类压力（20 万次构造）：虚 call
  内 8.0 次/构造；string_concat 压力：stdlib 内 0；loop_int：全程 31 次拷贝。
- **时间估算**：原生调用约 150 ns/次，stdlib 内 3 次拷贝约 3-6 ns，上限约
  2-4%——且是 100% 原生调用的微基准，混合负载可忽略。
- **决定：收益不足，不立批次**。最明显的单点是 `NativeFn` 的 `Value thisVal`
  传值形参（约 1 拷贝/次），改 const 引用需全量重构 stdlib 签名；若未来做
  调用机械批次可顺带处理。原始计数见[测量报告](../../../reports/2026-11-10-perf-attribution.json)。

## 完成条件

要么形成"收益不足，不立批次"的决定并记录测量，要么完成消除批次并验证
（可观察结果、异常恢复、GC 生命周期）。——**已形成不立批次决定并记录测量**。

## 风险或备选方案

拷贝消除的常见风险是借用指针悬挂与 GC 根缺失；逐库小步进行，
不一次性修改 Value 语义。
