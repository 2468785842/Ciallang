# GC 对象图标记改显式工作队列

Status: proposed

## 问题

对象图 GC 标记（`Array::marked()` 等）仍是 C++ 递归，深图标记依赖宿主线程栈，
深数组用例需显式栈预算兜底；应改为显式工作队列消除对栈的依赖。

## 已知事实与证据

- 2 万层数组用例在 Linux 1 MiB 栈子进程复现溢出、8 MiB 通过；Windows 默认线程栈更小，
  `CiallangTests` 在 Windows 显式预留 16 MiB，见
  [FAQ](../../../../doc/faq.md#windows-ci-的深数组压力测试为什么配置较大栈)。
- 约束见根 [AGENTS.md](../../../../AGENTS.md)"GC 与持有关系"行：
  新持有者必须接入标记，sweep 不执行脚本；入口为 `Runtime.hpp` 的 create/RootGuard、
  `gc.hpp` 的 marked/collect。

## 建议下一步

把标记遍历改为显式工作队列（迭代化），移除 Windows 的 16 MiB 特例预算；
保持标记语义与根集合不变。

## 完成条件

深数组压力测试在默认栈预算通过；全量 CTest 与 GC 生命周期回归通过；
FAQ 中栈预算说明同步改写为历史记录。

## 风险或备选方案

队列化改变标记访问顺序，需确认不影响 sweep 与弱引用的可观察行为；
工作队列内存峰值与原递归栈深度需同机对照。
