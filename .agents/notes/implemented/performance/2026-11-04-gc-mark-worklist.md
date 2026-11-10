# GC 对象图标记改显式工作队列

Status: implemented（2026-11-10）

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

## 实现（2026-11-10，Windows）

- `MarkSweepHeader::marked()` 收敛为"标记自身并入队"（thread_local 工作队列，
  单线程安全点协议不变）；子对象遍历拆到新虚函数 `markChildren()`，由
  `MarkSweep::drainMarkQueue()` 排空——深图遍历 O(1) 栈深。
- 全部派生类 override 同步转换（Array/Dictionary/Class×3/Function×2/Property/
  RegExp/NativeObject/Chunk/Constant×3/Binding/MathLib/ScriptsLib）；
  RandomGen 无子对象，声明与实现一并移除。守卫语义不变：基类 marked() 统一
  判重入队，对象图环不再依赖派生类守卫。
- 标记入口审计：全部根标记（Runtime handles/RegExp/pendingFinalizers +
  Context 全局对象/寄存器池/调用栈）都在 `collectAllMark()` 闭包内经
  `collect()` 触发；`collect()` 在 mark 与终结器预标记后各 drain 一次。
  binding Tracer 仅在标记阶段（NativeObject::markChildren / 装配 marked）内
  调用，无 collect 外裸调用点，预标记语义不变。
- tests/CMakeLists.txt 的 Windows `LINKER:/STACK:16777216` 特例移除；
  [FAQ](../../../../doc/faq.md#windows-ci-的深数组压力测试为什么配置较大栈)
  改写为历史条目。

## 验证证据（2026-11-10，Windows x64 Release）

- 全量 CTest **249/249**（默认线程栈，无 16 MiB 特例；含 2 万层深数组标记
  用例与 GC 生命周期回归）。
- 队列峰值对照（out/gc-queue/probe.cpp 探针，池 8 万节点）：
  20k 深链 + 单根 2 万元素宽图 + 1.5 万层混合树，峰值 **20001 条**（宽图
  单根一次入队全部子节点，约 160 KB 指针队列）；深链 DFS 下队列恒为 O(1)。
  对照旧实现同图需 2 万层 C++ 栈帧（当时 Linux 1 MiB 栈实测溢出）。
- clang-format 20.1.0 通过；`git diff --check` 通过。

## 完成条件

深数组压力测试在默认栈预算通过；全量 CTest 与 GC 生命周期回归通过；
FAQ 中栈预算说明同步改写为历史记录。——**全部达成（见上节）**。
