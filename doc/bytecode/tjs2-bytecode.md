# TJS2 字节码规范（旧版 TJSCompiler）

> 本文描述**原版 Kirikiri TJS2**（参考实现 `../TJSCompiler`）的虚拟机指令集
> 与编译产物序列化格式。是本项目实现"字节码兼容（加载 → 转译执行）"的规格依据。
> 转译设计见 [mapping.md](mapping.md)；本项目自身指令集见 [ciallang-bytecode.md](ciallang-bytecode.md)。
>
> 事实来源（行号以 TJSCompiler 当前源码为准）：
> - 操作码枚举：`src/syntax/lexer/tjsInterCodeGen.h:45-76`
> - 执行器（语义权威）：`src/syntax/tjsInterCodeExec.cpp`（`ExecuteCode` 主循环 938-1400）
> - 指令长度表/地址换算：`src/syntax/tjsScriptBlock.cpp:575-894`（`TranslateCodeAddress`）
> - 文件容器导出：`src/syntax/tjsScriptBlock.cpp:460-510`（`ExportByteCode`）+ `src/syntax/lexer/tjsInterCodeGen.cpp:3639-3747`（context 导出）
> - 常量池：`src/syntax/tjsConstArrayData.h:44-54`（类型 tag）、`tjsConstArrayData.cpp:201-323`（`ExportBuffer`）
> - 加载端：`src/syntax/tjsByteCodeLoader.cpp`

---

## 1. 执行模型

TJS2 VM 是**基于寄存器 + 常量池**的解释器，无操作数栈。每个函数调用（`tTJSInterCodeContext::ExecuteAsFunction`，`tjsInterCodeExec.cpp:716`）持有三块区域：

| 区域 | 变量 | 说明 |
|---|---|---|
| 代码区 | `CodeArea`（`tjs_int32*`） | 指令流，每条指令占 1~N 个 32 位字 |
| 寄存器区 | `ra`（`tTJSVariant*`） | 帧内所有槽位，**下标可为负**（见 1.1） |
| 数据区 | `DataArea`（`tTJSVariant*`） | 编译期常量池（`da`） |

另有整个 VM 级别的：

- **标志寄存器 `flag`**（单个 bool）：比较类指令（CEQ 等）写入，JF/JNF 消费，NF 取反，SETF/SETNF 与寄存器互传。
- **代码指针**：指向 CodeArea 的 `tjs_int32*`，按字推进。

### 1.1 帧布局（ra 的负偏移区）

帧分配 `MaxVariableCount + VariableReserveCount + 1 + MaxFrameCount` 个 Variant，
`ra = regs + MaxVariableCount + VariableReserveCount`（`tjsInterCodeExec.cpp:724-726`）：

```
                 更负 …
  ┌───────────────────────────────────┐
  │ 命名局部变量区（Namespace 绑定）    │  负方向延伸
  │ …                                  │
  │ ra[-5]  第 3 个形参                │
  │ ra[-4]  第 2 个形参                │
  │ ra[-3]  第 1 个形参  ← 实参从这里向下排（tjsInterCodeExec.cpp:778-800）
  │ ra[-2]  objthis-proxy（this+global 二义查找）或 global
  │ ra[-1]  this（objthis）
  ├───────────────────────────────────┤
  │ ra[0]   恒为 void（入口 Clear，代码生成器不占用）
  │ ra[1..] frame 临时区（表达式求值深度，MaxFrameCount 个）  正方向延伸
  └───────────────────────────────────┘
```

- 实参传递：调用方把第 i 个实参放到被调帧 `ra[-3-(i-1)]`；不足 `FuncDeclArgCount` 的补 void；
  `FuncDeclCollapseBase >= 0`（`function f(a, *rest)` 收拢参数）时，把多余实参打包成 Array 放到
  `ra[-3-FuncDeclCollapseBase]`（`tjsInterCodeExec.cpp:802-814`）。
