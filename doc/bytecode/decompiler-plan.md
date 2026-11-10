# 字节码反编译器方案（TAC 层直译路径）

状态：设计已采纳（2026-11-01 修订）。目标是从自产编译产物和 TJS2 字节码恢复可读、
可重新编译的 TJS2 源码；不要求字符级还原，行为保真优先于接近原始写法。本版把原
"可组合全量管线"提案收敛为 TAC 层直译路径：反编译核心建在自有 TAC 上，不直连 TJS2
字节码；原方案的契约、来源表与验证闭环要求继续有效（§3、§4、§7），可插拔
adapter/pass/backend 降级为后续演化方向。

## 1. 架构：一个反编译核心，两条入口

```text
TJS2 .tjsbc → TjsBytecodeReader → TjsTranslator ─┐ 壳: TjsTranslatedProgram（context 树/
                                                 │   签名/ClassMeta/PropMeta/nameAtom 透传）
                                                 ├→ TAC 反编译核心 → {文本, 行表} → TJS2 源码
.tjs → Parser → IRGenerator(+Optimizer) → Chunk ─┘ 体: Chunk TAC（函数内容唯一来源）
```

壳/体分离是硬边界：ctClass 在加载期解释、不生成 TAC（`mapping.md` §7），类结构
（成员、构造入口、默认值、extends）只存在于加载器元数据；函数内容只从 TAC 还原。

选择 TAC 层而非直连 TJS2 字节码的理由：

| # | 理由 | 依据 |
|---|---|---|
| 1 | TJS2 的 flag 线程在转译层收敛为两个规则形态：相邻比较+跳转融合写 `_zf`，非相邻物化为 `%F` 布尔寄存器——条件还原无任意特例 | `TjsTranslator.hpp` M3 支持集、`mapping.md` §3 |
| 2 | TJS2 128 条变长指令（四联属性变体、CALLD 变长）已被转译为 97 条内部指令（`Instruction.hpp`），变长仅 CallV 族实参表 | `TjsTranslator.hpp`（128 操作码覆盖闭环） |
| 3 | 融合指令（`obj.x += y` 展开为 GProp+Add+DProp 等）的逆映射是对自己确定性模板的查表；tjsbc 路径 Optimizer 未接线，TAC 即模板原样 | `src/tjsbc/` 无 optimize 引用 |
| 4 | 转译器是加载功能必经之路，共用件摊薄 | `--tjsbc` 加载链 |
| 5 | 变量名/行号在自控格式中可保留（自产路径）；TJS2 直连无解 | `FuncMeta::localVars`、`TjsContext::sourcePos` |

## 2. 事实核对表（2026-11-01 复核）

