# TJS2 字节码 → Ciallang 内部指令 转译设计

> **已选定的技术路线**（2026-10 决策）：Ciallang 加载 TJSCompiler 编译出的字节码文件
> （`"TJS2" "100\0"` 格式，见 [tjs2-bytecode.md](tjs2-bytecode.md) §5），**转译（转义）为
> Ciallang 的 TAC 指令**，进入现有 `Chunk`，由现有 VM（`run()`）执行。
> 不采用"独立 TJS 解释器"方案（被否决原因：保留两条执行路径不如统一到现有 VM，
> 且本项目 TAC 本就对标 TJS 指令集）。
>
> 本文是**实现前的设计文档**。实施进度（2026-10-27）：阶段 0（--dump）与阶段 1（Reader）完成；
> 阶段 2（TjsTranslator）**M1+M2 完成**：kirikiri `test_class.tjs` 编译产物对拍通过
> （字节码路径与源码路径输出逐字节一致；s6/s7 + kirikiri test_* 语料全绿）。M2 落地：类 context 加载期解释
> （var 成员 + initDefaultVal（无成员类也产出，initClassFields 契约）+ 构造器 setConstructor
> （不入成员表，保 super.<Ctor>() 经类对象调用进 super 分支）+ 成员 property 注册 +
> superClassGetter 加载期解释为 extends）、NEW→Call（ClassObject::call 构造）、
> GETP/SETP→DerefGet/Set、P 变体（INCP/二目 P 读-算-写）、DELD/DELI→DelProp、
> TYPEOFD/I→GProp+TypeOf、octet 常量、proxy(ra[-2]) 访存→GThis/DThis/GRefRaw
> （TJS2 objthis-proxy 的 this→global 回落语义）。修复的 M1 遗留缺陷：JF/JNF 物化跳转
> 方向颠倒、INCPD/ADDPD 等 +1/+2/+3 变体落 default 静默不发射、INCPD 自加（加载 1 的
> 寄存器未参与运算）、INTER_OBJECT 占位常量被 addConstant 去重合并（回填互相覆盖）。
> 配套 VM 语义修正：DProp/GProp 对全局 property 槽触发 getter/setter（TJS2 PropSet 语义），
> ChgThis 接受 Property 闭包，Function::call 未绑定时继承调用者 this（TJS2 调用点绑定，
> super.<方法>() 经类对象成员访问的关键路径），GPD/GPI/SPD/SPI/DPropRaw/GPropRaw 支持
> 整数成员名（数组数值下标，旧版 PropGet/SetByNum——TJS `a[0]` 编译为 GPI+整数名）。
> **M4 完成（2026-10-27）**：省略透传（argc=-1 → CallOmit）、展开调用（argc=-2 →
> CallExpand，fatNormal/fatExpand/fatUnnamedExpand 条目表）、collapse 参数
> （FuncDeclCollapseBase → CollapseArgs，超额实参由 Function::call 复制到窗口顶部
> 保留区按调用序保全）、EVAL/EEXP→EvalStr；CLT/CGT 序修正。`test_function.tjs`
> 与 `test_misc.tjs` 字节码路径全量通过（test_misc 与源码路径逐行一致；这两类语法
> 源码路径不支持，对拍基准为原版引擎行为）。
> **成员名 Atom 直引（2026-10-28）**：GPD/GPDS/SPD/SPDS/DELD/TYPEOFD/CALLD 及
> INC·DEC·二目组的 PD 变体不再「Load 名字常量进寄存器 + 寄存器名属性指令」，
> 改发 `GPropC/DPropC/GPropRawC/DPropRawC/DelPropC`（成员名 u32 Atom 内联，
> dispatch 免再 intern）；PI/GPI 等整数名形态保留寄存器指令（见
> ciallang-bytecode.md §3.10）。
> **M3 完成（2026-10-27）**：CmpEQ/CmpDEQ/CmpLT/CmpGT 比较直写 _zf + 转译器前瞻融合
> （比较/TT/TF 紧随 JF/JNF 时省 %F 物化与 Test，每分支省 1 条）、Inc/Dec 单指令
> （int/real，TJS increment 语义——旧译法 LoadImm+Add 对字符串会误拼接）、
> BNOT/NUM/OCTET 转换指令补全（Value 增 bitNot/toNumber；128 操作码转译覆盖闭环）。
> **Reader 修复**：OCTET 段条目读取后未推进 pos（内容字节缺失导致多 octet 文件
> 错位，仅靠段尾收口掩盖）——test_variant 因此解锁并全量通过（与源码路径一致）。
> **dst=0 调用丢弃语义修复**：TJS `CALLD dst=0` 表示不写回（原版传 NULL 结果指针），
> 旧译法写入 %K（void 槽）沿隐式 return 链污染上层 `SRV r0` 返回值——统一落到窗口
> 顶部专用丢弃寄存器。仍缺口：test_string（TJSCompiler --dump 自身崩溃）。
> 已知近似见 §8.1。
> 前置阅读：
> [tjs2-bytecode.md](tjs2-bytecode.md)（旧格式）、[ciallang-bytecode.md](ciallang-bytecode.md)（本项目指令集）。