- `ra[-2]` 的 proxy 是 `tTJSObjectProxy`（this 命中失败时回落 global），与本项目 `GThis` 的
  动态链查找语义对应（见 mapping.md §4）。

### 1.2 "既是编译器又是可调用对象"

`tTJSInterCodeContext` 同时实现 `iTJSDispatch2`——编译产物本身就是一个可调用对象
（函数值/类值/属性 getter/setter）。序列化文件里的一个 context 就是一个这样的对象。

---

## 2. 指令编码

### 2.1 码字与操作数

- 每条指令 = 1~N 个 `tjs_int32` 码字；**操作码独占 code[0]**。
- 操作数是后续整字。**寄存器操作数和 DataArea 成员操作数在内存中是"字节偏移"**：
  ```cpp
  // tjsInterCodeGen.h:35-41
  TJS_TO_VM_REG_ADDR(x)  = x * sizeof(tTJSVariant)     // 寄存器/常量池下标 → 字节偏移
  TJS_TO_VM_CODE_ADDR(x) = x * sizeof(tjs_uint32)      // 代码位置 → 字节偏移
  ```
  即 codegen 写入的是 `索引 × sizeof(tTJSVariant)`；执行时 `*(tTJSVariant*)((char*)ra + code[i])` 取值。
  **文件里存的则是除算后的索引**（见 §5.4）。
- 跳转操作数（JF/JNF/JMP/ENTRY 的 catch 地址）是**相对本指令首字的字节偏移**，
  执行 `(*(char**)&code) += code[1]`。
- 助记符约定（下文沿用 TJS 反汇编风格的小写助记符，如 `gpd` = GPD）：
  `%n` 表示寄存器（编号可为负、数量不限，含义见 §1.1 帧布局），
  `*n` 表示常量池下标 `DataArea[n]`。例：`const %1, *3` 即 `ra[1] = DataArea[3]`。

### 2.2 属性访问四联变体

对 16 组"可作用于成员"的运算（INC/DEC/LOR/LAND/BOR/BXOR/BAND/SAR/SAL/SR/ADD/SUB/MOD/DIV/IDIV/MUL），
每种有 4 个变体（宏 `TJS_NORMAL_AND_PROPERTY_ACCESSER`，`tjsInterCodeGen.h:43`）：

| 后缀 | 含义 | 成员名来源 | 标志（TJS 派发 flag） |
|---|---|---|---|
| （无） | 直接对寄存器运算 | — | 0 |
| `PD` | Property Direct | 常量池 `DataArea[code[3]]` | 0 |
| `PI` | Property Indirect | 寄存器 `ra[code[3]]`（整数→按下标，否则转字符串按名） | 0 |
| `P` | Property（闭包整体） | 无成员名，作用于闭包本身 | 0 |

`S` 后缀（GPDS/SPDS/GPIS/SPIS）= 同型操作 + `TJS_IGNOREPROP`（穿透 property getter/setter）；
`E` 后缀（SPDE/SPIE）= + `TJS_MEMBERENSURE`（成员不存在则创建）；`SPDEH` 再加 `TJS_HIDDENMEMBER`。

属性变体通过 `iTJSDispatch2::Operation(ope, name, ...)` 一次派发（读成员→运算→写回原子完成），
对 property 对象（getter/setter）透明。

助记符中的对象成员引用有两种形式：`%obj.*name` 直接访问（成员名为常量池常量，对应 `D` 变体）、
`%obj.%name` 间接访问（成员名由另一寄存器的值决定，对应 `I` 变体）。例：

```
gpd %2, %1.*foo   ; %2 = %1.foo
gpi %3, %1.%bar   ; %3 = %1[bar]
```

---

## 3. 操作码全表（0~127）

### 3.1 基础与传送（1~3 字）

