# SSA 交接与协作状态（2026-10-29 深夜收工快照）

> 由 DustyKite（ZCode 会话）按用户指示收工时写。下次任一 agent 会话从本文件继续。
> 功能文档见 [doc/ssa.md](ssa.md)（SunnyMouse 撰写，含 `--dump-ssa` 用法）。

## 本次已完成

- **SSA 三文件交付并全绿**（Agent Mail 线程 `readme-completion-20261029`，我方任务）：
  - `src/gen/SSA.hpp` / `src/gen/SSA.cpp`：按需构造——CHK 迭代支配树、支配边界、
    块级活跃性、pruned phi（判据 liveIn）、Cytron 重命名、def-use 链；
    GMethodC 多定义以 slot 区分身份；异常着陆块 = SSA 边界（Catch 每块独立身份、
    整栈快照恢复）；CallOmit/CallExpand/CollapseArgs 隐式读帧实参→明确拒绝；
    未知 opcode 抛错不猜测；facts 变量越界校验；入口自环支配边界。
  - `tests/ssa_test.cpp`：20 用例 135 断言全绿（scratch 构建验证）。
  - SunnyMouse 的 4 例集成验收（`tests/ssa_integration_test.cpp`）通过。
  - **Debug 完整套件 194 用例 / 1411 断言全绿**（`build/Windows/Debug`，按 #30 交接构建）。
- 审核反馈 #24–#31 全部修复；ChgThis/ChkIns/GMethodC 语义逐一对照 `VMState.cpp`
  dispatch 核实（ChgThis 只改堆对象；ChkIns 读改写 op1；GMethodC 双定义）。

## 未完成任务（下次继续）

1. **回帖交付 SunnyMouse**（Agent Mail 恢复后发送；草稿见下节）。内容：交付清单、
   测试结果、Debug 目录与文件租约交回。
2. **Agent Mail 服务器已下线**：Codex 关闭带走 HTTP 服务（127.0.0.1:8765 无监听）。
   重启方式：`C:\Users\LiDon\AppData\Local\mcp-agent-mail\start-agent-mail.ps1`
   （幂等，先探测端口再拉起）。
3. **审查任务（SunnyMouse 已预约我）**：IntegerJit.cpp / typeguard / 异常边界 /
   trySelfTailCall 白名单（#13/#18），尾递归安全子集"遗漏的可观察语义"专项。
4. **README/AGENTS 整合**：「SSA 静态单赋值构造」勾选与目录地图登记由 SunnyMouse
   统一做（doc/ssa.md 已就位）；我方支持范围表述可直接引用本文上节。
5. **租约善后**：我方 `src/gen/SSA.*`、`tests/ssa_test.cpp` 预约（TTL 至 16:26Z）
   交付后应 release；Debug 目录认领因 mail 宕机未登成功，重建后补登/交回。
6. **后续里程碑**：JIT/Trace JIT 合并后联调；LSP 在核心完成后另议（#18）。

## 交付回帖草稿（下次直接贴）

> SSA 交付完毕：三文件如约（`SSA.hpp/SSA.cpp/ssa_test.cpp`），你登记的 CMake 路径
> 命中。修复清单对照你的 #24–#31：liveIn 判据 phi、入口 enterBlock、ChkIns 读改写、
> ChgThis 只读、GMethodC 双定义（SsaRef 增 slot；未拒绝）、CallOmit/Expand/Collapse
> 明确拒绝（错误信息含 opcode）、异常边界整栈快照 + Catch 每块独立身份（#27 采纳，
> 未用 Entry 混身份）、facts 越界校验、入口自环 DF 补齐。验证：ssa_test 20 例 135
> 断言 + 你 ssa_integration_test 4 例全过；Debug 全套 194 例/1411 断言全绿
> （按 #30 交接用完 Debug 目录，现交回）。用户指示本轮收工已统一 git 提交推送
> （覆盖原"暂不提交"约定）。JIT 审查收到，下轮开始；#13 的可观察语义意见：执行
> 路径 trace 边计数在帧复用下呈环状属预期，建议 doc 标注；实参窗口/对象排除/
> 深度上限均已覆盖，未发现新的遗漏。

## 协作约定备忘

- 文件预约先行、冲突即停；构建目录轮流（Debug 现已交回 SunnyMouse；Release/Jit 归它）。
- 共享文件（CMake/README/AGENTS/main.cpp）由 SunnyMouse 整合。
- git：原约定"整体审查后再提交"，本次经用户明确指示提前提交推送。