---

## 1. 总体架构

新增模块 `src/tjsbc/`（建议）：

```
"TJS2 100\0" 文件（TJSCompiler --dump 产物，见 §9）
      │
      ▼
TjsBytecodeReader        ── 解析容器/常量池/context 骨架（对应 tjsByteCodeLoader 的读取部分）
      │  产出内存模型：ConstPool(7 段) + Vec<TjsContext>（code 为 i16 码字、索引形态）
      ▼
TjsTranslator            ── 两遍转译
      │  Pass 1: 按指令长度表线性步进，记录 TJS 码字偏移 → TAC 下标 映射
      │  Pass 2: 逐条发射 TAC（Jmp 用 Pass 1 的映射做 Label）
      │  每个 context 产出一个 Chunk；INTER_OBJECT 常量回填为 FuncMeta/ClassMeta
      ▼
vm::Chunk + FuncMeta     ── 进入现有执行体系（VM::eval / run()）
```

关键原则：

1. **加载器不做语义**——只按 §5 格式解析（对应 `tjsByteCodeLoader.cpp` 的行为）；
2. **转译器不改 TJS 语义**——每条 TJS 指令的转译规则固定、可单测；
3. **执行走现有 VM**——与源码路径共用同一套 Value/GC/类系统，收益共享。

## 2. 寄存器空间映射（核心方案）

### 2.1 偏移恒等映射

TJS 寄存器下标 `r ∈ [-(VC+RC), MaxFrameCount]`（VC=maxVariableCount，RC=variableReserveCount，
见 [tjs2-bytecode.md](tjs2-bytecode.md) §1.1）。定义：

```
K = VC + RC                      （TJS 帧的 ra 基偏移）
cialReg(r) = r + K               （恒等平移，全为非负）
chunk->_registerCount = VC + RC + 1 + MaxFrameCount + 附加 scratch（§3.1）
```

TJS 帧的全部槽位与 Ciallang 寄存器一一对应，**TJS 代码的寄存器使用纪律
（frame 临时区的分配/释放顺序）原样保留**，无需分析数据流。

| TJS 槽 | Ciallang 寄存器 | 入口初值（prologue 发射） |
|---|---|---|
| `ra[-2]`（proxy/global） | `%K-2` | `Global %K-2`（近似：不建 proxy，GThis 的动态链查找已覆盖回落语义） |
| `ra[-1]`（this） | `%K-1` | `This %K-1` |
| `ra[0]`（void） | `%K` | `Load %K, *0`（常量池 0 号恒为 Void） |
| `ra[1..]`（frame 临时） | `%K+1 ..` | —（TJS 字节码自会先写后读） |
| `ra[-3-i]`（第 i+1 实参） | `%K-3-i` | prologue 搬运，见 §2.2 |

### 2.2 实参搬运（prologue）

Ciallang 调用约定：实参压池顶，被调帧 `%0..%arity-1` 与之重合（[ciallang-bytecode.md](ciallang-bytecode.md) §2）；
TJS 约定：实参在 `ra[-3]` 向下。转译 prologue：

```
FuncMeta.arity = TJS FuncDeclArgCount           // Function::call 会自动补 void 到 arity
for i in 0 .. FuncDeclArgCount-1:
    DLocal %i → %K-3-i                          // 搬运实参到 TJS 位置
（可优化：DLocal 纯 mov 语义，等价 GLocal；选不影响 prop 语义的 DLocal）
```