| 值 | 助记 | 字数 | 操作数 | 语义 |
|---|---|---|---|---|
| 0 | NOP | 1 | — | 空 |
| 1 | CONST | 3 | `[dst, daIdx]` | `ra[c1] = DataArea[c2]`（CopyRef） |
| 2 | CP | 3 | `[dst, src]` | `ra[c1] = ra[c2]` |
| 3 | CL | 2 | `[r]` | `ra[c1]` 清空为 void |
| 4 | CCL | 3 | `[startReg, count]` | **连续清空** `ra[c1] .. ra[c1]+c2`；⚠️ `code[2]` 是数量**不是寄存器**（`ContinuousClear`，`tjsInterCodeExec.cpp:1495`） |

### 3.2 标志寄存器与比较（2~3 字）

| 值 | 助记 | 字数 | 操作数 | 语义 |
|---|---|---|---|---|
| 5 | TT | 2 | `[r]` | `flag = bool(ra[c1])` |
| 6 | TF | 2 | `[r]` | `flag = !bool(ra[c1])` |
| 7 | CEQ | 3 | `[r1, r2]` | `flag = (ra[c1] == ra[c2])`（宽松相等 NormalCompare） |
| 8 | CDEQ | 3 | `[r1, r2]` | `flag = (ra[c1] === ra[c2])`（严格 DiscernCompare，对应 `===`/`!==`） |
| 9 | CLT | 3 | `[r1, r2]` | `flag = ra[c1] > ra[c2]`（注意：CLT 实为 GreaterThan，编译器已换序） |
| 10 | CGT | 3 | `[r1, r2]` | `flag = ra[c1] < ra[c2]`（同上，LittlerThan） |
| 11 | SETF | 2 | `[r]` | `ra[c1] = flag` |
| 12 | SETNF | 2 | `[r]` | `ra[c1] = !flag` |
| 13 | LNOT | 2 | `[r]` | `ra[c1] = !ra[c1]`（原地逻辑非） |
| 14 | NF | 1 | — | `flag = !flag` |

### 3.3 跳转（2 字，操作数为相对字节偏移）

| 值 | 助记 | 语义 |
|---|---|---|
| 15 | JF | `if (flag) 跳转` |
| 16 | JNF | `if (!flag) 跳转` |
| 17 | JMP | 无条件跳转 |

### 3.4 一元/转换（2 字，原地）

| 值 | 助记 | 语义 | 值 | 助记 | 语义 |
|---|---|---|---|---|---|
| 82 | BNOT | 按位非 | 93 | INV | invalidate 对象（置失效） |
| 89 | ASC | 首字符→字符码（`$"a"` → 97） | 94 | CHKINV | `ra[c1] = isvalid(ra[c1])`（原地写回 bool） |
| 90 | CHR | 字符码→单字符字符串（`#97` → "a"） | 95 | INT | 原地转 int |
| 91 | NUM | 数值规范化（int/real 归一） | 96 | REAL | 原地转 real |
| 92 | CHS | 变号（一元负号） | 97 | STR | 原地转 string |
| 83 | TYPEOF | 原地求 typeof 字符串 | 98 | OCTET | 原地转 octet |
| 86 | EVAL | 把字符串作为 TJS 表达式**即时编译执行**（`expr!` 后缀），this 取 `ra[-1]` | | | |
| 87 | EEXP | 同 EVAL 但作为内嵌表达式（embexp 展开形态） | | | |

### 3.5 属性访问型一元（3~4 字）

| 值 | 助记 | 字数 | 操作数 | 语义 |
|---|---|---|---|---|
| 84 | TYPEOFD | 4 | `[dst, obj, daIdx]` | `ra[c1] = typeof ra[c2][DataArea[c3]]` |
| 85 | TYPEOFI | 4 | `[dst, obj, memberReg]` | 同上，成员名取自寄存器 |
| 116 | DELD | 4 | `[dst, obj, daIdx]` | `ra[c1] = delete ra[c2][DataArea[c3]]`（返回 bool） |
| 117 | DELI | 4 | `[dst, obj, memberReg]` | 同上 |
| 88 | CHKINS | 3 | `[name, obj]` | `flag = (ra[c2] instanceof ra[c1])`（`ra[c1]` 是类名字符串） |

