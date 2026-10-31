# Ciallang 指令集规范（TAC + 扁平字节码）

> 本项目（Ciallang）的两级指令集：**TAC 三地址码**（编译产物、IR 载体）与
> **扁平 u8 字节码**（TAC 的紧凑编码，按需惰性生成）。
> 与旧版 TJS2 指令集的对照见 [mapping.md](mapping.md)。
>
> 事实来源：
> - TAC 指令定义：`src/gen/Instruction.hpp`（X-macro）、`Instruction.cpp`（dump）
> - 扁平字节码：`src/vm/Bytecode.hpp`（X-macro）、`Bytecode.cpp`（encode）、`src/vm/VMState.cpp`（dispatch）
> - Chunk 结构：`src/vm/Chunk.hpp`、`src/vm/Constant.hpp`
> - 旧版 TJS2 指令集参考：[tjs2-bytecode.md](tjs2-bytecode.md)

---

## 1. 两级结构总览

```
AST ──IRGenerator──▶ TAC 指令序列（95 种，Instruction 对象）
                        │  存于 Chunk::_instructions
                        ▼  Chunk::toBytecode()（幂等、惰性）
                     扁平字节码 Vec<u8>（95 种 VmOpCode，与 TAC 一一对应）
                        │  存于 Chunk::_code
                        ▼
                     VMState::run() switch 派发
```

- 指令语义由 `src/vm/VMState.cpp` 中的 `dispatchXXX` 实现。
- `Instruction` 子类只作为 IR 载体与 dump 来源保留。

## 2. 执行模型

- **寄存器**：`%n`（`u16`，`Register` 强类型），帧内 Value 槽，由 `FastRegisterPool` 分配
  （4096 个 Value 槽分块、2 的幂移位寻址）。`Chunk::_registerCount` 决定帧窗口大小。
- **零标志 `_zf`**：VM 级单 bool（`VMState::_zf`）。`Test`、`Cmp*`、`CompareTest` 写入，`JmpE`/`JmpNE` 消费。
  ⚠️ 与 TAC 的 3 操作数比较指令（EQ/LT 等把 bool 写进寄存器）是**两套机制**：
  3 op 比较产出值，`Test`+`JmpNE` 做分支。
- **实参栈**：实参经 `Push` 压入寄存器池顶部；`Call` 触发 `Function::call`：
  不足 arity 补 void → `allocCallFrame`，帧 `_sp` 回退 `arity`，使**被调帧的 %0..%arity-1
  与压入的实参区重合（%0 = 第 1 个实参）**。IRGenerator 为参数依次分配 `%0..` 起的寄存器
  （`src/gen/IRGenerator.cpp` 的 `generateFuncMeta`）。
  **CallV 直写布局**（性能专项 2026-10-28）：Function 被调者跳过池顶中转——一次分配
  被调帧窗口（`argWindowDirect` 布局，`_sp` = 分配基址不回退 arity），实参寄存器直接
  拷入 %0..%arity-1（缺位补 void、超额/hasCollapse 备份窗口尾区，语义与 `Function::call`
  一致）；其余可调用对象回落压栈 + 虚 `call` 旧约定。
- **返回**：`Ret` 把寄存器值写入调用方 `CallFrame::ret`（即 Call 的 dst 寄存器）并弹帧。
- **this**：`CallFrame::thisObj`；`Function::call` 设为函数对象的 `thisObj` 字段（可被
  `ChgThis`/`GProp` 的方法绑定改写）。`GThis`/`DThis` 做名字查找（thisObj → closure 局部 → global）。

## 3. TAC 指令全表（95 条）

枚举值 = 表中顺序（X-macro `CIAL_TAC_OPCODE_ENUMS`）。操作数记号：
`%r`=寄存器、`@l`=标签（TAC 指令下标）、`*c`=常量池下标、`imm`=立即整数、`atom`=驻留名。

### 3.1 空操作与调试

