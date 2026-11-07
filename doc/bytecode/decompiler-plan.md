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
| M5 | 硬核兜底：任意 CFG 降级、switch、嵌套异常及覆盖扩展 | **进行中**。switch 主干、循环组合与自产嵌套已验收；参考嵌套返回/三层合流固定样本已验收；函数内标量 CFG 状态机首版已完成；异常区域状态机首版已完成（结构化 try/catch 属后续批次）。当前状态见下节，历史证据见 M5 note。 |

M0/M1 后以真实产物的语义缺口重新估算工期；属性协议、异常、动态作用域是主要不确定项。

### M5 当前状态（2026-11-05）

| 项目 | 已验收能力 | 剩余工作 |
|---|---|---|
| switch | 常量/多值标签、贯穿、break/default、判别值单次求值；自产双层/三层嵌套及直接返回；参考函数内双层嵌套、贯穿、返回线程化与三层合流 | 更多合流形态；有副作用的后续 case 表达式 |
| 循环组合 | 自产/参考 while、do、for 与 switch 的 break/continue；case/default 内循环；for 更新段调用计数 | 更多数据流形态；顶层无名寄存器携带的 for 初值仍拒绝 |
| 控制流与异常 | 结构化优先；函数内 51 类受支持 TAC 的任意 CFG（含不可约）可降为状态机，寄存器逐次赋值，合成行不伪造来源；函数内 try/catch（嵌套/重抛/无绑定）经状态机恢复，9 组语料参考对拍一致 | 结构化 try/catch 区域恢复（现全部经状态机平铺输出）；调用/接收者、闭包及对象常量的降级协议扩展 |
| 动态名字 | 避让已有参数、局部与 Atom 名；合成存储仅限函数，声明提升、赋值留原路径 | with/eval 名字依赖与不能恢复的边界裁定 |
| M4 移交 | s7_ext outputdebug 读取已修复；实参及赋值右值匿名函数字面量的自产恢复已验证 | super 构造 prologue、CallMethodV 行表、testDeref 形态验收；ExprFunction 变量赋值/实参参考路径已验收；对象成员赋值的 NEW/this 差异待修 |
| 最终验收 | 37 组成功 switch 字节码三路执行一致；新返回/三层及 ExprFunction 针对性参考对拍通过，扩展参考运行有间歇失败，见 note；拒绝样本固定登记 | 完整能力矩阵与 Unsupported 清单、模糊覆盖；参考原字节码直接加载环缺入口 |

实现见 `src/decompile/ExprBuilder.cpp`；回归按接口/表达式与控制流分在
`tests/cli/test_decompile.py`、`tests/cli/test_switch.py`，运行入口见
[测试导航](../../tests/README.md)。固定输入与参考身份见
`tests/test_files/tjsbc/decompile/manifest.json`。M5 总体仍为进行中；
各批次命令和失败证据见 [M5 note](../../.agents/notes/proposed/architecture/2026-11-04-decompiler-m5.md)。

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

## 7. 验证闭环

沿用 `tests/tjs/` 脚本断言与 CTest；内部图/来源/GC 所有权用 Catch2 并在
`tests/CMakeLists.txt` 显式登记；runner 参考 `bench/bytecode_conformance.py` 的路径探测、
哈希、超时与无损输出记录。

| 环 | 比较对象 | 发现的问题 |
|---|---|---|
| 自产 round-trip | 源码在 Ciallang 执行 vs 反编译再编译执行 | 内部分析、打印与后优化形态恢复错误 |
| TJS2 源码对拍 | 同一 TJSCompiler 中原源码 vs 其字节码的反编译源码 | 原操作恢复与目标语法问题 |
| 字节码真实性 | 参考引擎加载原字节码 vs 运行反编译源码 | 无源码产物及路径差异 |
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
