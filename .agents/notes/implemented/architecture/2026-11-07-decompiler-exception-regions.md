# 反编译器异常区域状态机首版（try/catch 与重抛）

Status: implemented

## 目标与完成范围

函数内 try/catch（含嵌套、重抛、无变量 catch）经 CFG 降级状态机恢复为
可执行的 `try { … } catch (…) { … }` 文本，语义与原 TAC 一致。实现随
`edbf3ae`（2026-11-05）提交，本批（2026-11-07，darwin）补齐参考对拍验收、
能力边界登记与文档回填。2026-11-08 批次在其上完成结构化 try/catch 首版
（见下文"结构化区域恢复首版"）。

结构化首版后的现状：结构化路径在证明成立时输出真 try/catch（catch 体仅读
异常槽、无绑定槽、同起点嵌套等已证明形态），未证明形态落回状态机平铺输出；
调用/接收者协议、顶层 try/catch 仍拒绝。

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

## 结构化区域恢复首版（2026-11-08，darwin）

- `precomputeTryRegions`（挂 `buildBlocksAndCondZones` 的非状态机分支）：区间
  校验（沿用"CFG 异常区域或 catch 入口越界"文案）、两两真嵌套或不相交、出口
  取体尾跳过 catch 的 Jmp（缺失即拒绝；优化器不折叠不可达 Jmp，bc 路径要求
  catch 目标落在指令边界）、catch 体区域唯一入口证明（区域外块的普通后继不得
  落入 [tryEnd, join)）。`_tryHeads` 为 multimap，`try{try{` 同起点嵌套在发射
  时先认领 tryEnd 最大的最外层（首批实现用单 map 曾静默丢弃外层，嵌套测试
  捕获后修正）。
- `emitRegion` 在 do-while 消费之后、switch/while/if 之前认领 try 头；体区与
  catch 体各自递归发射，继承外层 LoopCtx；体尾 Jmp 目标 = join ≥ 区域末尾按
  贯穿省略。try/catch 结构行不对应任何 TAC 指令（异常表为旁表），标 synthetic。
- 异常槽重绑定：VM 在 unwind 时写入 exValueReg，catch 体首条指令即可读——
  Pass A（scanVersions）在 tryEnd pushDef 并绑定 catch 变量名节点（IRGen 已把
  catch 变量登记为 startPC=tryEnd 的具名局部，无名字时 nextTempName 合成）；
  Pass B（buildNodesAndStmts）重置 `_current` 后需按 Pass A 记录的版本下标恢
  复（首轮只在 Pass A 重绑定，裸 catch/具名槽样本在 Pass B 报"使用未定义寄存
  器"，双游标修复后通过）。无绑定槽输出裸 `catch {`。
- 数据流守卫（2026-11-08 首版保守拒绝；同日第二批改为无名寄存器提升，见下节）：
  1. catch 体读 try 体内定义的寄存器——异常时点版本不明确；例外：读取之前
     catch 体内已有同号寄存器定义（寄存器复用，线性链绑定 catch 局部定义）。
  2. 具名局部声明落点守卫：localVars 的 pc 索引已随 pcAnchors 平移
     （`Optimizer.cpp` 压缩时重映射锚点，`IRGenerator::parseAst` 回填），但
     优化器删除体外死初始化后**幸存定义内移**，`var` 声明落进 try 体，块作用
     域下 catch 与 try 后代码不可见（`catch(e){ r += e; }` 报 Not found
     property 的真实回放失败）。当某具名局部最早的命名定义在体区内、体前无
     同名定义、且体外有同名引用时拒绝。
- 覆盖面（2026-11-08 首版）：bc 路径 `m5_exception_return`/`m5_exception_catch_switch`
  走结构化；自产路径 catch 仅读异常槽/无绑定/嵌套重抛经 Catch2 断言与回放
  验证；其余经状态机回退，9 组语料参考对拍 9/9 一致。
- macOS Debug 全量 CTest **447/447 通过**（新增 4 个结构化 Catch2 用例；
  clang-format 20.1.0、git diff --check 通过）。

## 存储提升协议（2026-11-08 第二批，darwin）

catch 体读取的体定义无名寄存器提升为函数局部合成存储（`_tryStorageNames`，
与 switch 合流存储同机制），首批回退的两个根因按挂点解决：

- **spanClean 脏规则（正确性关键）**：span 内出现对提升寄存器的定义 → 脏，
  先于 `_impureOrStore` 短路检查——挡住 Copy.srcVersion 渲染期实时解析的内联
  穿越存储重定义（寄存器复用下条件节点读到重定义后的值的现象 a）。
