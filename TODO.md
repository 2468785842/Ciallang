# 待办

只记录尚未完成的工作；完成后删除条目。使用限制见 [FAQ](doc/faq.md)，历史测量见[性能记录](doc/performance.md)。

## 性能与优化

- [ ] 定位语义/SDK 对齐批次的 GCC 全局整数循环回退（同机 Release 约 +10%，生成 TAC 一致）；正式发布前复核，见[测量](doc/performance.md#2026-11-01-语义对齐与-sdk-基础批次)。

- [ ] 复核结构化异常栈/getter 隔离批次的 Windows Release 类压力回退（2 万次实例化，同机两次约 +8%，尚未归因），见[测量](doc/performance.md#2026-11-01-结构化异常栈与-getter-异常隔离)。

- [ ] 评估 stdlib 内部临时值拷贝消除；继续采用同机 Release 对照及异常/GC 验证。已有调用布局、Value 生命周期与 asInteger 快路径结论见[性能记录](doc/performance.md)。

## 字节码反编译器

设计与批次见[反编译器方案](doc/bytecode/decompiler-plan.md)（2026-11-01 采纳 TAC 层直译路径）。

- [ ] M1 残项：现有 tjsbc 语料均为无 debug 产物，需生成/收集 outputdebug 样本验证
  `--source` 行号换算的端到端（合成用例已在 `tests/decompile_test.cpp` 覆盖）；固定参考
  二进制/版本/样本清单（哈希与导出选项）。

- [ ] M2–M5：后优化形态表达式恢复（含 BinaryImm/CompareTestImm 逆映射）、语句结构化与短路/三元跨块折叠、壳组装与 `+=` 逆融合双守卫（模板命中 + `computeBlockLiveness` 无其他读者）、任意 CFG/switch 链/try 嵌套兜底；按方案 §7 四环验证逐批验收。

M0 已完成（2026-11-01）：`src/decompile/` 的 `{文本, 行表}` 管道、CLI `--decompile`（严格模式
默认 / `--view` 反汇编视图 / `--map` 行表 JSON / `--source` 偏移换算）、TJS2 路径来源表
（`TacOrigin`，码字坐标 + 可选源偏移）随转译器透传；两条入口反汇编级端到端，CTest 237 全绿。

- [ ] M2–M5：后优化形态表达式恢复（含 BinaryImm/CompareTestImm 逆映射）、语句结构化与短路/三元跨块折叠、壳组装与 `+=` 逆融合双守卫（模板命中 + `computeBlockLiveness` 无其他读者）、任意 CFG/switch 链/try 嵌套兜底；按方案 §7 四环验证逐批验收。

## SDK 与开发工具路线图

- [ ] 完成诊断重构批次 5 剩余的 JIT 等价位置验证（继续扩展原生互调与异常恢复组合）。故障位置、源码快照、结构化异常栈与栈降级已落地，验收见[诊断重构计划](doc/diagnostics-plan.md)实施状态一节。

设计边界、依赖与验收条件见[路线图](doc/roadmap.md)；以下均为未完成项。

- [ ] 批次 1：收敛公共 SDK 兼容承诺与能力查询，再拆分核心与开发工具仓库并建立独立发行。install/export、Engine 入口、逻辑源码身份、安装后示例与三平台远端安装验收已完成。
- [ ] 批次 2：实现 API 元数据与 `.d.tjs` 声明模式，连接标准库/宿主导出与 LSP，逐步统一核心前端分析；执行器和脚本扫描须正确区分声明文件。
- [ ] 批次 3：实现模块作用域、显式导出、宿主解析、Context 缓存、循环诊断及异常/GC 恢复，保留 `Scripts` 兼容入口；稳定后再讨论 `import/export` 语法。
- [ ] 批次 4：提供核心脚本调试接口、DAP 适配器及 VSCode 端到端调试，首先覆盖行断点、单步、调用栈、变量和异常暂停。

## krkr2 接入

目标为 [2468785842/krkr2](https://github.com/2468785842/krkr2)，接入位置与验收门槛见[接入说明](doc/krkr2-integration.md)。

- [ ] 实现双向 `iTJSDispatch2` / VariantClosure 桥：对象身份、AddRef/Release 与 GC 根、flags、Operation、枚举、NativeInstanceSupport、ClassInstanceInfo、无效化和异常转换。
- [ ] 补齐字节码 proxy 的 direct/indirect 回落、缺失成员 typeof、一次 Operation 派发与动态类体/父类表达式。
- [ ] 对接 TVP 文本/二进制读写流；Scripts 文本读取器已有入口，Array/Dictionary 保存与加载仍直接使用文件系统。接入 XP3、模式串、字节码探测与源码身份映射。
- [ ] 在目标仓库增加可切换后端并验证 startup/Initialize、KAG、Window/Layer/Timer 回调、插件、存档和退出回收；桌面通过后验证 Android ARM64/x86_64。
- [ ] 补齐其余 Unicode 边界（sprintf 的字符/宽度/精度、大小写及文本存取），建立目标 TJS2 与真实游戏语料的持续差分验收。
