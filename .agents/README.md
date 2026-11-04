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

- [反编译器 M5 与移交遗留](notes/proposed/architecture/2026-11-04-decompiler-m5.md)：M0–M4 已完成
  （记录在方案 §6）；顶层物化项闭环、类体逗号声明链与 outputdebug 行表读取修复
  （CTest 243/243）；M5 首批为 switch 链，super/`CallMethodV` 恢复形态与
  表达式位置 FuncLit 在册。
- [krkr2 接入](notes/proposed/architecture/2026-11-04-krkr2-integration.md)：五个阶段均未开始，
  入口为双向 `iTJSDispatch2` 桥；完成标准见[接入说明](../doc/krkr2-integration.md)。
- [SDK 与工具路线图批次](notes/proposed/architecture/2026-11-04-sdk-tooling-roadmap.md)：
  批次 1 部分完成，批次 2–4 未开始；设计边界以[路线图](../doc/roadmap.md)为准。
- [诊断批次 5 剩余](../doc/diagnostics-plan.md#5-实施状态2026-10-31)：剩余 JIT 等价位置验证
  （继续扩展原生互调与异常恢复组合），状态以该文档实施状态一节为准。
- [GC 标记工作队列](notes/proposed/performance/2026-11-04-gc-mark-worklist.md)：对象图标记仍为
  C++ 递归，Windows 依赖 16 MiB 显式栈预算。
- [GCC 整数循环回退](notes/proposed/performance/2026-11-04-gcc-integer-loop-regression.md)：
  语义对齐批次后约 +10%，TAC 一致，待归因；正式发布前复核。
- [Windows 类压力回退](notes/proposed/performance/2026-11-04-win-release-class-pressure.md)：
  异常栈/getter 隔离批次后约 +8%，两次测量方向一致，尚未归因。
- [stdlib 临时值拷贝](notes/proposed/performance/2026-11-04-stdlib-temp-copy.md)：
  优化候补，先测拷贝占比再决定是否立批次。

其他提案按需浏览 `notes/proposed/`，不因为文件已存在就视为已实现。
历史测量与验证放 `notes/implemented/`，否决提案与理由放 `notes/rejected/`
（如[嵌套函数自由变量](notes/rejected/bug-fix/2026-11-04-nested-function-free-vars.md)，
参考 TJS2 同样拒绝，非兼容缺口），早期失效分析放 `notes/archived/`。
