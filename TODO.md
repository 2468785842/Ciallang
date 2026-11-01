# 待办

只记录尚未完成的工作；完成后删除条目。使用限制见 [FAQ](doc/faq.md)，历史测量见[性能记录](doc/performance.md)。

## 性能与优化

- [ ] 定位语义/SDK 对齐批次的 GCC 全局整数循环回退（同机 Release 约 +10%，生成 TAC 一致）；正式发布前复核，见[测量](doc/performance.md#2026-11-01-语义对齐与-sdk-基础批次)。

- [ ] 评估 stdlib 内部临时值拷贝消除；继续采用同机 Release 对照及异常/GC 验证。已有调用布局、Value 生命周期与 asInteger 快路径结论见[性能记录](doc/performance.md)。

## SDK 与开发工具路线图

- [ ] 完成诊断重构批次 5 剩余项：源码正文快照共享（摘录已按 linecache 式重读渲染，文件编译后被改动的窗口待快照消除；跨 eval/闭包/REPL 历史取回当时正文）、结构化异常栈帧（多帧调用点）与栈降级、JIT 等价位置验证。故障位置元数据化、Chunk 源码身份、顶层 `源:行:列` 与运行期摘录已落地，验收见[诊断重构计划](doc/diagnostics-plan.md)实施状态一节。

设计边界、依赖与验收条件见[路线图](doc/roadmap.md)；以下均为未完成项。

- [ ] 批次 1：收敛公共 SDK 兼容承诺与能力查询，完成三平台远端安装验收，再拆分核心与开发工具仓库并建立独立发行。install/export、Engine 入口、逻辑源码身份和安装后示例已有实现。
- [ ] 批次 2：实现 API 元数据与 `.d.tjs` 声明模式，连接标准库/宿主导出与 LSP，逐步统一核心前端分析；执行器和脚本扫描须正确区分声明文件。
- [ ] 批次 3：实现模块作用域、显式导出、宿主解析、Context 缓存、循环诊断及异常/GC 恢复，保留 `Scripts` 兼容入口；稳定后再讨论 `import/export` 语法。
- [ ] 批次 4：提供核心脚本调试接口、DAP 适配器及 VSCode 端到端调试，首先覆盖行断点、单步、调用栈、变量和异常暂停。

## 首次 CLI 预览版

- [ ] 提交发布基础后，手动运行三平台 package-and-release 工作流并检查附件、许可及构建信息；全部验收通过后，在确认提交上推送版本标签，核对并公开 Release 草稿。当前只完成本机 Windows 包验收，见[发布指南](doc/releasing.md)。

## krkr2 接入

目标为 [2468785842/krkr2](https://github.com/2468785842/krkr2)，接入位置与验收门槛见[接入说明](doc/krkr2-integration.md)。

- [ ] 实现双向 `iTJSDispatch2` / VariantClosure 桥：对象身份、AddRef/Release 与 GC 根、flags、Operation、枚举、NativeInstanceSupport、ClassInstanceInfo、无效化和异常转换。
- [ ] 补齐字节码 proxy 的 direct/indirect 回落、缺失成员 typeof、一次 Operation 派发与动态类体/父类表达式。
- [ ] 对接 TVP 文本/二进制读写流；Scripts 文本读取器已有入口，Array/Dictionary 保存与加载仍直接使用文件系统。接入 XP3、模式串、字节码探测与源码身份映射。
- [ ] 在目标仓库增加可切换后端并验证 startup/Initialize、KAG、Window/Layer/Timer 回调、插件、存档和退出回收；桌面通过后验证 Android ARM64/x86_64。
- [ ] 补齐其余 Unicode 边界（sprintf 的字符/宽度/精度、大小写及文本存取），建立目标 TJS2 与真实游戏语料的持续差分验收。