TJS 的默认参数值在函数体头部以普通字节码出现（判 void → 赋值），**无需转译器处理**（验证点，见 §10）。

### 2.3 限制（首期明确不支持）

- `FuncDeclCollapseBase >= 0`（`*args` 收拢）：Ciallang 无对应 → 加载时报"不支持"；
- 省略调用（argcount=-1）与展开调用（-2）：见 §6 CALL；
- 实参数 > arity 时 Ciallang 丢弃多余实参；TJS 在无 collapse 时同样丢弃——一致，无处理。

## 3. 标志寄存器（flag）物化方案

TJS 的 VM 级单 bool `flag`（CEQ/TT 写、JF/JNF 读、NF 取反、SETF/SETNF 与寄存器互传）
在 Ciallang 对应 `_zf`，但 **TAC 的 3 op 比较直接产出 bool 值**。方案：**物化到专用 scratch 寄存器
`%F`（编号 = 寄存器总数 +0，即 K+MaxFrameCount+1 起）**，统一规则：

| TJS | 转译序列 |
|---|---|
| CEQ/CDEQ/CLT/CGT r1,r2 | `EQ/AbsEQ/LT/GT r1, r2, %F`（⚠️ CLT/CGT 是自然序：tTJSVariant::GreaterThan 实现为 `this < val2`、LittlerThan 为 `this > val2`（tjsVariant.cpp:827/853），即 CLT= `<`、CGT= `>`。§8 旧表"编译器已换序"的说法有误——只对了名字没对实现） |
| TT r / TF r | `GLocal r → %F` / `GLocal r → %F` + `LNot %F,%F` |
| SETF r / SETNF r | `GLocal %F → r` / `GLocal %F → tmp` + `LNot tmp → r` |
| NF | `LNot %F, %F` |
| JF addr / JNF addr | `Test %F` + `JmpE @l` / `Test %F` + `JmpNE @l` |
| JMP addr | `Jmp @l` |

优点：用现有 TAC 指令，语义逐条可对拍；缺点：每分支多一条 `Test`。
M3 优化提案：新增 2 操作数"比较写 zf"指令族（`CmpEQ %r1,%r2` 直写 `_zf`），可省 `%F` 与 `Test`。

## 4. 操作码转译总表

> 完整 TJS 语义见 [tjs2-bytecode.md](tjs2-bytecode.md) §3。`%x` = 映射后寄存器，`*c` = 转换后常量池下标。

### 4.1 基础/传送

| TJS | 转译 | 备注 |
|---|---|---|
| NOP | `NOP` | |
| CONST r, da | `Load %r, *c` | 常量转换见 §5 |
| CP d, s | `DLocal %s → %d` | |
| CL r | `Load %r, *0`（Void 常量） | |
| CCL r, n | `Load %r,*0; Load %r+1,*0; … ×n` 展开 | n 编译期已知 |

### 4.2 双目运算（含四联变体）

以 ADD 组为例（其余 15 组同构）：

| TJS | 转译 | 备注 |
|---|---|---|
| ADD r1,r2 | `Add %r1, %r2, %r1` | TJS 基型破坏性，3op TAC 传回原寄存器即等价 |
| ADDPD d,o,m,p | `GPropD 读成员` + `Add` + `SPD 写回`，见 §6.1 | 近似：读-算-写**非原子**，property 并发语义差异可接受 |
| ADDPI d,o,m,p | 同上（成员名在寄存器） | |
| ADDP d,t,p | **缺口**：无成员闭包运算 | 近似：propObjectGet/Set（需新 TAC `GetP/SetP`，提案） |
| INC r | `LoadImm %t,1` + `Add %r,%t,%r` | 提案：`Inc %r` 单条 |
| INCPD/PI/P | 同属性变体展开 | |

### 4.3 属性访问

> 下表为设计期快照，实施状态以本文件头部进度注记与 `TjsTranslator.cpp` 为准；
> direct 名（GPD/SPD/DELD/TYPEOFD/CALLD）现均发 *C 直引指令（不再 Load 名字常量）。