### 3.6 属性读写（3~4 字）

> ⚠️ **GPD 与 SPD 操作数顺序不同**：GPD 是 `[dst, obj, member]`，SPD 是 `[obj, member, src]`。

| 值 | 助记 | 字数 | 操作数 | 语义 |
|---|---|---|---|---|
| 103 | GPD | 4 | `[dst, obj, daIdx]` | `ra[c1] = ra[c2][DataArea[c3]]` |
| 110 | GPDS | 4 | 同上 | + `IGNOREPROP` 穿透 getter |
| 107 | GPI | 4 | `[dst, obj, memberReg]` | 成员名取自寄存器（整数→按数字下标，`PropGetByNum`） |
| 112 | GPIS | 4 | 同上 | + `IGNOREPROP` |
| 104 | SPD | 4 | `[obj, daIdx, src]` | `ra[c1][DataArea[c2]] = ra[c3]` |
| 105 | SPDE | 4 | 同上 | + `MEMBERENSURE` |
| 106 | SPDEH | 4 | 同上 | + `MEMBERENSURE|HIDDENMEMBER` |
| 111 | SPDS | 4 | 同上 | + `MEMBERENSURE|IGNOREPROP` |
| 108 | SPI | 4 | `[obj, memberReg, src]` | 同 SPI 族 |
| 109 | SPIE | 4 | 同上 | + `MEMBERENSURE` |
| 113 | SPIS | 4 | 同上 | + `MEMBERENSURE|IGNOREPROP` |
| 115 | GETP | 3 | `[dst, target]` | `ra[c1] = *ra[c2]`（经闭包无成员读，property 透明） |
| 114 | SETP | 3 | `[target, src]` | `*ra[c1] = ra[c2]`（经闭包无成员写） |

字符串/字节串目标是特判的：GPD/SPD 遇到 `ra[c2]`（或 SPD 的 `ra[c1]`）是 String/Octet 时走
`GetStringProperty`/`GetOctetProperty`（即 `str[i]` 下标访问）。

### 3.7 双目运算四联变体（18~81）

16 组，每组 4 个 opcode。**基型是破坏性的**：`ra[c1] = ra[c1] op ra[c2]`。