| 论断 | 仓库实际与设计影响 |
|---|---|
| "flag 全部物化为普通寄存器，无特例" | 弱化成立：相邻对融合写 `_zf`（CompareTest/Cmp* + JmpE/JmpNE），非相邻物化 `%F`。反编译按这两个规则 tile 处理即可，但分析必须把 `_zf` 当显式定义/使用 |
| "75 条均匀 3-op" | 过时：现为 97 条（e4a357f 起 +BinaryImm/CompareTestImm），CallV 族变长实参。以 `Instruction.hpp`/`Bytecode.hpp` 枚举为准 |
| "Optimizer 未接线" | 分路径：tjsbc 转译器未接线（TAC=模板原样）；自产路径 `optimizeDefaultChunk` 常规接线。**输入契约定为"后优化 TAC"**——模式表按后优化形态建（LoadImm 已被立即数融合吸收、比较+Test 为 CompareTest 族、拷贝已传播），tjsbc 模板形态是其真子集 |
| "转译器待 M1" | 已闭环：128 操作码、扫描发射、跳转回填、ctClass 装配（`TjsTranslator.cpp`） |
| "每个 Chunk 补 Rlc 即可" | `Chunk::toBytecode()` 会**原地**把 Rlc 的 TAC 下标换成字节码偏移。行表须在 TAC 层快照（反编译先于执行/编码），或独立坐标空间，不得复用会变义的数字 |
| "sourcePos 可直接当行号" | `TjsContext::sourcePos` 是无源码文本下的位置编码，仅在原源码可得且可解析时才作为"原行号"输出字段；否则只标 TJS2 码字区间，不伪造行号 |
| "SSA 可全量复用" | `SSA.hpp` 拒绝 CallOmit/CallExpand/CollapseArgs，未建模 `_zf` 与属性内存。结构化 pass 需自建保守数据流或先扩展 SSA 访问描述 |
| "+= 逆融合即修复属性行为" | 逆融合只恢复**写法**；不能证明转译执行与 TJS2 一次 `Operation()` 等价（`mapping.md` §6/§8.1 近似清单），两契约须分别报告（§3）。误融合（把非模板来源的 GProp+Add+DProp 合成 `+=`）的守卫复用 `Optimizer.cpp` 的 `computeBlockLiveness`：验证中间寄存器无其他读者 |
| 局部变量名恢复 | TJS2 导出格式**无局部名表**（参考 `tjsInterCodeGen.cpp:3639` ExportByteCode 记录：parent/name/type/帧尺寸/签名/property/成员表/行号，无名字表）。bc 路径局部名只能合成（`v3`/`l7`）；自产路径 `FuncMeta::localVars` 全量可得。此为格式边界，非实现缺口 |
| "localVars.startPC 可直接按 pc 查名" | 过时：IRGen 的零拷贝 `var` 在表达式发射后取声明标签（startPC = 定义 pc + 1），且优化器压缩曾不重映射它。M2 起经 `Chunk::pcAnchors` 随压缩重映射（`IRGenerator::parseAst`），查名用"本指令执行后"的 pc |
| "一元负号优先级高于乘法" | 已对齐（2026-11-02）：解析器改按 tjs.y unary_expr 同级，`-a * 2` = `(-a) * 2`（TAC 先 ChgSign 后 Mul），反编译不再补括号；数值转换同步并入前缀层（`int o.b` = `int (o.b)`，此前报类型错误） |

## 3. 行为一致的契约

分别验收两件事，分别报告；第一项通过不能掩盖第二项的转译近似：

1. 输入字节码与生成源码在同一参考语义、同一宿主环境中的可观察行为一致。
2. TJS2 输入经转译在 Ciallang 中执行，与参考引擎执行一致（转译近似见 §8.1 清单）。

可观察项：返回值及类型、输出、全局/对象状态、成员访问与调用的次数和顺序、this 绑定、
异常值和捕获范围、实参省略/展开、对象身份与共享、有脚本可见终结行为的生命周期。GC
内部收集时刻不要求相同，但新增临时值导致可见 finalize 变化时不得忽略。

严格模式结果状态为 `Success` / `Unsupported` / `InvalidInput` / `InternalError`，逐样本记录。
`Unsupported` 给出 context、来源位置和原因，不生成可误当成完整程序的成功文件；反汇编或
带缺口的阅读稿另设显式诊断模式，不得以 TODO 注释/空函数/运行期报错占位符冒充完成。

不可恢复边界（提前声明）：eval/动态源码可能按名字访问局部，bc 路径局部名不可得时
严格模式对无法证明安全的动态求值拒绝输出；异常堆栈/文件名/行号可被脚本观察，源码重排
天然改变这些值，要求一致时须有宿主位置映射；插入状态变量/临时变量影响动态名字查找、
with、eval 和局部值保活，"生成名字不冲突"不足以保证安全。

## 4. 数据模型与来源表

**产物从第一天起就是 `{文本, 行表}`**（硬约束，M0 起）：

```text
输出文本行（或 UTF-8 字节范围）
  ↔ TAC 指令半开区间 [tacStart, tacEnd)     ← 主坐标，TAC 层快照
  ↔ 可选 TJS2 context + 码字半开区间         ← bc 路径（Pass 1 映射）
  ↔ 可选原源码行号                           ← 仅原源码可得且可解析时
```