| TJS | 转译 | 备注 |
|---|---|---|
| GPD d,o,m | `GPropC %o,atom,%d` | 成员名 Atom 内联（设计期方案 Load+%n+GProp 已替换） |
| GPI d,o,m | `GProp %o,%m,%d` | 成员名已在寄存器（整数名=数组下标） |
| SPD o,m,s | `DPropC %o,atom,%s` | ⚠️ SPD 操作数顺序 `[obj,member,src]`，DProp 是 `[obj,name,src]` 一致 |
| SPI o,m,s | `DProp %o,%m,%s` | |
| GETP/SETP | `DerefGet/DerefSet` | M2 落地 |
| DELD/DELI | `DelPropC`/`DelProp`（返回 bool 到 %d） | M2/M4 落地 |
| GPDS/SPDS | `GPropRawC`/`DPropRawC`；GPIS/SPIS 走寄存器 Raw 指令 | M2/M4 落地 |
| SPDE/SPIE/SPDEH（MEMBERENSURE） | Ciallang `setProp` 语义即"不存在则创建" | DataObject::setProp 直接 `_props[a]=v`，天然 ENSURE，行为兼容 |

字符串/字节串下标（`str[i]`）：TJS 在 GPD/GPI 内特判；Ciallang 走 stdlib 原生方法表
（`GProp` 的非对象分支）——**需为 String/Octet 补数字下标成员访问**（验证点 §10）。

### 4.4 转换/一元

| TJS | 转译 | 备注 |
|---|---|---|
| INT/REAL/STR | `ToInt/ToReal/ToString %r,%r` | 原地 |
| CHS | `ChgSign %r,%r` | |
| LNOT | `LNot %r,%r` | |
| BNOT | **缺口**（按位非） | 提案 `BNot`（Value 已有位运算体系） |
| NUM | 近似 `ToReal`+`ToInt`？ | **验证点**：tjsVariant::tonumber 语义（int/real 归一） |
| ASC/CHR | **缺口** | 提案 `Asc/Chr`（`$`/`#` 前缀） |
| TYPEOF/TYPEOFD/TYPEOFI | **缺口** | 提案 `Typeof`（返回类型名字符串） |
| INV | `Inv %r` | |
| CHKINV r | `ChkInv %r,%r` | TJS 原地写回 bool，Ciallang src,dst 同寄存器即可 |
| CHKINS n,o | `DLocal %o→%F2` + `ChkIns %n,%F2` | 物化 flag：结果在 %F2，接 §3 规则 |
| EVAL/EEXP | **缺口（远期）** | 运行期把字符串送 Parser→IRGenerator→执行；依赖源码前端完备 |

### 4.5 调用

| TJS | 转译 | 备注 |
|---|---|---|
| CALL d,f,n,[args] | `Push %a1 … Push %an` + `Call %d,%f,n` | this 绑定：Ciallang `Function::call` 用函数对象 thisObj；成员调用经 GProp 时 `DataObject::getProp` 已克隆并绑定 this（`Class.cpp:210-235`），语义对齐 |
| CALLD d,f,m,[args] | `GPropC %f2,%o,atom` + Push… + `Call %d,%f2,n` | 方法名 Atom 直引（设计期方案 Load+%n+GProp 已替换）；obj=proxy 走 GThis |
| CALLI d,f,m,[args] | `GProp %f,%m,%f2` + … | |
| NEW d,c,[args] | **设计点** | TJS：调用类 context（其代码体做类构造，见 §7）。转译：对类值 Call；需要 Ciallang `ClassObject` 可调用 = 创建 DataObject + 跑构造；当前 ClassObject::call 语义需核对/补齐 |
| argcount = -1/-2 | **缺口** | 省略透传/展开调用；提案 `ExpandCall`；首期加载报不支持 |

### 4.6 返回/异常/其他