| 组起值 | 运算 | 基型字数 | PD/PI | P |
|---|---|---|---|---|
| 18 | INC（`++`） | 2 `[r]` | 4 `[dst, obj, member]` | 3 `[dst, target]` |
| 22 | DEC（`--`） | 2 | 4 | 3 |
| 26 | LOR（`\|\|`） | 3 `[r1, r2]` | 5 `[dst, obj, member, param]` | 4 `[dst, target, param]` |
| 30 | LAND（`&&`） | 3 | 5 | 4 |
| 34 | BOR | 3 | 5 | 4 |
| 38 | BXOR | 3 | 5 | 4 |
| 42 | BAND | 3 | 5 | 4 |
| 46 | SAR（`>>`） | 3 | 5 | 4 |
| 50 | SAL（`<<`） | 3 | 5 | 4 |
| 54 | SR（`>>>`） | 3 | 5 | 4 |
| 58 | ADD | 3 | 5 | 4 |
| 62 | SUB | 3 | 5 | 4 |
| 66 | MOD | 3 | 5 | 4 |
| 70 | DIV | 3 | 5 | 4 |
| 74 | IDIV（`\`） | 3 | 5 | 4 |
| 78 | MUL | 3 | 5 | 4 |

组内排布：`值 = 组起值 + 0/1/2/3`，依次为 基型 / `PD` / `PI` / `P`。
变体语义（以 ADDPD 为例，`OperatePropertyDirect`，`tjsInterCodeExec.cpp:1701`）：
`ra[c1] = ra[c2][DataArea[c3]] ADD ra[c4]`（c1 为 0 时丢弃结果）。

### 3.8 调用与对象（变长，见 §4）

| 值 | 助记 | 语义 |
|---|---|---|
| 99 | CALL | 调用 `ra[c2]` 闭包 |
| 100 | CALLD | 同 CALL，方法名来自常量池 `DataArea[c3]`（先做成员访问再调用） |
| 101 | CALLI | 同 CALLD，方法名来自寄存器 `ra[c3]` |
| 102 | NEW | 构造对象（等价 CALL 但以"类"语义派发） |

### 3.9 返回与异常

| 值 | 助记 | 字数 | 操作数 | 语义 |
|---|---|---|---|---|
| 118 | SRV | 2 | `[r]` | `result = ra[c1]`（写返回值槽） |
| 119 | RET | 1 | — | 返回（结束 ExecuteCode） |
| 120 | ENTRY | 3 | `[catchAddr, exReg]` | 进入 try：以 catch 地址（相对偏移）和异常接收寄存器压 try 栈，**嵌套执行** try 块（`ExecuteCodeInTryBlock`） |
| 121 | EXTRY | 1 | — | 弹出 try 栈 |
| 122 | THROW | 2 | `[r]` | 抛出 `ra[c1]` |
| 127 | DEBUGGER | 1 | — | 断点（挂有调试器则中断执行，否则等价 NOP） |

异常传播：try 块内异常被捕获 → 异常值写入 `ra[exReg]`（该寄存器是**字节偏移形式**的 code[2]）→
跳到 catch 地址继续执行（`tjsInterCodeExec.cpp:1344-1360`）；
若本帧 try 栈无匹配 ENTRY，异常继续向上层调用帧抛出，直至顶层交由宿主报告。

### 3.10 其他

| 值 | 助记 | 字数 | 操作数 | 语义 |
|---|---|---|---|---|
| 123 | CHGTHIS | 3 | `[func, thisObj]` | `ra[c1]` 闭包的 objthis 改为 `ra[c2]`（`incontextof` / 方法的 this 绑定） |
| 124 | GLOBAL | 2 | `[dst]` | `ra[c1] = global 对象` |
| 125 | ADDCI | 3 | `[obj, str]` | 向 `ra[c1]` 的 class-instance-info 追加 `ra[c2]`（类标记，供 `instanceof`） |
| 126 | REGMEMBER | 1 | — | 把当前 context 已收集的成员函数表（序列化里的 propcount 段）注册到 `ra[-1]` 对象（类构造过程） |

> `TranslateCodeAddress` 对未知 opcode `default: size = 1`（`tjsScriptBlock.cpp:886`）——
> 意味着**操作码表一旦错位，整个指令流解析都会错位**且不报错，转译器必须逐条对齐。

---

## 4. 调用指令的变长编码

`VM_CALL / VM_CALLD / VM_CALLI / VM_NEW` 的实参表是**变长**的（`CallFunction`，`tjsInterCodeExec.cpp:~2077`）：

```
CALL / NEW :  [op][res][func][argcount][args...]
CALLD/CALLI:  [op][res][func][name][argcount][args...]
```

| argcount | 含义 | 后续编码 |
|---|---|---|
| `>= 0` | 普通调用 | `argcount` 个寄存器字（每个是一个实参寄存器） |
| `-1` | 省略调用 `f(...)` | 无实参字；透传**当前函数收到的实参**（`args/numargs`） |
| `-2` | 展开调用 `f(a, *arr, *)` | `[written_count][(fatType, reg) 对 × written_count]`；`fatType`：0=fatNormal、1=fatExpand（`*arr` 数组展开）、2=fatUnnamedExpand（匿名 `*` 全展开） |

- `res == 0` 表示丢弃返回值（表达式语句）。
- 指令总字数：CALL/NEW 为 `4 + argcount`（普通），CALLD/CALLI 为 `5 + argcount`；
  `-1` 时分别固定 4/5 字；`-2` 时为 `5/6 + 2×written_count`（见 `tjsScriptBlock.cpp:732-778` 的长度推算）。

---

## 5. 序列化文件格式（compile-only 产物）

`TJSCompiler` 的 `tTJS::CompileScript` → `tTJSScriptBlock::Compile`
（`tjsScriptBlock.cpp:513`）→ `ExportByteCode`（460）输出如下格式。
**全文件小端**；所有多字节整数按本机序写出（跨平台兼容依赖两端同为小端）。

### 5.1 容器布局

```
偏移   内容                     说明
0      "TJS2"                   4B 魔数（BYTECODE_FILE_TAG 前半）
4      "100\0"                  4B 版本 1.0.0（后半）
8      filesize: i32            整个文件大小（loader 校验相等）
12     "DATA"                   4B（BYTECODE_DATA_TAG）
16     datasize: i32            = 4 + 数据区大小
20     <数据区>                  常量池，见 §5.2
       "OBJS"                   4B（BYTECODE_OBJ_TAG）
       objsize: i32             = 8 + 4 + 4 + Σ(每个 context: 4+4+体大小)
       toplevel: i32            顶层 context 在列表中的索引（无则 -1）
       count: i32               context 数量
       × count:
         "TJS2"                 4B（BYTECODE_CODE_TAG —— 与文件魔数同串！）
         ctxsize: i32           本 context 体大小
         <context 体>           见 §5.3