- 行表在**编码前**（`toBytecode()` 之前）快照 TAC 下标；缩进、括号、生成的合成声明标为
  synthetic；空行可无来源。转换可多对多，不得只取 min/max 混入无关指令。
- 壳 = `TjsTranslatedProgram` 透传（`TjsTranslator.hpp`：parent/type/arity/ClassMeta/
  PropMeta/FuncMeta.nameAtom），自产路径壳从 AST 元数据（`FuncMeta`、类定义节点）取。
- 体 = `Chunk` TAC。若适配期持有 Chunk/元数据，用符合 `Runtime.hpp` 的根保护覆盖整个
  适配/分析期；ProgramIR 优先拥有字符串/常量描述与稳定索引，不长期借用 Atom 编号或裸
  GC 指针。
- bc 路径 M1 起：转译器为每个转译 Chunk 发 Rlc 行号条目（Pass 1 已建 codePos→TAC 的
  映射，接线即可；注意 §2 的变义警告）。

## 5. 分析与恢复流程

1. **校验与归一化**：常量/context 引用、跳转边界、寄存器范围、实参窗口与异常区域；
   Push/Call 与 CallV 统一为有序调用节点，保留省略/展开种类、接收者与丢弃结果。
2. **tile 表达式恢复（后优化形态表）**：以模板查表为主——转译模板是确定性的（§1 理由
   3）；优化器改写形态（BinaryImm/CompareTestImm/拷贝传播/死加载消除）逐条给出逆映射。
   只有证明单次求值、无额外异常/别名影响才内联临时值；getter、动态转换、调用、分配及
   `Load` 实例化不可按纯表达式处理。
3. **CFG 与数据流**：复用 `ControlFlowGraph` 正常边，补异常边；支配/后支配/循环/活跃性。
   异常发生时寄存器可能处于指令中间状态，不能仅把块末值送进 catch。
4. **结构化**：if/else、循环、break/continue、短路/三元（跨块折叠需支配/后支配与副作用
   条件）、switch 比较链（证明顺序与贯穿语义等价才输出）、try/catch 嵌套与重抛。
   函数内受支持标量指令的不可约 CFG 降为基本块状态机；其它协议仍返回 `Unsupported`。不输出 TJS 不支持的 goto，不嵌字节码解释器。
5. **程序组装**：壳（context 树→嵌套函数/类/property/extends/字段初始化顺序）+ 体。
   保留声明安装顺序、共享函数身份与求值时机；不能把每次 `Load` 创建的闭包提升为共享函数。
6. **属性读改写**：`GProp+Add+DProp` 逆融合为 `+=` 需双守卫——来源确认是转译模板（§1
   理由 3 的查表命中），且 `computeBlockLiveness` 证明中间寄存器无其他读者。自产源码
   本来就是显式读写时不强制合并；`obj()[key()] += rhs()`、前/后置自增、getter/setter
   修改对象、异常中断分别验收。
7. **打印**：独立 AST 层处理优先级、括号、转义、保留字；整数边界、负零、NaN/Infinity、
   Octet、共享/常量容器逐类验证，不靠数值默认格式猜测。

## 6. 实施批次