| TJS | 转译 | 备注 |
|---|---|---|
| SRV r + RET | 合并为 `Ret %r` | 逐条扫描时记住最近 SRV 的寄存器；裸 RET → `Ret %voidReg` |
| THROW r | `Throw %r` | |
| ENTRY catch,ex / EXTRY | 转译器静态收集 `{tryStart, tryEnd=catchAddr, exValueReg}` 进 `Chunk::_throwHandlers`；指令处发 `NOP` | Ciallang unwind 命中后 `setPC(tryEnd)` 从 catch 块继续，与 TJS catch 语义对齐；**嵌套 try/重抛**为验证点 |
| CHGTHIS f,t | `ChgThis %t → %f` | ⚠️ Ciallang ChgThis 操作数是 (src=this值, dst=函数)，TJS 是 (code1=函数, code2=this)，**顺序相反** |
| GLOBAL r | `Global %r` | |
| ADDCI / REGMEMBER | 不发射 TAC；类构造在加载期解释，见 §7 | |
| DEBUGGER | `Debugger` | |

## 5. 常量池转换

| TJS DataArea 条目 | Ciallang 常量 | 备注 |
|---|---|---|
| VOID / OBJECT(null) | `*0`（Void） | |
| BYTE/SHORT/INTEGER/LONG idx | 不进常量池：转译为 `LoadImm`（在 CONST 转译点内联） | 整数值域已归一为 i64 |
| REAL idx | `Constant(Real)` | double 位模式原样 |
| STRING idx | UTF-16LE → UTF-8 → `AtomTable::intern` → `Constant(Atom)` | **注意代理对**（tjs_char 16 位，UTF-16 → UTF-8 需正确处理代理项） |
| OCTET idx | **缺口**：Constant 无 Octet 类型 | 提案 `ConstantType::Octet` + `Value::createValue` 分支；首期可报不支持 |
| INTER_OBJECT ctxIdx | `FuncMeta*` / `ClassMeta*`（Pass 2 回填） | 见 §7 |

两遍结构：Pass 1 为每个 context 建空 Chunk + 常量骨架；Pass 2 转译代码体并回填引用
（对应 `tjsByteCodeLoader` 的两遍回填，[tjs2-bytecode.md](tjs2-bytecode.md) §5.6）。

## 6. 关键机制细化

### 6.1 属性变体的读-算-写展开模板

`ADDPD %d, %o, %m, %p`（`d = o[m] + p`，成员为常量名）：

```
Load  %n, *m'          ; 成员名常量
GProp %o, %n, %t       ; t = o[m]   （Property 触发 getter）
Add   %t, %p, %t       ; t = t + p
DProp %o, %n, %t       ; o[m] = t   （Property 触发 setter）
（d≠0 时再 DLocal %t → %d）
```

与 TJS 原子 `Operation()` 派发的差异：getter/setter 间状态可变——**已知近似**，记录在案。

### 6.2 跳转重定位（两遍法）

- Pass 1 按 [tjs2-bytecode.md](tjs2-bytecode.md) §3 长度表步进（含 CALL 变长解码），
  记录 `tjsWordOffset → (Pass 2 将发射的) 首 TAC 下标`；
- Pass 2 发射跳转：`Jmp Label(map[目标字偏移])`（TJS 跳转操作数是相对字节偏移 → 目标 = 指令首字偏移 + code[1]）；
- 一条 TJS 指令展开多条 TAC（CCL/flag 物化/属性模板）时，映射只指向**首条** TAC——
  TJS 跳转目标永远是指令边界，安全。

## 7. context 类型 → Ciallang 元数据

| TJS contextType | Ciallang 映射 | 方案 |
|---|---|---|
| ctTopLevel | 顶层 Chunk | 直接执行入口 |
| ctFunction / ctExprFunction | `FuncMeta(arity=FuncDeclArgCount, chunk)` | 直接 |
| ctPropertySetter / ctPropertyGetter | `PropMeta(setFunc/getFunc)` | 父 ctProperty context 聚合二者 |
| ctSuperClassGetter | **惰性求值**，Ciallang extends 是名字列表 | 首期：加载期执行 getter 代码体求出父类名列表（若代码体可静态解释），否则报不支持 |
| ctClass | `ClassMeta` + 运行期构造 | **加载期解释**（见下） |

**ctClass 的加载期解释**：类 context 的代码体是固定模式的构造序列
（CONST 函数常量 → REGMEMBER 注册成员表 → ADDCI 标记 → 可能的 GPD 赋初值）。
转译器遇到 ctClass context 时**不生成 TAC**，而是直接解释这几种模式，填充
`ClassMeta.memberShapeMetas/memberMetas/constructor`；遇到无法识别的模式（类体内出现任意语句）
则回退为"运行期方案"（提案：新增 `RegMember/AddCI` TAC，首期不做）。