| # | 指令 | 操作数 | 语义 |
|---|---|---|---|
| 0 | NOP | — | 空 |
| 1 | Debugger | — | dump 寄存器/局部变量/常量/指令 + DEBUG_BREAK |

### 3.2 栈与调用

| # | 指令 | 操作数 | 语义 |
|---|---|---|---|
| 2 | Push | `%src` | 实参压栈（寄存器池顶） |
| 3 | PopN | `imm` | 弹出 n 个池顶临时槽；编码为 ZigZag + uLEB128（`Bytecode.cpp`），派发调用 `pop(n)`（`VMState.cpp`） |
| 10 | Ret | `%src` | 返回值并结束当前调用帧 |
| 57 | Call | `%dst, %func, imm` | `func->call(state, dst, argc)`；实参需先 Push；DispatchRet::Call 换帧 |

### 3.3 对象/上下文装入

| # | 指令 | 操作数 | 语义 |
|---|---|---|---|
| 4 | Global | `%dst` | 装入 global 对象 |
| 5 | Super | `%dst` | 装入 thisObj 的父类（DataObject::getSuperClass） |
| 6 | This | `%dst` | 装入 thisObj |
| 22 | ChgThis | `%func, %this` | 把函数/原生函数/property 闭包的绑定 this 改为第二操作数（TJS 的 chgthis / incontextof） |

### 3.4 标志与控制流

| # | 指令 | 操作数 | 语义 |
|---|---|---|---|
| 7 | Test | `%src` | `_zf = src.asBool()` |
| 11 | Jmp | `@l` | 无条件跳（TAC 下标） |
| 12 | JmpE | `@l` | `_zf` 为真跳 |
| 13 | JmpNE | `@l` | `_zf` 为假跳 |
| 8 | Inv | `%src` | invalidate 对象（DataObject::invalidate） |
| 9 | Throw | `%src` | 抛异常值（触发 unwind） |
| 23 | ChkInv | `%src, %dst` | `dst = isvalid(src)` |
| 24 | EvalStr | `%src, %dst` | 后缀 `!` 即时求值：src 字符串化为 TJS 表达式编译执行（嵌套双帧 run），结果写 dst；空串/非串原值不变（tjs.y T_EVAL → VM_EVAL 对应物） |

### 3.5 常量装入

| # | 指令 | 操作数 | 语义 |
|---|---|---|---|
| 27 | Load | `%dst, *c` | 装入常量池条目（Real/Atom/FuncMeta/ClassMeta/PropMeta/Octet → Value） |
| 28 | LoadImm | `%dst, imm` | 装入立即整数 |

### 3.6 名字访问（三层作用域）

| # | 指令 | 操作数 | 语义 |
|---|---|---|---|
| 29 | DGlobal | `atom, %src` | `global[atom] = src`（已有 Property 则触发 setter） |
| 30 | GGlobal | `atom, %dst` | `dst = global[atom]`（Property 触发 getter） |
| 31 | GThis | `atom, %dst` | 名字查找：thisObj 成员 → closure 帧局部变量 → global |
| 32 | DThis | `atom, %src` | 同上方向写入 |
| 33 | GLocal | `%src, %dst` | 寄存器间纯复制（不触发 Property） |
| 34 | DLocal | `%src, %dst` | 寄存器间纯复制（不触发 Property） |

### 3.7 类型转换（原地写 dst）