| 批次 | 内容 | 完成条件 |
|---|---|---|
| M0 | `DecompiledOutput{文本,行表}` 类型、CLI `--decompile` 骨架、反汇编级 Printer | {文本,行表} 管道通；环①以反汇编级跑通；错误不留成功文件 |
| M1 | 转译器发 Rlc（Pass 1 接线）；固定参考二进制/版本/样本清单（哈希与导出选项） | bc 路径反汇编级端到端；行表含码字区间；未知版本被拒 |
| M2 | ExprBuilder：后优化形态模式表（含 BinaryImm/CompareTestImm 逆映射）、调用族/成员访问族 | 表达式级 round-trip 逐形态对拍；**已完成（2026-11-02，`ExprBuilder.*`）** |
| M3 | 语句结构化：if/while/for 归约、短路/三元跨块折叠 | 路径副作用、求值次数、break/continue 对拍通过；**已完成（2026-11-03，ExprBuilder 结构化发射）** |
| M4 | 壳组装（context 树→声明）+ `+=` 逆融合（双守卫） | 类/继承/property/初始化顺序用例；getter 计数不翻倍。**已完成（2026-11-04，`ExprBuilder.*`/`Decompiler.cpp` 壳组装）：顶层与体内 function/class/property 声明 round-trip 通过；`+=`/`++` 逆融合覆盖成员/下标/具名局部/全局/`*p` 目标（单读者 + 对象一致性双守卫，匿名拷贝按源头解析）；实参位置匿名函数字面量恢复；类成员注册按源码位置排序（枚举序对齐 TJS2）；property 安装名登记 `_declaredGlobals`。验收扩展项（super 构造 prologue、`CallMethodV`/`testDeref` 形态）移交 M5** |
| M5 | 硬核兜底：任意 CFG 降级、switch、嵌套异常及覆盖扩展 | **已完成（2026-11-10 关闭评审）**。switch/循环/异常区域/数据流提升/调用族/声明落点/**with/eval 恢复**全部验收；模糊覆盖落地且 KNOWN_FAILURES 清零；能力矩阵与 Unsupported 清单已量化（见下文）；闭包捕获经复核为**两引擎一致的对齐特性**（自由名 this-proxy 降级，非缺口）；CallMethodV 行表经 Windows/WSL Linux 双侧复核收敛，移交项全部闭环。关闭证据：Windows Release CTest **248/248**、WSL Linux Debug CTest **449/449**、CI 八 job 全绿。当前状态见下节，历史证据见 M5 note。 |

M0/M1 后以真实产物的语义缺口重新估算工期；属性协议、异常、动态作用域是主要不确定项。

### M5 当前状态（2026-11-05）

| 项目 | 已验收能力 | 剩余工作 |
|---|---|---|
| switch | 常量/多值标签、贯穿、break/default、判别值单次求值（含调用判别值）；自产双层/三层嵌套及直接返回；参考函数内双层嵌套、贯穿、返回线程化与三层合流；调用标签表达式（`case f():`，首组/后续组/多组，自产与参考模板） | 更多合流形态；参考更复杂副作用标签形态 |
| 循环组合 | 自产/参考 while、do、for 与 switch 的 break/continue；case/default 内循环；for 更新段调用计数 | 更多数据流形态；顶层无名寄存器携带的 for 初值仍拒绝 |
| 控制流与异常 | 结构化优先；函数内受支持 TAC 的任意 CFG（含不可约）可降为状态机（调用族、容器/成员/下标语句、字符串与函数字面量已入白名单，接收者内联渲染 `o.m(...)`），寄存器逐次赋值，合成行不伪造来源；函数内 try/catch 结构化恢复（嵌套/重抛/无绑定/同起点嵌套），体/catch 定义寄存器的 catch 读取与 join 后读取提升为合成存储（含无名循环携带），10 组语料全部结构化且参考对拍一致 | 具名局部被 catch 读取的声明落点协议；函数字面量在状态机中需装配宿主；闭包及对象常量中仍拒的协议形态（ChgThis/TestVoid/With/EvalStr/CallExpand/CollapseArgs/Super） |
| 动态名字 | 避让已有参数、局部与 Atom 名；合成存储仅限函数，声明提升、赋值留原路径 | with/eval 名字依赖与不能恢复的边界裁定 |
| M4 移交 | s7_ext outputdebug 读取已修复；实参及赋值右值匿名函数字面量的自产恢复已验证；super 构造 prologue 自产路径已验收（bc 侧被参考编译器段错误阻断，登记绕开）；testDeref 与 super/CallMethodV 形态严格恢复 + 回放一致（2026-11-10 复核）；对象成员赋值 NEW/this 已修复（2026-11-08） | —（移交项全部闭环；debug 语料 pendingUnits 登记已按现状刷新） |
| 最终验收 | 37 组成功 switch 字节码三路执行一致；新返回/三层及 ExprFunction 针对性参考对拍通过，扩展参考运行有间歇失败，见 note；拒绝样本固定登记；能力矩阵与 Unsupported 清单已量化，模糊覆盖落地（KNOWN_FAILURES 清零） | 参考原字节码直接加载环缺入口（环③按 §7 补偿控制替代） |

实现见 `src/decompile/ExprBuilder.cpp`；回归按接口/表达式与控制流分在
`tests/cli/test_decompile.py`、`tests/cli/test_switch.py`，运行入口见
[测试导航](../../tests/README.md)。固定输入与参考身份见
`tests/test_files/tjsbc/decompile/manifest.json`。M5 已完成（2026-11-10 关闭评审）；
各批次命令和失败证据见 [M5 note](../../.agents/notes/implemented/architecture/2026-11-04-decompiler-m5.md)。

### CFG 降级首版（2026-11-05）

结构化路径失败时，仅在函数局部作用域及受支持指令白名单内启用状态机。
每个物理寄存器对应一个局部槽，按原基本块执行顺序更新；参数转存后清除入口
副本，生成名字避开已有名字。该路径不做跨块内联、死加载删除或逆融合。
框架、声明与参数清除在行表中标记 synthetic，保留实际计算/跳转的来源。

当前支持 51 类 TAC，Load 仅限 None/Real/Atom。顶层、with/eval、
变长参数、调用/接收者、函数/类/property/容器对象常量等仍严格拒绝。
详细白名单、边界与完整证据见 [CFG 降级实现记录](../../.agents/notes/implemented/architecture/2026-11-05-decompiler-cfg-fallback.md)。
验收包含不可约双入口循环、32 组有界输入、全局写入、名称冲突、对象实参回收、
合成行及区间检查；4 份实际恢复函数在参考引擎中与独立逻辑程序输出一致。

### 异常区域首版（2026-11-05 实现，2026-11-07 darwin 验收）

结构化路径遇 handler 表非空仍拒绝；状态机路径逐 pc 包 `try {`，条件求值固定在
原比较/Test 指令 pc，catch 内把异常值写入对应寄存器槽后跳到 `tryEnd` 续跑；
重抛即 catch 内普通 `Throw` 语句。handler 校验拒绝区间越界；`exValueRegNone`
表示无绑定槽（寄存器 0 是合法槽位）。配套：Optimizer 的 catch 入口块不继承
前驱拷贝事实（异常可中途发生）；转译器 ENTRY 内层优先登记（VM 查表取首个
匹配）。详见[异常区域实现记录](../../.agents/notes/implemented/architecture/2026-11-07-decompiler-exception-regions.md)。

验收：9 组固定语料（5 组 Windows 导出 + 4 组 darwin 导出，覆盖嵌套/重抛/
无绑定/getter 异常/循环 continue/try 包 switch/catch 内 switch/三层嵌套/
catch 读 try 体局部）三路 stdout 与参考标记序列逐字节一致；try 体内含调用
诚实拒绝；顶层 try/catch 维持 M5 拒绝。语料哈希已固定为 LF（`.gitattributes`），
manifest 在 2026-11-07 按实际字节重生成。

### 结构化 try/catch 首版（2026-11-08，darwin）

`precomputeTryRegions` 从 handler 表认领 try 区域（区间校验、两两真嵌套或不相
交、体尾跳过 catch 的 Jmp 推导出口、catch 体区域唯一入口证明），`emitRegion`
在 do-while 之后优先认领 try 头，体区与 catch 体各自作为封闭区域递归发射；
`_tryHeads` 为 multimap，`try{try{` 同起点嵌套先认领最外层。异常槽在 Pass A/B
双游标重绑定为 catch 变量名节点（VM 在 unwind 时写入，方案 §5.3）；无绑定槽
输出裸 `catch`。数据流守卫保守拒绝：catch 体读 try 体内定义的寄存器（异常时
点版本不明确；读取前已有 catch 局部定义的寄存器复用除外）、具名局部声明落点
随幸存定义内移进 try 体且体外有引用（localVars 的 pc 索引随 pcAnchors 平移，
但死初始化删除后声明落点语义不保，块作用域下 catch 不可见）。被拒形态自动落
回状态机，语义不回退。

### catch 数据流存储提升（2026-11-08 第二批，darwin）

catch 体读取的体定义无名寄存器提升为函数局部合成存储（与 switch 合流存储同
机制）：`spanClean` 把跨越提升存储重定义的内联判脏（正确性关键，挡住
Copy.srcVersion 渲染期解析穿越重定义）；`guardLoopCond` 字面量判定前下钻匿名
拷贝链（修复 `0 < a0` 类无名循环携带逃逸的独立漏洞）；`noteNamedDef` 补
Inc/Dec、NewArray/NewDict 路径（IncDec 经语句形态渲染 `name++;`）；参数槽以
入口拷贝初始化。具名局部被 catch 读取仍拒绝（声明落点未证明）。

### 无名循环携带变量提升（2026-11-08 第三批，darwin）

提升机制扩展到循环：`precomputeLoopStorage` 扫描已识别 while/do 循环的条件区，
条件读取、体内（含 for 更新区）重定义、无名绑定的寄存器提升为合成存储——条
件渲染变量名保住迭代语义，自增经 IncDec 语句形态渲染，跨迭代读取即变量当前
值。顶层单元（无函数作用域）不提升，"循环携带变量"拒绝仅保留在顶层路径。
异常语料 9/9 全部结构化。

### 调用标签表达式（2026-11-08 第四批，darwin）

`case f():` 恢复——判别值调用与首组调用标签此前已支持；本批放宽两条约束为
"标签定义链"证明（每条前缀指令的结果被本组比较链消费、不外溢）：

- 自产链归约（`precomputeSwitches`）：分组间前缀从"仅常量装入"放宽为
  `isLabelDefChain`；`scanDispatch` 组间材料先纳入、扫描结束统一校验（尾部
  未被比较闭合的材料保持仅常量的旧行为，链上下一分组的标签常量经版本链渲染）。
- 参考模板（`precomputeReferenceSwitches`）：块尾 `[?, CmpEQ, JmpNE]` 的标签
  值判定从"常量装入"放宽为"定义比较右值"；组间前缀同用 `isLabelDefChain`。
- 新增固定语料 `m5_switch_calllabel_{first,later,discriminant,multigroup}`
  （darwin 导出，manifest `bytecodeChecks` 断言 `case lbl()` / `switch (d(`）。
  默认体在无 break 模板下落为 switch 后直线语句（语义等价）。

### 状态机调用族与 try 写入侧合流（2026-11-08 第五批，darwin）

- 状态机白名单扩展：调用族（Push/Call/CallV/CallMethodV/CallOmit）、成员与
  容器语句族（GProp 族、NewArray/NewDict、GetIndex/SetIndex、DProp 族、
  DelProp 族、DerefGet/DerefSet、ToString/Inv/Debugger）、Load 的 Object 与
  FuncMeta 常量。Pass B 的 tile 构建与 Push/Call 线性配对与块无关，状态机按
  pc 重放机器变量；Call 实参 Push 与 Call 跨块时防御性拒绝。函数字面量每次
  Load 独立创建（不提升为共享函数）；状态机路径无装配宿主时函数字面量如实
  拒绝。仍拒：ChgThis/TestVoid/With/EvalStr/CallExpand/CollapseArgs/Super。
- 接收者内联（`memberCalleeInlineOk`）：GMethodC/GPropC 的 Member 节点唯一
  使用是同块调用的 callee 且基址稳定时，内联渲染 `o.m(...)`（getter 求值时机
  与异常归属随块保持）；跨块或基址重定义回落逐 pc 物化。
- try 写入侧合流提升：体/catch 区定义、join 之后读取的寄存器提升为合成存储
  （no-throw 路径携带体区值、throw 路径携带 catch 区值，线性链绑定到最后定
  义会丢另一条路径的值）。新增语料 `m5_exception_call_in_try`（bc 结构化 +
  调用族）；自产同名形态经状态机回退验收（声明落点守卫 + 状态机调用族）。
  异常语料 10/10 结构化。

### 声明落点提升与能力矩阵（2026-11-09，darwin）

声明落点提升：优化器删除体外死初始化后具名局部的幸存定义会内移（进循环体/
try 体），`var` 声明随之落进内层块、外层读取不可见。每个 (reg,name) 作用域
的声明在 declareLocal 登记；发射期**同名单作用域**名字的全部 StoreLocal 降为
普通赋值（初值留原 pc）、入口统一 `var name;` 承载声明。同名多作用域（遮蔽）
保留原位 declare——提升会合并两个运行时槽。模糊 KNOWN_FAILURES 清零
（119/119）。配套 VM 修复：`loadGlobal` 的 Property getter 快路径仅当
property owner 是全局对象时触发（`&o.x` 引用存入全局 var 不再被劫持，
[调查记录](../../.agents/notes/implemented/bug-fix/2026-11-09-amp-property-reference.md)）。

**能力矩阵与 Unsupported 清单（2026-11-09 量化）**：

| 维度 | 已验收 | 诚实拒绝（原因串前缀） |
|---|---|---|
| 表达式 | 字面量/算术/位/比较/逻辑/一元/成员/下标/调用/容器字面量/delete/物化/逆融合 | — |
| 语句 | if/while/do/for/break/continue/switch（含贯穿/多值/调用标签）/**with（嵌套/裸词链）**/try-catch（嵌套/重抛/无绑定）/**后缀 eval** | 顶层 try（"try/catch 顶层结构化属 M5"）；with 体 try 且 catch 读体定义具名局部（声明落点守卫，放宽尝试回归 21 个模糊程序已回退）；状态机平铺 with 栈（"with 语句状态机无法平铺"） |
| 声明 | function/class（继承/构造器 prologue）/property/var；赋值右值与实参 FuncLit | 具名函数表达式（"具名函数表达式恢复属后续批次"） |
| 数据流 | SSA 版本链、单次求值内联、多使用物化、合成存储提升（catch/循环/join）、声明落点提升 | 同名多作用域遮蔽（原位声明，非拒绝） |
| CFG | 结构化归约 + 任意 CFG（含不可约）状态机降级 | ChgThis/TestVoid/With/EvalStr/CallExpand/CollapseArgs/Super/"预检未覆盖的指令" |
| 顶层 | 直线表达式、声明、裸语句 | 需合成全局名的形态（"顶层无局部作用域可物化"/"顶层复制会改变可观察行为"）；循环携带变量（"循环条件读取的寄存器在体内重定义"） |
| 协议 | 闭包字面量创建（每次 Load 独立）；eval 名字依赖（恒全局+with 栈，verbatim 安全） | 闭包捕获——两引擎一致不支持（对齐特性，见 compatibility.md），反编译无此形态 |

量化锚点：61 组固定语料（manifest `expectedRecovery`/`rejectReason` 断言）+
CiallangFuzz 120 程序（KNOWN_FAILURES 空）+ CLI/Catch2 拒绝原因断言。
四环状态：环①②④ 全覆盖；环③（参考字节码直接加载）缺参考引擎入口。

### CallMethodV 行表平台收敛复核（2026-11-10，Windows + WSL Linux）

M4 记录的"Linux CTest 全绿 vs Windows s7_ext 行表失败"冲突已由读取端修复闭环
（导出器字节数 count 垃圾条目 + 平局不稳定排序，见 M5 note 2026-11-04 批）；
本批复核确认无残留平台漂移。WSL Ubuntu（Linux Debug，clang 21）与 Windows
（Release）对 `debug/` outputdebug 语料逐字节对比：

- s7_ext（super.<方法>() 即 CallMethodV 形态）与 s6_class（testDeref 体内方法
  直调 + `*p` 形态）的 `--map` 行表 JSON **逐字节一致**，反编译文本一致（仅行
  尾）；行表值符合 manifest `expectedLines`（s7 describe 合并 [4, 8]、s6
  testDeref [15..22]）。
- 复核同时确认 M4 移交形态已在严格模式全部恢复：4 组 debug 语料 `--decompile`
  均 success（rc 0），恢复源码执行输出与原源码一致（s7 `"dog:legs=4"`、s6 五行
  输出），Windows/WSL 双侧一致。manifest 的 stale `pendingUnits` 登记随之刷新；
  `test_decompile_outputdebug_source_lines` 补严格模式 + 回放对拍断言。
- `--view` 诊断视图的 map status 按设计恒为 unsupported（诊断产物不可重编译，
  `Decompiler.cpp` 无条件记 diagnostic），不反映样本恢复状态。

## 7. 验证闭环

沿用 `tests/tjs/` 脚本断言与 CTest；内部图/来源/GC 所有权用 Catch2 并在
`tests/CMakeLists.txt` 显式登记；runner 参考 `bench/bytecode_conformance.py` 的路径探测、
哈希、超时与无损输出记录。

| 环 | 比较对象 | 发现的问题 |
|---|---|---|
| 自产 round-trip | 源码在 Ciallang 执行 vs 反编译再编译执行 | 内部分析、打印与后优化形态恢复错误 |
| TJS2 源码对拍 | 同一 TJSCompiler 中原源码 vs 其字节码的反编译源码 | 原操作恢复与目标语法问题 |
| 字节码真实性 | 参考引擎加载原字节码 vs 运行反编译源码 | 无源码产物及路径差异。**以补偿控制替代**（2026-11-09 决策）：参考引擎 `execStorage` 读文件当纯文本、`tTJS::LoadByteCode` 无入口调用，不改参考引擎则环③无入口。等价证据链 = ①产物身份可追溯（manifest 固定参考二进制 SHA-256 + commit + `--dump` 参数 + 语料源/字节码哈希）②格式逐字段符合（61 组字节码全部经转译器解析并执行成功）③环④语义三方一致。若未来参考引擎增加字节码加载入口，可随时补跑——manifest 哈希链保证语料不漂移 |
| 跨引擎执行 | 参考执行 vs Ciallang 转译执行/反编译源码执行 | 转译近似（§8.1）与两引擎共用错误 |

stdout 逐字节外，脚本主动记录类型、状态和副作用序列；stderr 脚本输出与工具诊断分通道。
重点语料：无 debug、优化前后、flag 跨块、对象/键/RHS 单次求值、raw property、setter 抛错、
this/global proxy、null/void、NEW 与 CALL、缺省/省略/展开实参、动态父类、字段顺序、短路返回值、
switch 贯穿、嵌套 try/catch/重抛、with、eval 名字依赖、共享容器与闭包身份、临时值 GC 保活。
按 `doc/grammar/tjs.y` 建形态清单，另加不受编译器模板限制的合法字节码变异。验收报告保留每个
输入哈希、工具版本、转换能力、状态与首个差异。

## 8. 预期使用方式

```text
cial --decompile input.tjsbc -o restored.tjs --map restored.map.json
cial --decompile input.tjs            # 自产路径
```

默认严格输出；`Unsupported` 时非零退出且不生成成功文件。参数在 CLI 接入批次（M0）确定。
宿主可经注册组装同一管线；首批只有 `Tjs2SourceBackend`，可插拔 adapter/pass 为演化方向。
