# TJS2 字节码 → Ciallang 内部指令 转译设计

> **已选定的技术路线**（2026-10 决策）：Ciallang 加载 TJSCompiler 编译出的字节码文件
> （`"TJS2" "100\0"` 格式，见 [tjs2-bytecode.md](tjs2-bytecode.md) §5），**转译（转义）为
> Ciallang 的 TAC 指令**，进入现有 `Chunk`，由现有 VM（`run()`/`run0()`）执行。
> 不采用"独立 TJS 解释器"方案（被否决原因：保留两条执行路径不如统一到现有 VM，
> 且本项目 TAC 本就对标 TJS 指令集）。
>
> 本文是**实现前的设计文档**（本文档本轮只写设计、不写代码）。前置阅读：
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
| CEQ/CDEQ/CLT/CGT r1,r2 | `EQ/AbsEQ/GT/LT r1, r2, %F`（⚠️ CLT=GreaterThan、CGT=LittlerThan 已被 TJS 编译器换序，直接对应即可） |
| TT r / TF r | `GLocal r → %F` / `GLocal r → %F` + `LNot %F,%F` |
| SETF r / SETNF r | `GLocal %F → r` / `GLocal %F → tmp` + `LNot tmp → r` |
| NF | `LNot %F, %F` |
| JF addr / JNF addr | `Test %F` + `JmpE @l` / `Test %F` + `JmpNE @l` |
| JMP addr | `Jmp @l` |

优点：完全用现有 55 条 TAC，语义逐条可对拍；缺点：每分支多一条 `Test`。
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

| TJS | 转译 | 备注 |
|---|---|---|
| GPD d,o,m | `Load %n, *m'`（名字常量）+ `GProp %o,%n,%d` | direct 型先把常量池字符串装入寄存器 |
| GPI d,o,m | `GProp %o,%m,%d` | 成员名已在寄存器 |
| SPD o,m,s | `Load %n,*m'` + `DProp %o,%n,%s` | ⚠️ SPD 操作数顺序 `[obj,member,src]`，DProp 是 `[obj,name,src]` 一致 |
| SPI o,m,s | `DProp %o,%m,%s` | |
| GETP/SETP | **缺口** | 提案 `GetP/SetP` 或 Property invoke 近似 |
| DELD/DELI | **缺口** | 提案 `DelProp`（返回 bool 到 %d） |
| GPDS/SPDS/GPIS/SPIS（IGNOREPROP） | **缺口** | Ciallang Property 无穿透标志；提案 GProp/DProp 加 flag 操作数 |
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
| CALLD d,f,m,[args] | `Load %n,*m'` + `GProp %f,%n,%f2` + Push… + `Call %d,%f2,n` | 方法名 direct |
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
| CLT/CGT 换序 | TJS 编译器已把 `<` 编为 CLT(=GreaterThan 换序) | 转译按表直译即可，**不要"纠正"** |
| 整数/实数除法 | DIV/IDIV vs div/idiv | 对拍 |
| 布尔判定 | TJS `value != 0` 式宽松真 vs `Value::asBool` | 对拍（空串/void/null/NaN） |
| flag 物化 | flag 跨指令存活的场景（CEQ …非 flag 指令… JF） | %F 物化天然正确；仅性能损耗 |
| 字符串编码 | 文件 UTF-16LE ↔ 运行 UTF-8 | 转换器单测（ASCII/中文/emoji 代理对） |
| 全局对象 | TJS global 可任意读写；Ciallang GlobalObject | 行为一致，对拍 |
| 栈深限制 | Ciallang 1024 帧 vs TJS 无显式同值 | 记录差异 |

## 9. 对拍与工具链（已立项，未实现）

1. **TJSCompiler 加 `--dump <out>` 导出选项**（仅改 `src/main.cpp`，默认行为不变）：
   调 `tTJS::CompileScript(text, NULL, NULL, &stream)`（`tjs.cpp:571`）产出 `"TJS2 100\0"` 文件；
2. **语料**：`tests/test_files/tjs2bc/*.tjs` + 对应 `.bc` 产物入库（生成后提交，CI 不依赖 TJSCompiler）；
3. **对拍用例**：同一脚本分别跑 `TJSCompiler.exe x.tjs` 与 `cial --tjsbc x.bc`，stdout 逐字节比对
   （`cial` 增加 `--tjsbc` 入口为 M1 任务）；
4. 分组用例与 §10 里程碑一一对应，进 Catch2（`tests/tjsbc_test.cpp`）。

## 10. 里程碑

| 里程碑 | 范围 | 出口判据 |
|---|---|---|
| **M1** | Reader（容器+常量池+context 骨架）；顶层/函数 context 转译（§4.1/4.2 基型/4.5 CALL/4.6）；flag 物化；prologue；`cial --tjsbc` | fib/循环/算术/字符串拼接/print 的 .bc 对拍全绿（走 `run()`） |
| **M2** | 属性全族（GPI/SPI/GPDS…模板）、ctClass 加载期解释、NEW、try/catch、property | 原 kirikiri `test_class.tjs`/`test_function.tjs` 编译产物对拍 |
| **M3** | 新增 TAC（比较写 zf / Inc/Dec / Typeof / Asc/Chr / BNot / DelProp / GetP/SetP）+ run0 编码补全 | M1 语料走 `run0()` 性能对齐源码路径；指令覆盖 128/128 |
| **M4** | octet 常量、collapse 参数、展开/省略调用、EVAL/EEXP、superClassGetter 惰性 | tjs2 目录全部脚本 + 边缘语义清单通过 |

**新增 TAC 指令提案汇总**（M3 落地，命名沿用现有风格）：
`CmpEQ/CmpDEQ/CmpLT/CmpGT`（写 zf）、`Inc/Dec`、`Typeof`、`Asc/Chr`、`BNot`、`Num`、
`DelProp`、`GetP/SetP`、`GPropI 已有(=GProp)`、`CallMethod(direct)` 可选、`ExpandCall`（M4）。
每条按 AGENTS.md"新增指令清单"四处同步（X-macro/execute/encode/dispatch）。
