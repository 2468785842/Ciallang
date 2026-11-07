# 反编译器异常区域状态机首版（try/catch 与重抛）

Status: implemented

## 目标与完成范围

函数内 try/catch（含嵌套、重抛、无变量 catch）经 CFG 降级状态机恢复为
可执行的 `try { … } catch (…) { … }` 文本，语义与原 TAC 一致。实现随
`edbf3ae`（2026-11-05）提交，本批（2026-11-07，darwin）补齐参考对拍验收、
能力边界登记与文档回填。

这不代表结构化 try/catch 已完成：结构化路径仍对 handler 表整单拒绝
（`try/catch 结构化属 M5（throw handler 表非空）`，`ExprBuilder.cpp` precheck），
所有异常函数实际走状态机平铺输出。结构化区域恢复是下一批；调用/接收者
协议、顶层 try/catch 仍拒绝。

## 实现与不变量

- 结构化路径首查 handler 表即拒（保留 M5 批次原因）；`recoverUnitBody` 回退链
  不变，状态机接管后逐 pc `_chunk.findThrowHandler(pc)` 包 `try {`，条件求值
  固定在原比较/Test 指令 pc——移到跳转处会使异常的 try 区域归属、部分寄存器
  写入和副作用漂移。
- handler 校验：`tryStart >= tryEnd || tryEnd >= code.size()` 拒绝（"CFG 异常区域
  或 catch 入口越界"）；`exValueReg` 计入寄存器上限；`exValueRegNone`（0xFFFF）
  表示无绑定槽，寄存器 0 是合法槽位。
- catch 内把异常值写入 `_machineNames[exValueReg]`、清空异常临时、
  `state = tryEnd; continue;`；重抛即 catch 内后续 `Throw`（普通语句渲染）。
- VM 契约：`Throw` 指令必须落在其 try 区间内（unwind 按故障指令 pc 线性查表，
  `VMState.cpp` 主循环）；异常表"内层优先"登记，查表取首个匹配。
- 配套修复（同 `edbf3ae`）：
  - `Optimizer.cpp`：catch 入口块（`handler.tryEnd == block.start`）不继承前驱
    拷贝事实——异常可在前驱块中途发生，异常值槽由 VM 隐式写入。
  - `TjsTranslator.cpp`：参考 ENTRY 登记改内层优先（反向遍历 entryFixups），
    与 VM 取首个匹配一致。
  - `tests/tjs/exceptions/register-state-after-getter.tjs`：catch 读到的是抛出点
    寄存器状态（getter 异常边界），不继承正常出口拷贝事实。

## 验证与证据

- Catch2（`tests/decompile_test.cpp`）：异常槽为 0 与无槽两种寄存器状态保真
  （含 catch 入口越界拒绝断言）；重抛跨函数边界保持原异常值；捕获不透明
  对象身份一致。
- 固定语料 9 组（`tests/test_files/tjsbc/decompile/`，manifest 哈希核对）：
  - `edbf3ae` 5 组（Windows `40a736c` 导出）：nested（双层 + 重抛）、empty
    （无绑定）、return（双路返回）、getter（异常来自 property getter）、
    loop（for 体 + catch 内 continue）。
  - 本批新增 4 组（darwin `d8044c3` 导出，样本标 `reference: darwin`）：
    try_switch（try 包 switch + case 内 throw）、catch_switch（catch 内 switch）、
    nested_three（三层嵌套 + 条件重抛）、catch_reads_try_local（catch 读 try
    体内定义的寄存器）。
- darwin 参考对拍（`out/m5-exception/reference-check.py` / `reference-report.json`）：
  **9/9** manifest 哈希、三路 stdout（源码 / 转译执行 / 恢复源码执行）、恢复
  回放逐字节一致；参考原源码 vs 字节码恢复源码的 `Debug.message` 标记序列
  9/9 一致、参考进程退出 0。参考二进制 `bin/darwin/TJSCompiler` SHA-256
  `478eab17…b4471b1` 与 manifest `referenceDarwin` 一致。
  println 包装为 `Debug.message`，剔除时间戳；参考原字节码直接加载环仍缺
  入口，不宣称四环全部通过。
- 边界探测（`out/m5-exception/probe_*.tjs`）：try 体内含调用诚实拒绝（复合
  原因"try/catch 结构化属 M5…；CFG 降级：CFG 状态机尚不支持该指令的生命
  周期/接收者协议"）；try+while+break/continue 穿越、catch 内 switch、三层
  嵌套、catch 读 try 体局部均经状态机成功回放。try_while 探针未固化语料。
- macOS Debug 全量 CTest **443/443 通过**（17.93s；`ctest -N` 注册 443 项）。
  `test_exception_regions_roundtrip` 已补 manifest 哈希校验（对齐 switch 语料
  测试），断言保持形态无关（仅 `catch (`）。

## 首轮失败与修正

本轮基线 CiallangSwitch 33 个子失败：`decompile/` 语料全部源码哈希与 manifest
不符。根因是平台行尾依赖——manifest 源码哈希在 Windows 检出（CRLF）内容上
计算，git blob 为 LF，而 `.gitattributes` 只对 `debug/` 语料钉了 `eol=lf`，漏了
`decompile/`；另发现 `m5_switch_single_default.tjs` 的 manifest 哈希与任何行尾
变体均不符（源码后改过、哈希未刷新；磁盘源/字节码对经三路执行验证一致）。
修复：`.gitattributes` 补 `decompile/*.tjs text eol=lf` 与 `*.tjsbc -text`，
manifest 42 组 sourceSha256 按实际 LF 字节重生成（字节码哈希全部本就正确）。
**Windows 侧拉取本批后必须重检出该目录**（删除 `tests/test_files/tjsbc/decompile/`
后 `git checkout`，或等效方式），否则旧 CRLF 工作区文件仍与 manifest 不符。

## 后续边界

- 结构化 try/catch 区域恢复（emitRegion 认领 try 区域、输出真 try/catch 结构）
  是下一个实现批次；catch 体读 try 体重定义寄存器等数据流形态需守卫。
- 状态机调用/接收者、闭包及对象常量协议扩展照旧独立推进。
- 顶层 try/catch（无函数局部作用域）维持 M5 拒绝；`finally` 在文法、TAC、
  字节码与参考引擎中均不存在，不在范围内。
