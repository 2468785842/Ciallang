# 待办

只记录尚未完成的工作；完成后删除条目。使用限制见 [FAQ](doc/faq.md)，历史测量见[性能记录](doc/performance.md)。

## 字节码与语言兼容

- [ ] 修复 CHKINS 转译的 target/name 顺序及结果写回。最小函数 `x instanceof "Integer"` 的手工字节码以 21 调用仍返回 21，预期为 1（`src/tjsbc/TjsTranslator.cpp` decode / OP_CHKINS）。
- [ ] 对齐 `(const)` 容器的编译期构造、元素限制及同一字面量跨调用共享；当前 Parser 忽略标记并在运行期新建，见 [语法说明](doc/syntax.md#8-内联字面量array--dictionary--常量折叠形)。

## 性能与优化

- [ ] 继续缩小与参考 TJS2 的原生调用性能差距，优先评估调用帧及实参窗口成本；使用同机 Release 交替对照记录收益与回退。
- [ ] 定位原生绑定重构后整数循环与字符串拼接约 2.4%–3.4% 的回退，区分布局变化与派发成本。基线与数据见[绑定层对照](doc/performance.md#2026-10-30-原生绑定层重构对照)。
- [ ] 评估 SSA 执行 IR、跨块汇合点传播和循环体变换；保留外部结果寄存器、异常边与行号映射语义（`src/gen/Optimizer.*`、`SSA.*`）。

## 编辑器集成

- [ ] 在目标 IDE 中验证补全、跳转与重命名交互；当前自动化覆盖 stdio 协议，尚未提供 JetBrains 专用插件。边界见 [LSP 文档](doc/lsp.md)。