propcount 成员表（[tjs2-bytecode.md](tjs2-bytecode.md) §5.3）直接映射 `ClassMeta` 成员。

## 8. 与源码路径的语义对账（转译特有风险）

| 差异点 | 说明 | 对策 |
|---|---|---|
| `===` 语义 | TJS DiscernCompare vs Ciallang `discernEquals` | 用例对拍（int/real/string/void/null/object 全组合） |
| 加法重载 | TJS ADD 对 string/int/real 混合的隐式转换规则 | Value::add 已按 TJS 实现（有 ieee 测试），对拍确认 |
| CLT/CGT 序 | `CLT=自然 <`、`CGT=自然 >`（GreaterThan/LittlerThan 名实相反，见 §3） | CLT→LT、CGT→GT（2026-10-27 修正：曾按"换序"说法译反，s 语料无 `<` 比较未暴露，test_misc 暴露） |
| 整数/实数除法 | DIV/IDIV vs div/idiv | 对拍 |
| 布尔判定 | TJS `value != 0` 式宽松真 vs `Value::asBool` | 对拍（空串/void/null/NaN） |
| flag 物化 | flag 跨指令存活的场景（CEQ …非 flag 指令… JF） | %F 物化天然正确；仅性能损耗 |
| 字符串编码 | 文件 UTF-16LE ↔ 运行 UTF-8 | 转换器单测（ASCII/中文/emoji 代理对） |
| 全局对象 | TJS global 可任意读写；Ciallang GlobalObject | 行为一致，对拍 |
| 栈深限制 | Ciallang 1024 帧 vs TJS 无显式同值 | 记录差异 |

### 8.1 M2 落地后的已知近似（逐条记录在案）

| 差异点 | TJS2 行为 | 本项目行为 | 说明 |
|---|---|---|---|
| proxy(ra[-2]) 直访 | objthis-proxy：this 优先、global 回落 | prologue 以 `This` 填 %K-2；GPD/GPDS/SPD/SPDE/SPDEH/INCPD/二目 PD/CALLD 在 obj==-2 时译为 GThis/GRefRaw/DThis（atom 形态，自带回落）| GPI/GPIS/SPIS/CALLI（成员名在寄存器）经 proxy 无回落，直接对 this 值访问 |
| SPDS obj==-2 | 原始写 proxy（this 空则 global） | `DPropRaw(%K-1)`（落 this 槽） | this 为空的帧不回落 global；语料未覆盖此形态 |
| TYPEOFD/TYPEOFI | 成员不存在 → "undefined" 字符串 | GProp 直接抛错 | 成员存在路径一致 |
| 顶层 property 写 | PropSet 触发 setter | DProp 已放开 Global 豁免，同触发 | 源码路径走 DGlobal，行为一致 |
| 未绑定函数调用 | 调用点绑定 `clo.ObjThis ?: ra[-1]` | `Function::call` 未绑定时继承调用者 thisObj | super.<方法>() 经类对象取成员（未绑定）的关键路径；全局函数调用行为不变（调用者 this 即 global 或空） |
| 读-算-写属性模板 | `Operation()` 原子派发 | GProp→算→DProp 三条指令 | getter/setter 间状态可变，已知近似 |
| 类体 super 初始化三连（GPD+CHGTHIS+CALL） | 运行期重跑父类类体 | 加载期跳过，继承语义由 superClassGetter 解释的 `extends` 承担 | 与源码路径 extends 模型一致 |
| int/real 等 binop PD 变体 dst==0 | 丢弃结果 | 不发射写回（%K void 槽不被污染） | 修复：曾无条件写回 %K，污染裸 RET 的 void 返回值 |
| 省略透传（argc=-1） | 转发调用者实际实参（含超额） | `CallOmit`：实参由 Function::call 复制到窗口顶部保留区（regCount..，调用顺序），帧 %0..%arity-1 重写为前 arity 个 | 超额时物理池布局只会让末 arity 个落在 %0..，且 prologue 静态写位会覆盖之 |
| 展开调用（argc=-2） | fatNormal/fatExpand(Array)/fatUnnamedExpand 条目表 | `CallExpand dst, func, unnamedBase, entries` 同构实现；fatExpand 仅支持 Array | UnnamedExpand 以调用方 FuncDeclUnnamedArgArrayBase 为界 |
| collapse 参数（`function f(a, b*)`） | 入口把 args[base..] 收拢成 Array 落 ra[-3-base] | `CollapseArgs dst, base` 在 prologue 末尾发射（实参尚完整时） | 无超额实参也建空 Array（TJS2 无条件建） |
| EVAL/EEXP | 字符串即时编译执行，this=ra[-1] | `EvalStr`（源码路径已有指令）；EEXP 编译期形态差异运行期无别 | — |
| 整数成员名（`a[0]` → GPI+整数名） | PropGet/SetByNum | GPD/GPI/SPD/SPI/DPropRaw/GPropRaw 整数名分支 = 数组数值下标（负从尾、写越界扩展） | — |
| 调用 dst=0 | 丢弃（结果指针 NULL，不写回） | 落窗口顶部专用丢弃寄存器（regCount-1） | 修复：曾写 %K（void 槽）污染 `SRV r0` 返回链 |
| INC/DEC 基型 | increment（int/real，字符串报错） | `Inc`/`Dec` 单指令，同语义 | 旧译法 LoadImm+Add 对字符串会误拼接 |
| BNOT/NUM/OCTET | bitnot / tonumber / ToOctet | `BNot`/`ToNumber`/`ToOctet`（Value 增 bitNot/toNumber） | OCTET：void→空 octet、octet 不变、其余报错 |

