# SDK 与开发工具路线图批次状态

Status: proposed

## 问题

[路线图](../../../../doc/roadmap.md)定义了批次方向与验收条件，但批次级执行状态此前只记在待办；
本 note 接续追踪各批次剩余工作，避免设计文档与执行状态混在一处。

## 已知事实与证据

- 批次 1（SDK 兼容承诺与仓库拆分）：install/export、Engine 入口、逻辑源码身份、
  安装后示例、公共头两层承诺与能力查询（`cial/Capability.hpp` + `cial --capabilities`，
  证据见 [implemented note](../implemented/architecture/2026-11-09-sdk-public-header-capability.md)）
  已完成；**三平台远端安装验收已收口（2026-11-10，run 42775661634）**：
  "Test relocated installed SDK" 在 Linux/macOS/Windows Release 及 Windows
  Release JIT-ON 四个 job 全部通过，Ubuntu 另含 "Configure library consumer"。
  前置阻塞为此前 Windows Release JIT-ON job 的 CiallangCli 失败（同日修复，
  见该 note）。剩余：拆分核心与开发工具仓库并建立独立发行。
- 批次 2 进行中：API 元数据模型与 `cial --api-json` 导出已落地（2026-11-11，模型
  `src/types/ApiMeta.hpp`、聚合 `ApiCatalog` 挂 `NativeRegister`、导出 schemaVersion 1；
  依据与推导规则见 doc/api-metadata.md，测试 `[api]` 标签 + test_cli.py 的
  test_api_json）。剩余：`.d.tjs` 声明模式（Parser decl 模式、执行侧拒绝、生成器、
  测试扫描排除）与 LSP 消费声明（替换 `BUILTIN_MEMBERS`，doc 文案随注册点逐步补全）。
- 批次 3 未开始：模块作用域、显式导出、宿主解析、Context 缓存、循环诊断及
  异常/GC 恢复，保留 `Scripts` 兼容入口；稳定后再讨论 `import/export` 语法。
- 批次 4 未开始：核心脚本调试接口、DAP 适配器及 VSCode 端到端调试，
  先覆盖行断点、单步、调用栈、变量和异常暂停。已定决策：DAP 适配层用 C++ 内建
  `cial-dap`（链接 cialdbg 复用 DebugSession 能力），不引入 Python 适配层。

## 建议下一步

按批次顺序推进；每批先在路线图核对设计边界与依赖，完成后把状态回写本 note
并更新路线图对应章节。

## 完成条件

各批次验收条件以[路线图](../../../../doc/roadmap.md)对应章节为准；
批次完成即更新本 note 状态，全部完成后移入 `implemented/` 或归档。

## 风险或备选方案

批次 2 的声明文件分析影响 LSP 与核心前端两条路径，须先定接口再实现；
批次 3 模块语义稳定前不引入 `import/export` 语法。