| # | 指令 | 操作数 | 语义 |
|---|---|---|---|
| 14 | ToInt | `%src, %dst` | 转整数 |
| 15 | ToReal | `%src, %dst` | 转实数 |
| 18 | ToString | `%src, %dst` | 转字符串 |
| 19 | TypeOf | `%src, %dst` | 把类型名字符串写入 dst |
| 20 | LNot | `%src, %dst` | 逻辑非 |
| 21 | ChgSign | `%src, %dst` | 变号 |
| 16 | Asc | `%src, %dst` | `#s`：字符串化后取首字符码（octet 报错） |
| 17 | Chr | `%src, %dst` | `$n`：字符码转 1 字符字符串 |
| 25 | ChkIns | `%obj, %name` | `obj = (obj instanceof name)`（基础类型按类型名匹配：Number 覆盖 Integer/Real；结果原地写回 obj 槽） |
| 26 | TestVoid | `%src, %dst` | `dst = (src is void)`；函数默认参数守卫用 |
| 64 | DerefGet | `%prop, %dst` | `*p` 强制属性访问读：Property 调 getter（嵌套 run），非 Property 报错 |
| 65 | DerefSet | `%prop, %src` | `*p = v` 强制属性访问写：调 setter（压帧 → Call 接管），非 Property 报错 |
| 66 | NewArray | `%dst, sLEB count` | `[...]` 字面量：新建 count 长度 Array（元素 void，随后逐个 SetIndex） |
| 67 | NewDict | `%dst` | `%[...]` 字面量：新建空 Dictionary |
| 68 | GetIndex | `%obj, %idx, %dst` | `a[i]` 读：数组整数下标（负=从尾，越界报脚本异常）/字典键（字符串化）/字符串字符（旧 charAt 语义）/对象成员名 |
| 69 | SetIndex | `%obj, %idx, %src` | `a[i] = v` 写：数组越界自动扩展补 void；字典键写入 |
| 70 | WithPush | `%src` | with 语句：压入 with 上下文栈（对象挂 Runtime handle 保 GC） |
| 71 | WithPop | | 弹出 with 上下文栈（块结束配对；异常穿越随帧截断） |
| 72 | GetWith | `%dst` | 裸词链 `.member` 基座：取 with 栈顶对象，空栈得 global（TJS2 顶层裸词链语义） |
| 73 | NewRegExp | `%dst, %pattern(ConstIdx)` | `/pattern/` 字面量：String 常量（模式）→ RegExp 对象（std::regex ECMAScript 编译） |

### 3.8 算术/位/比较（三操作数，dst ← src1 op src2）

TAC 层**非破坏性**；扁平字节码层同样保持三寄存器非破坏性编码（§5.3）。