## 9. 对拍与工具链

### 已有 fixture 与严格源码/字节码对拍

Reader 回归位于 `tests/tjsbc_reader_test.cpp`（畸形输入拒绝 + 手工构造最小文件），
转译执行回归位于 `tests/tjsbc_translator_test.cpp`；入库的 `.tjsbc` 语料位于
`tests/test_files/tjsbc/`（s6_class/s7_ext/test_class/test_function/test_misc/
test_variant/test_with，**全部有对应源码**），CI 不需要参考引擎即可运行。
`cial --tjsbc` 的 CLI 路径另由 `tests/cli_test.py` 覆盖。

> 历史清理（2026-10-30）：早期入库的 s1–s5 `.tjsbc` 无对应 `.tjs` 源码（初始提交即只含
> 二进制，Git 历史不可恢复源码），按"无源码 fixture 一并清理"的约定删除；仅针对它们的
> Reader 断言（常量池/签名/context 结构 SECTION）与对比工具的降级覆盖路径同步移除。
> 语料级结构覆盖由有源码的 s6/s7/test_* 承担，必要时可用
> `TJSCompiler --dump <script.tjs> <out.tjsbc>` 从源码再生成。

`bench/bytecode_conformance.py` 默认运行同一个 Ciallang 的源码路径和字节码路径，
要求两边都以 0 退出，并按字节比较 stdout，不剥引号、时间戳、空行或诊断文本。
源码先在 fixture 同目录寻找，再按同名文件在 `tests/test_files/tjs2/` 寻找；
**找不到源码即报 `TOOL_ERROR` 并整体判失败**——工具不再提供任何"仅执行"性质的
降级覆盖（原 `--mode execute` 与 `SKIPPED_NO_SOURCE` 分支随 s1–s5 清理一并移除）。

```sh
python bench/bytecode_conformance.py --cial build/Windows/Debug/cial.exe   --report bench/results/bytecode-conformance.json
# 只复核指定用例（--case 可重复），目录和输出文件都可含空格
python bench/bytecode_conformance.py --cial build/Windows/Debug/cial.exe   --case s7_ext --case test_class
```

报告为 JSON，记录执行器、源码及字节码 SHA-256、退出码、超时、stdout 首个不同字节位置，
并同时保留 stdout/stderr 的可读文本和无损 Base64。`MATCH` 以外的实际比较结果均返回 1，
包括双方都执行失败；选中用例存在缺源码/工具错误也返回 1。参数错误返回 2。
每个子进程默认 15 秒超时，可由 `--timeout` 设置（`bench/bytecode_conformance.py`）。