```

### 5.2 数据区（常量池，7 段分型）

`tjsConstArrayData::ExportBuffer`（`tjsConstArrayData.cpp:201-323`）按以下**固定顺序**写 7 段，
每段 `[count i32][元素…][补零到 4 字节对齐]`：

| 序 | 段 | 元素编码 |
|---|---|---|
| 1 | BYTE（int8） | 1B × count，补零到 4 对齐 |
| 2 | SHORT（int16） | 2B × count，补零到 4 对齐 |
| 3 | INTEGER（int32） | 4B × count |
| 4 | LONG（int64） | 8B × count |
| 5 | REAL（double） | 8B × count（位模式原样） |
| 6 | STRING | 每条 `[len i32][UTF-16LE 码元 × len]`，len 为奇数时补 2B 零 |
| 7 | OCTET | 每条 `[len i32][bytes × len]`，补零到 4 对齐 |

- 各段内按值**去重**（std::map 哈希；octet 线性比较）。
- **没有任何字符转义**——二义性完全由长度字段消除（`len` 是 UTF-16 **码元数**，不是字节数）。
- 整数按值域自动分型：`|v|≤127`→BYTE、`≤32767`→SHORT、`≤2^31-1`→INTEGER、否则 LONG。

### 5.3 context 体

每个 context（`tTJSInterCodeContext::ExportByteCode`，`tjsInterCodeGen.cpp:3639-3747`）：

```
parent                 i32   父 context 索引（顶层无，-1）
name                   i32   → 常量池 STRING 索引（-1 无）
contextType            i32   tTJSContextType（见下）
maxVariableCount       i32   ─┐
variableReserveCount   i32    │ 帧尺寸（§1.1）
maxFrameCount          i32   ─┘
funcDeclArgCount       i32   ─┐
funcDeclUnnamedArgArrayBase i32 │ 函数签名
funcDeclCollapseBase   i32   ─┘
propSetter             i32   → context 索引（property setter，-1 无）
propGetter             i32   → context 索引（-1 无）
superClassGetter       i32   → context 索引（-1 无）
srcposcount            i32   仅 outputdebug 时非 0
  [CodePos ×count]     i32   （先全部 CodePos）
  [SourcePos ×count]   i32   （再全部 SourcePos）
codecount              i32   = CodeAreaSize（码字数）
  code                 i16 × codecount   ★每个 32 位码字截断成 int16；奇数个补 1 个 i16 零对齐
datacount              i32   = DataAreaSize（常量槽位数）
  (type i16, idx i16) × datacount   每个常量槽指向 §5.2 的某一段内下标
scgettercount          i32
  addr                 i32 × count     SuperClassGetterPointer（代码地址，已索引化）
propcount              i32   成员函数表（REGMEMBER 注册用）
  (name i32, objidx i32) × count     name→STRING 索引；objidx→context 索引