| # | 指令 | 语义 | # | 指令 | 语义 |
|---|---|---|---|---|---|
| 35 | Add | 加 | 45 | BrShift | `>>`（算术右移） |
| 36 | Sub | 减 | 46 | BurShift | `>>>`（无符号右移） |
| 37 | Mul | 乘 | 47 | EQ | 宽松相等（bool 写 dst） |
| 38 | Div | 除 | 48 | NEQ | 宽松不等 |
| 39 | Idiv | 整除 `\` | 49 | AbsEQ | 严格相等 `===` |
| 40 | Mod | 取余 | 50 | AbsNEQ | 严格不等 |
| 41 | BXor | `^` | 51 | LT | `<` |
| 42 | BOr | `\|` | 52 | LE | `<=` |
| 43 | BAnd | `&` | 53 | GT | `>` |
| 44 | BlShift | `<<` | 54 | GE | `>=` |
| 55 | LAnd | 逻辑与（值语义） | 56 | LOr | 逻辑或（值语义） |

### 3.9 属性访问（成员名取自寄存器字符串）

| # | 指令 | 操作数 | 语义 |
|---|---|---|---|
| 58 | GProp | `%obj, %name, %dst` | `dst = obj[name]`；特判 GlobalObject/ClassObject（方法绑定 this；构造器名返回类对象）/非对象先查原生属性表（`str.length`）再查方法表 |
| 59 | DProp | `%obj, %name, %src` | `obj[name] = src`；槽位为 Property 则调 setter（压帧 → Call 接管），普通成员纯写 |
| 60 | DelProp | `%obj, %name, %dst` | `dst = delete obj[name]`（仅删实例自有成员，TJS2 DeleteMember 语义） |
| 61 | GPropRaw | `%obj, %name, %dst` | `&obj[name]`：取成员本体，property 不触发 getter（旧版 GPDS） |
| 62 | GRefRaw | `%atom, %dst` | `&name`：取 this/global 槽位本体，不触发 getter（旧版 GPDS 符号形态） |
| 63 | DPropRaw | `%obj, %name, %src` | `&obj[name] = src`：绕过 setter 直接写槽位（旧版 SPDS），& 作赋值左值 |

⚠️ 与 TJS 不同：Ciallang 的 GProp/DProp 原本**只有 indirect 形态**（名字来自寄存器）。
性能专项（2026-10-28）补齐 **direct 形态**：`GPropC/DPropC/GPropRawC/DPropRawC/DelPropC`
（§3.10）成员名以 u32 Atom 内联进指令流，编译期直引、dispatch 免再 intern；
寄存器形态保留给动态成员名与整数成员名（旧版 GPI/PropGetByNum，数组下标）。
IGNOREPROP 由 *Raw 指令承担；MEMBERENSURE 依赖对象 setProp 的创建语义，HIDDENMEMBER 无独立编码，见 [转译边界](mapping.md#8-已知差异与待修复项)。

**property 触发协议**：GProp/DProp/GThis/GGlobal/DThis/DGlobal 在槽位为 Property 时
调 getter/setter；GLocal/DLocal（局部变量=寄存器拷贝，TJS2 VM_CP 语义）与
*Raw 系列永不触发——这是 `&` 引用能作为一等值传递的前提。

### 3.10 扩展指令（TJS 字节码转译与性能批次）

> 全部编号按 `src/gen/Instruction.hpp` 的 `CIAL_TAC_OPCODE_ENUMS` 零起点顺序，
> 与 `src/vm/Bytecode.hpp` 的扁平操作码一一对应。本节也归纳前述部分扩展指令。

| # | 指令 | 操作数 | 语义 |
|---|---|---|---|
| 57 | Call | `%dst, %func, argc` | 调用对象（Function/Class/Property 之外的通用入口，实参已 Push） |
| 74 | CallV | `%dst, %func, argc, %r1..%rn` | 变长实参调用。Function 被调者走实参直写（§2 执行模型）：一次分配被调帧窗口、实参寄存器直接落 %0..；其余对象压栈 + 虚 call 回落 |
| 75 | CallOmit | `%dst, %func` | 省略透传（TJS `f(...)`）：转发本帧 actualArgc 个实参（CallFrame 记录） |
| 76 | CallExpand | `%dst, %func, unnamedBase, (type,%r)*` | 展开调用（TJS `f(1, arr*, *)`）：Normal/Expand(Array)/UnnamedExpand 条目表 |
| 77 | CollapseArgs | `%dst, base` | collapse 参数（TJS `function f(a, b*)`）：实参 %base.. 收拢成 Array |
| 64 | DerefGet | `%prop, %dst` | `*p` 强制属性访问读（调 getter，旧版 GETP） |
| 65 | DerefSet | `%prop, %src` | `*p = v` 强制属性访问写（调 setter，旧版 SETP） |
| 87 | GPropC | `%obj, atom, %dst` | `dst = obj.member`，成员名编译期 Atom 直引（性能专项 2026-10-28，语义同 GProp） |
| 88 | DPropC | `%obj, atom, %src` | `obj.member = src`（同 DProp：Property 槽触发 setter 压帧 → Call） |
| 89 | GPropRawC | `%obj, atom, %dst` | `&obj.member`（同 GPropRaw，不触发 getter） |
| 90 | DPropRawC | `%obj, atom, %src` | `&obj.member = src`（同 DPropRaw，绕过 setter） |
| 91 | DelPropC | `%obj, atom, %dst` | `delete obj.member`（同 DelProp） |
| 66 / 67 | NewArray / NewDict | `%dst[, count]` | `[...]` / `%[...]` 字面量 |
| 68 / 69 | GetIndex / SetIndex | `%obj, %idx, %dst/%src` | 通用下标读写（数组负下标/越界扩展/字典键/字符串字符） |
| 70 / 71 / 72 | WithPush / WithPop / GetWith | `%src` / — / `%dst` | with 上下文栈与 `.member` 裸词链 |
| 73 | NewRegExp | `%dst, *pattern` | `/pattern/flags` 字面量 |
| 78 / 79 / 80 / 81 | CmpEQ / CmpDEQ / CmpLT / CmpGT | `%src1, %src2` | **比较直写 _zf**（M3：无 dst，配 JmpE/JmpNE 直连分支，省标志物化） |
| 82 / 83 | Inc / Dec | `%r` | 自增/自减单指令（int/real，TJS increment 语义） |
| 84 | BNot | `%src, %dst` | 按位非 `~`（旧版 BNOT） |
| 85 | ToNumber | `%src, %dst` | 数值规范化（旧版 NUM：字符串解析、void→0） |
| 86 | ToOctet | `%src, %dst` | 转 octet（旧版 OCTET：void→空 octet、其余报错） |
| 24 | EvalStr | `%src, %dst` | 后缀 `!` 即时求值（见 3.4 表 #24） |
| 92 | CompareTest | `kind, %lhs, %rhs, %dst` | 融合比较与紧随的 Test，保留 `%dst` 布尔值并同步 `_zf`；不跨 CFG 基本块边界（`src/gen/Optimizer.cpp`） |
| 93 | GMethodC | `%obj, atom, %func, %receiver` | 调用前读取成员：原生方法不分配绑定对象，函数和接收者分开存入寄存器；其他成员走原有属性读取协议，receiver 为 void |
| 94 | CallMethodV | `%receiver, %dst, %func, argc, %args...` | receiver 非 void 时按原生调用约定传递 objthis；否则复用 CallV 的脚本函数直写或通用调用。成员读取先于实参求值，提取为一等值的方法仍用 GPropC 绑定（`src/gen/IRGenerator.cpp`、`src/types/Function.cpp`） |

## 4. Chunk 结构（`src/vm/Chunk.hpp`）

| 成员 | 说明 |
|---|---|
| `_instructions` | `Vec<Box<inter::Instruction>>`，TAC 序列（`emit/repl/inst`） |
| `_constants` | 常量池：`Vec<Constant>`；**index 0 恒为 Void**；去重；上限 u16（65535） |
| `_throwHandlers` | `Vec<ThrowHandler>`：`{tryStart, tryEnd, exValueReg}` 线性表，**TAC 下标空间**（**非** TJS 的 entry/extry 栈） |
| `_bcThrowHandlers` | 同上但已换算到**扁平字节码 pc 空间**（`toBytecode()` 时生成，`run` 的 unwind 使用） |
| `_registerCount` | 帧寄存器需求（CallFrame 据此开窗） |
| `_code` + `_isBytecode` | 惰性扁平字节码（`toBytecode()` 幂等） |

常量池类型（`src/vm/Constant.hpp` 的 `ConstantType`）：`None(0)` / `Real(1)` / `Atom(2)` / `FuncMeta*(3)` /
`ClassMeta*(4)` / `PropMeta*(5)` / `SharedPayload<Octet>*(6)`。

- `FuncMeta`：`name / arity / chunk / localVars`（localVars 同时是"参数+局部变量"的名字→寄存器表，
  供 GThis 动态查找与调试 dump）。
- `ClassMeta`：`className / arity / constructor / extends(按名) / memberShapeMetas + memberMetas`
  （slot 化成员表；继承按名字查找，与 TJS2 行为一致）。
- `PropMeta`：`setFunc / getFunc`。

**无磁盘序列化**：当前 Chunk 只在内存；TJS 字节码加载/转译是唯一的二进制入口（已实现，见 `src/tjsbc/TjsTranslator.cpp`）。

## 5. 扁平字节码（`run()` 路径）

### 5.1 操作码（95 个，`VmOpCode : u8`）

与 TAC 指令一一对应、编号一致（`CIAL_BYTECODE_OPCODE_ENUMS` 与
`CIAL_TAC_OPCODE_ENUMS` 是两份顺序相同的 X-macro 序表，见对应头文件）。编码器覆盖全部 95 条，无占位断言。

### 5.2 立即数编码

- **uLEB128**：7bit/字节 + 最高位 0x80 延续（`Bytecode.cpp`）。u64 最多 10 字节，
  第十字节只能保留最低一位有效载荷；超出位宽或给定字节上限仍未终止均抛异常，
  失败时 pc 不推进（回归：`tests/bytecode_test.cpp`）。
- **sLEB128** = ZigZag 变换 + uLEB128：`zigzag(v) = (v<<1) ^ (v>>63)`。
- **定宽读写** `write<T>/read<T>`：T 为 2/4/6/8 字节时 `memcpy` 原样（小端跟随宿主）。
- 操作数宽度约定：寄存器 `u16`、Atom `u32`（Atom.v 是顺序索引，远小于 2^32）、
  跳转目标 `u32`（编译期回填的绝对偏移）。

### 5.3 指令编码表（95 条全覆盖）

| 类别 | 指令 | 编码 |
|---|---|---|
| 无操作数 | NOP / Debugger | `op(1B)` |
| 单寄存器 | Push / Test / Inv / Throw / Ret / Global / Super / This / Inc / Dec | `op + u16` |
| 栈回收 | PopN | `op + sLEB128 count` |
| 跳转 | Jmp / JmpE / JmpNE | `op + u32 目标`（占位 + 回填，§5.4） |
| 二寄存器 | ToInt/ToReal/ToString/TypeOf/Asc/Chr/LNot/ChgSign/ChkInv/EvalStr/TestVoid/DerefGet/DerefSet/BNot/ToNumber/ToOctet（`src,dst`）、ChgThis（`函数,新this`）、ChkIns（`dst,src`）、GLocal/DLocal（`src,dst`） | `op + u16 + u16` |
| 装载扩展 | NewArray（`dst, sLEB count`）/ NewDict（`dst`）/ WithPop / WithPush/GetWith（`src/dst`） | `op + u16 + sLEB` / `op + u16` / `op` / `op + u16` |
| 正则 | NewRegExp（`dst, constIdx`） | `op + u16 + u16` |
| 调用 | CallV（`dst, func, argc, r1..rn`；Operand 三槽装不下寄存器列表，手写子类 + emitRaw） | `op + u16 + u16 + uleb(argc) + u16×n` | 
| 省略调用 | CallOmit（`dst, func`） | `op + u16 + u16` |
| 展开调用 | CallExpand（`dst, func, unnamedBase, entries`） | `op + u16 + u16 + uleb(base) + uleb(entries数) + (u8 type + u16 reg)×n` |
| 收拢实参 | CollapseArgs（`dst, base`） | `op + u16 + uleb(base)` |
| 标志比较 | CmpEQ/CmpDEQ/CmpLT/CmpGT | `op + u16 src1 + u16 src2` |
| 装载 | Load（`dst, constIdx`）、LoadImm（`dst, imm`） | `op + u16 + u16` / `op + u16 + sLEB128` |
| 名字访问 | DGlobal/GGlobal/GThis/DThis/GRefRaw | `op + u32 atom + u16 reg` |
| 成员名直引 | GPropC/DPropC/GPropRawC/DPropRawC/DelPropC（`obj, atom, dst/src`） | `op + u16 obj + u32 atom + u16 dst/src` |
| 比较测试融合 | CompareTest | `op + u8 kind + u16 lhs + u16 rhs + u16 dst`；kind 0..7 依次 EQ/NEQ/AbsEQ/AbsNEQ/LT/LE/GT/GE |
| 原生方法读取 | GMethodC | `op + u16 obj + u32 atom + u16 func + u16 receiver` |
| 接收者分离调用 | CallMethodV | `op + u16 receiver + u16 dst + u16 func + uleb(argc) + u16×n` |
| 算术/位/比较 | Add..LOr 全部 22 条（§3.8） | `op + u16 src1 + u16 src2 + u16 dst`（**三寄存器非破坏性**） |
| 调用/属性 | Call（`dst, func, sLEB argc`）、GProp/DProp/DelProp/GPropRaw/DPropRaw（`obj, name, dst/src`）、GetIndex/SetIndex（`obj, idx, dst/src`） | 见括号内顺序 |

> 历史包袱说明：早期版本曾把三操作数算术展开为"DLocal 补拷贝 + 破坏性双操作数"
> （且在 `r1 == r3` 时算错），现已改为原生三寄存器编码，展开逻辑删除。

### 5.4 跳转回填机制

`Bytecode::compile`（`Bytecode.hpp`）单遍扫描：

1. 每条 TAC 指令编码前记录 `tacStartPos[i] = code.size()`（含末尾哨兵 `tacStartPos[N]`）；
2. 跳转指令写 4B 零占位，并把 `(占位符位置, 目标TAC下标)` 追加进 `jmpPatches` 列表；
3. 扫描结束后统一回填 `code[占位符] = tacStartPos[目标]`（**绝对字节偏移**）。

多条跳转可指向同一目标（switch 的多个 break 等），因此回填信息是**列表**——
早期 1:1 map 会静默丢失除最后一条外的全部修补。

### 5.5 try 区间表换算

`_throwHandlers`（TAC 下标空间）在 `toBytecode()` 时换算为 `_bcThrowHandlers`
（字节码偏移空间，区间 `[tacStartPos[start], tacStartPos[end])`），
`unwind()` 只查后者。

### 5.6 run 主循环（`VMState.cpp`）

```
run(entryStackTop = 进入时栈深):
FETCH_NEW_FRAME:
  取当前 CallFrame；chunk->toBytecode()（幂等惰性）
  while pc < bcSize:
    switch ip[pc++] → dispatchXXX(...)   // 返回 Continue / Call / Return
    Call   → goto FETCH_NEW_FRAME        // 换到新帧
    Return → break                        // 外层弹帧回到调用方
    Throw pending → 同步 cur->pc=pc → unwind()（查 _bcThrowHandlers，
                   可能逐帧退栈）→ 帧变了则 goto FETCH_NEW_FRAME，否则 pc=_curFrame->pc
  帧结束时: _stackTop > 1 → freeCallFrame；
            若 _stackTop < entryStackTop → return（嵌套进入：property getter /
            类初始化 / 构造函数的目标帧已完成，交还原调用方）
```

注意三个"压帧不执行"的点都返回 `DispatchRet::Call` 让主循环接管：
成员写入指令（如 `DThis/DGlobal/DProp`）触发属性 setter、`Inv` 触发 finalize、`Call` 本身。
`DLocal` 仅复制寄存器，不调用 setter。
（`Property::invokeGet` 与 `Class.cpp` 的初始化/构造走**嵌套 run**，自释放帧。）

## 6. 异常模型

- `Throw` TAC → `VMState::throwException`（挂起态 + GC 根保护异常值）→ `unwind()`：
  1. 当前帧 `chunk->findBcThrowHandler(pc)` 线性查字节码 try 表；
  2. 命中：异常值写入 `exValueReg`，`setPC(tryEnd)`，继续执行 catch 块；
  3. 未命中：向 ret 寄存器写 void、弹帧重试；栈空则把异常抛给宿主。
- IRGenerator 为 try-catch 生成 `{tryStart, tryEnd, exValueReg}` 区间表。
- 与 TJS 的 ENTRY/EXTRY try 栈语义等价（区间 vs 栈式记录的差异对转译无影响，见 mapping.md §5）。
