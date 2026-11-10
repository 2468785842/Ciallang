# AI 协作

先读根 [AGENTS.md](../AGENTS.md)，再按当前任务选择相关资料。
当前实现与操作指南以 [doc](../doc/README.md)、源码和 CMake 为准。

## 组织边界

| 位置 | 内容 |
|---|---|
| 根 `AGENTS.md` | 项目约束与任务路由 |
| [notes](notes/README.md) | 问题调查、设计提案、决策依据与历史验证 |
| [cial-verify](skills/cial-verify/SKILL.md) | 构建、测试范围、注册与结果证据 |
| [cial-diff](skills/cial-diff/SKILL.md) | 与参考 TJS2 引擎的对拍与差分验收 |
| `reports/` | 入库的小型测量报告，保留环境和样本数据 |
| 仓库 `out/` | Git 忽略的原始日志、dump、临时调试脚本 |

这里维护的是项目工作方法和任务记忆，执行仍使用现有终端、IDE、调试器和测试工具。
技能是否被当前宿主自动发现由宿主决定；未发现时按上述路径直接读取，
不要求安装全局技能副本。

## 接续工作

1. 查看 git status/diff，保留未提交改动；按主题搜索活动 notes，只加载相关内容。
2. 核对 note 的版本、路径、构建开关与产物身份，选用匹配技能。
3. 定义要保持的可观察行为，执行最小修改与相关验证。
4. 将新证据、仍缺的证据和具体下一步更新到原 note；实现事实同步到 doc/ 专题文档。

用户只说"继续"时延续当前主题，不把所有活动 notes 自动扩展为本轮任务。
验证结论应注明范围，未复现不关闭问题；通过条件以对应记录和真实行为为准。
可重复的方法逐步提炼进技能，可执行逻辑放 tools/scripts，避免在技能中复制整套工具实现。

## 当前各项工作的顺序

- [反编译器 M5 与移交遗留](notes/implemented/architecture/2026-11-04-decompiler-m5.md)：
  **已完成（2026-11-10 关闭评审）**：switch/循环组合、嵌套、异常区域、状态机
  降级、with/eval 全部验收，模糊 KNOWN_FAILURES 清零，能力矩阵与 Unsupported
  清单已量化，移交项全部闭环；关闭证据为 Windows Release 248/248、WSL Linux
  Debug 449/449、CI 八 job 全绿，见
  [当前状态表](../doc/bytecode/decompiler-plan.md#m5-当前状态2026-11-05)。
  测试脚本已按 CLI/反编译/switch 分组，入口见[测试导航](../tests/README.md)。
- [krkr2 接入](notes/proposed/architecture/2026-11-04-krkr2-integration.md)：五个阶段均未开始，
  入口为双向 `iTJSDispatch2` 桥；完成标准见[接入说明](../doc/krkr2-integration.md)。
- [SDK 与工具路线图批次](notes/proposed/architecture/2026-11-04-sdk-tooling-roadmap.md)：
  批次 1 部分完成，批次 2–4 未开始；设计边界以[路线图](../doc/roadmap.md)为准。
- [诊断批次 5 剩余](../doc/diagnostics-plan.md#5-实施状态2026-10-31)：剩余 JIT 等价位置验证
  （继续扩展原生互调与异常恢复组合），状态以该文档实施状态一节为准。
- 性能提案已全部收口（2026-11-10，见
  [性能记录](../doc/performance.md#2026-11-10-三项遗留归因与-gc-标记队列化)）：
  GC 标记改显式工作队列（16 MiB 栈特例移除）；GCC 整数循环回退二分定位到
  全局快路径守卫的 `Context::global()` 跨 TU 调用并消除；Windows 类压力回退
  归因于 getter 异常隔离哨兵帧（~95 ns/次，接受并登记优化候选）；stdlib
  临时值拷贝占比测量后决定不立批次。

其他提案按需浏览 `notes/proposed/`，不因为文件已存在就视为已实现。
历史测量与验证放 `notes/implemented/`，否决提案与理由放 `notes/rejected/`
（如[嵌套函数自由变量](notes/rejected/bug-fix/2026-11-04-nested-function-free-vars.md)，
参考 TJS2 同样拒绝，非兼容缺口），早期失效分析放 `notes/archived/`。