```

`contextType`（`tjsInterCodeGen.h:95-104`）：0=ctTopLevel、1=ctFunction、2=ctExprFunction（函数表达式）、
3=ctProperty、4=ctPropertySetter、5=ctPropertyGetter、6=ctClass、7=ctSuperClassGetter。

**注意**：序列化**不包含** Namespace（局部变量名→寄存器映射）。运行期按名字访问局部变量
（`eval`）在字节码形态下不可用，这与原版行为一致。

### 5.4 码字的两种形态与地址换算

| 操作数 | 内存态（执行时） | 文件态（序列化） |
|---|---|---|
| 寄存器 / DataArea 成员 | `索引 × sizeof(tTJSVariant)`（字节偏移） | `÷ sizeof(tTJSVariant)`（索引） |
| 跳转目标 / catch 地址 | `索引 × 4`（字节偏移，相对指令首字） | `÷ 4`（字索引） |

导出前调 `tTJSScriptBlock::TranslateCodeAddress`（`tjsScriptBlock.cpp:575-894`）除算；
加载后 `tTJSByteCodeLoader::TranslateCodeAddress`（`tjsByteCodeLoader.cpp:363`）乘回。
**两份代码按同一张指令长度表逐条步进，长度或操作码表不一致即整体错位。**

文件里码字只有 16 位（int16）——因为文件态是索引（除算后），脚本规模内不会溢出；
但内存态偏移在 64 位平台 `× sizeof(tTJSVariant)` 后恢复为 32 位。

### 5.5 常量槽类型 tag（DataArea 条目）

`type` 字段（`tjsConstArrayData.h:44-54`）：

| tag | 类型 | idx 指向 | 说明 |
|---|---|---|---|
| 0 | VOID | — | 恒 0 |
| 1 | OBJECT | — | **仅允许 null 闭包**（idx=0）；其他对象不可序列化 |
| 2 | INTER_OBJECT | context 索引 | 值是同文件内另一个 context（函数/类/属性值） |
| 3 | STRING | STRING 段下标 | |
| 4 | OCTET | OCTET 段下标 | |
| 5 | REAL | REAL 段下标 | |
| 6 | BYTE | BYTE 段下标 | |
| 7 | SHORT | SHORT 段下标 | |
| 8 | INTEGER | INTEGER 段下标 | |
| 9 | LONG | LONG 段下标 | |

### 5.6 加载流程（loader 行为，转译器需对齐）

`tTJSByteCodeLoader`（`tjsByteCodeLoader.cpp`）：

1. 校验 8B 头 `"TJS2" "100\0"` 与 filesize；
2. 按 §5.2 顺序读 7 段常量池（`ReadDataArea`，65-146；字符串经 `TJSMapGlobalStringMap` 驻留）；
3. 逐个读 context：**第一遍**先建出全部 `tTJSInterCodeContext` 骨架（记录 parent/setter/getter 等
   索引），把 code 的 i16 码字展开回 int32 并 `TranslateCodeAddress` 乘回内存形态；
4. **第二遍**回填引用：INTER_OBJECT 常量 → context 指针；propcount 成员表 → 注册到 context；
   propSetter/propGetter/superClassGetter → 建立关联；
5. toplevel context 即可执行入口（`tTJS::LoadByteCode`，`tjs.cpp:497`）。

---

## 6. 类的表示（ctClass context）

类在序列化里就是一个 `contextType=ctClass` 的 context，其代码体在**执行**时完成类构造：

- `REGMEMBER`：把 propcount 成员表（方法）注册到 `ra[-1]`（正在构建的类对象）；
- `ADDCI`：追加 class-instance-info（`instanceof` 的判定依据）；
- 方法/属性值是 INTER_OBJECT 常量（加载时已回填为 context 对象）；
- `superClassGetter` 是一个特殊 context，在首次取 `super` 时惰性执行返回父类。

转译时对 ctClass context 的处理是最大难点之一，方案见 [mapping.md](mapping.md) §7。