- **guardLoopCond 修复（独立既有漏洞）**：字面量判定前 `resolveCopyNode` 下钻
  匿名拷贝链——`for(var i=0;...)` 的 Copy→Int 链此前绕过"绑定字面量"判定渲染
  出 `0 < a0`（现象 b 的条件侧）。提升存储本身是变量语义，跨迭代携带正确，
  无需额外拒绝。
- **noteNamedDef 补 Inc/Dec、NewArray/NewDict**：IncDec 经 StoreLocal 语句形态
  渲染 `name++;` 并改绑新版本（避免后续读取内联出后缀 `name++` 表达式）。
- **提升登记**（precomputeTryRegions）：contested 寄存器（catch 读 ∧ 体定义 ∧
  非 exValueReg ∧ 读取前无 catch 局部重定义）中 `localNameAt` 无名者 → 合成名
  （startPc = 体前最早定义 pc+1，对齐"定义后可见"契约）；已在合成存储
  （switch 合流/其它 handler 提升）中的直接放行；具名局部保持拒绝（声明落点
  未证明）。参数槽附入口拷贝 `var name = a{i};`，并预登记 `_declaredLocals`
  使入口声明成为唯一 `var`。
- **发射**：`emitStructured` 入口发 `var name;` / `var name = a{i};`。
- 覆盖面：bc 语料 **8/9** 走结构化（manifest `structured: true`，CLI 测试对
  .tjsbc 输入断言无 `while (true)`；自产源码路径的具名局部按设计回退）；
  `m5_exception_loop` 因无名循环携带变量 `i`（修复后的 guardLoopCond 正确拒绝）
  保持状态机回退。9 组语料参考对拍复跑 9/9 一致；macOS Debug 全量 CTest
  **447/447**；clang-format 20.1.0、git diff --check 通过。

## 无名循环携带变量提升（2026-11-08 第三批，darwin）

提升机制泛化到循环：成员更名 `_promotedStorage`/`_promotedParamCopies`，抽
`storageReady`/`promoteRegister` 共用 helper；`precomputeLoopStorage` 扫描已
识别 while/do 循环条件区，条件读取 ∧ 体内（含 for 更新区与锁存）重定义 ∧
无名绑定 → 提升为合成存储。条件渲染变量名保住迭代语义，自增经 IncDec 语句
形态渲染，跨迭代读取即变量当前值；`guardLoopCond` 对提升寄存器经 LocalName
早退自然放行。顶层单元（无函数作用域）不提升，"循环携带变量"拒绝仅保留在
顶层路径（既有两个顶层拒绝测试保持通过）。

- 覆盖面：异常语料 **9/9 全部结构化**（`m5_exception_loop` 翻转成功，输出
  `while (v2 < a0)` + 更新区 `v2++;` + catch 内 continue）。9 组语料参考对拍
  复跑 9/9 一致；macOS Debug 全量 CTest **447/447**；clang-format 20.1.0、
  git diff --check 通过。

## 状态机调用族与 join 写入侧合流（2026-11-08 第四批，darwin）

- 状态机白名单扩展到调用族与容器/成员/下标/删除/转换语句族及 Load 的
  Object/FuncMeta 常量（tile 已存在，状态机按 pc 物化机器变量即正确）；
  Call 实参 Push 跨块防御性拒绝。接收者内联 `memberCalleeInlineOk`（唯一使用
  = 同块调用 callee + 基址未重定义）渲染 `o.m(...)`，旁路须挂在
  buildInstruction 的 GPropC/GMethodC case（兜底处已晚）。函数字面量每次 Load
  独立创建；状态机无装配宿主时如实拒绝。
- try 写入侧合流提升：体/catch 区定义、join 后读取的寄存器提升为合成存储
  （no-throw/throw 两路径在汇合点的值不同，线性链绑定最后定义丢另一条路径）。
- 新增语料 `m5_exception_call_in_try`（bc 结构化 + 调用族）；异常语料 10/10
  结构化。全量 CTest **448/448**；参考对拍 10/10 一致。

## 后续边界

- 具名局部的声明落点协议：需 IRGen 侧把声明与幸存定义解耦（如把 `var` 落点
  锚定到 localVars 语义位置），具名局部被 catch 读取的形态才能结构化。
- 状态机调用/接收者、闭包及对象常量协议扩展照旧独立推进。
- 顶层 try/catch（无函数局部作用域）维持 M5 拒绝；`finally` 在文法、TAC、
  字节码与参考引擎中均不存在，不在范围内。