严格报告会暴露 `test_misc` 的异常文本差异：源码路径携带源码行号，已有字节码 fixture
缺少相同源码位置，输出中没有该行号。此项仍为 `OUTPUT_DIFF`，不通过归一化或删除源码
诊断来强行判为匹配；这不等价于算术或异常捕获行为不兼容。

### 可选参考导出与入口能力探测

`--reference /path/to/TJSCompiler` 对**有源码的入库 fixture**（清理后即全部 fixture）有意义：
工具在临时目录内生成两个随机标记的最小 `Debug.message` 脚本，先确认参考 CLI 确实执行
不同的路径参数，再验证 `--dump <script.tjs> <out.tjsbc>` 生成 `TJS2 100 ` 头且能被
Ciallang 执行并输出对应标记。任一探测失败即停止并返回失败，不能把忽略参数、固定执行
脚本的旧入口当成有效对照。探测通过后才从源码临时导出选中语料并做上述源码/字节码对拍；
不会覆盖入库 fixture。该模式验证"参考导出 → 转译执行"工具链端到端，不依赖入库副本
的时效性。

```sh
python bench/bytecode_conformance.py --cial build/Windows/Debug/cial.exe   --reference /path/to/verified/TJSCompiler --case s7_ext   --report bench/results/bytecode-fresh-export.json
```

报告记录参考执行器 SHA-256 和两类探测结果。此模式验证**参考导出 → 转译执行**，
不是参考引擎完整语料 stdout 与 Ciallang 的三方语义对拍；后者已由宿主 `Scripts` 类
双侧实现后达成的 `test.tjs` 双引擎对拍覆盖（2026-10-30，见 doc/compatibility.md）。
参考项目的 CLI 适配（`--dump` 与脚本路径参数）不作为本仓库能力或上游默认行为承诺。
工具契约及真实 CLI/fixture 测试见 `bench/test_bytecode_conformance.py`
（设置 `TJS_REFERENCE` 环境变量可启用参考导出的真实子进程测试，否则跳过）。

## 10. 里程碑

| 里程碑 | 范围 | 出口判据 |
|---|---|---|
| **M1** ✅ | Reader（容器+常量池+context 骨架）；顶层/函数 context 转译（§4.1/4.2 基型/4.5 CALL/4.6）；flag 物化；prologue；`cial --tjsbc` | fib/循环/算术/字符串拼接/print 的 .bc 对拍全绿（s1–s5） |
| **M2** ✅ | 属性全族（GPI/SPI/GPDS…模板）、ctClass 加载期解释、NEW、try/catch、property、P 变体、DELD/DELI、TYPEOFD/I、octet 常量、proxy 回落链 | kirikiri `test_class.tjs` 编译产物对拍全绿（2026-10-27，与源码路径逐字节一致）；`test_function.tjs` 前半通过，展开/收拢调用部分留 M4 |
| **M3** ✅ | 比较写 zf 指令族（CmpEQ/DEQ/LT/GT + 前瞻融合）、Inc/Dec 单指令、BNot/ToNumber/ToOctet（Typeof/Asc/Chr/DelProp/GetP·SetP 已随 M1/M2 提前落地） | 指令转译覆盖 128/128（2026-10-27）；全部语料 run 路径全绿 |
| **M4** ✅ | collapse 参数（CollapseArgs）、展开调用（CallExpand）、省略透传（CallOmit）、EVAL/EEXP→EvalStr、CLT/CGT 序修正、dst=0 丢弃语义 | `test_function.tjs` 全量通过（2026-10-27）；test_misc/test_with/test_variant 与源码路径一致；`test.tjs` 双引擎对拍通过（2026-10-30 宿主 Scripts 类双侧落地，见 doc/compatibility.md）。遗留：test_string 原版导出崩溃（`--dump` segfault，2026-10-30 复核仍现，不影响对拍主路径） |

**新增 TAC 指令提案汇总**（M3 落地，命名沿用现有风格）：
`CmpEQ/CmpDEQ/CmpLT/CmpGT`（写 zf）、`Inc/Dec`、`Typeof`、`Asc/Chr`、`BNot`、`Num`、
`DelProp`、`GetP/SetP`、`GPropI 已有(=GProp)`、`CallMethod(direct)` 可选、`ExpandCall`（M4）。
每条按 AGENTS.md"新增指令清单"四处同步（X-macro/execute/encode/dispatch）。
