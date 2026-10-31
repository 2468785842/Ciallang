# Ciallang 架构设计

> 本文描述本项目的编译/执行流水线、模块划分与关键设计决策，并与旧版 TJSCompiler 逐段对照。
> 指令集细节见 [bytecode/ciallang-bytecode.md](bytecode/ciallang-bytecode.md)；
> 转译路线见 [bytecode/mapping.md](bytecode/mapping.md)。

---

## 1. 总体流水线

```
                    ┌─────────────────────────── 编译期（每次 eval） ───────────────────────────┐
 .tjs 源码文件 ─▶ SourceFile（加载/行切分，UTF-8）
                    │
                    ▼
              PreProcessor        @set / @if / @endif 条件编译（预处理变量栈）
                    │
                    ▼
              Lexer               手写 DFA 词法；模板字符串状态机（@"..." 内嵌表达式）
                    │  Token 流
                    ▼
              Parser              Pratt 优先级（decl/stmt/prefix/infix 四类子解析器表）
                    │  AST（AstNode 树，X-macro 节点枚举）
                    ▼
              IRGenerator         AST::generateBytecode 回调式发射；寄存器分配（自由表）；
                    │            作用域栈 / break-continue 回填 / FuncMeta、ClassMeta 生成
                    ▼
              Chunk               TAC 指令序列 + 常量池 + try 表 + 寄存器数
                    └──────────────────────────────────────────────────────────────────────┘
                    │
                    ▼
              Chunk::toBytecode()   惰性生成 Vec<u8>，同时换算跳转、异常表与行号
                    │
                    ▼
              VMState::run()        派发字节码指令，维护调用帧与异常展开
```

源码和 TJS2 字节码转译均生成 Chunk，执行器与异常处理共用 `src/vm/VMState.cpp`。

## 2. 模块职责

### 2.1 `src/common/` — 基础设施

| 文件 | 职责 |
|---|---|
| `Result.hpp` / `ResultMessage.hpp` | 编译诊断容器（info/warning/error/data 多条收集） |
| `Ret.hpp` | `ErrCode`/`Err` 异常 + `Ret<T>`（值或错误二选一） |
| `SourceFile` / `SourceLocation` | 源码加载、行切分、行列位置 |
| `UTF8` | 手写 UTF-8 解码器（Go 风格 DFA） |
| `Hash.hpp` / `Defer.hpp` / `Colorizer` | 工具 |

### 2.2 `src/parser/` — 前端

- `PreProcessor`：`@set(变量=值)` / `@if(条件)` / `@endif` 条件编译（对标原版 `tjsCompileControl` + `tjspp.y`，指令同名）。
- `Lexer`：手写词法器 + `OperatorTrie` 多字符运算符匹配 + 模板字符串状态机。
- `Parser`：**Pratt 优先级解析器**（`Parser.hpp` 的 `Precedence`），四类子解析器注册表——
  与 TJSCompiler 的 Bison 生成表采用不同实现路线；对照文法的来源见 [grammar/README.md](grammar/README.md)。
- `ast/`：`AstNode` 基类 + X-macro `CIAL_AST_NODE_ENUMS`；节点自带
  `generateBytecode(IRGenerator*, OptReg&)` 虚方法（**AST 直接驱动 IR 生成**，`IRGenerator.hpp`）。

### 2.3 `src/gen/` — 中端

- `IRGenerator`：核心类。AST→TAC 发射、寄存器分配（`_freeRegisters` 自由表 + 递增索引）、
  作用域栈（`_localVars` 即 FuncMeta 局部变量表）、循环 break/continue 标签回填、
  函数/类/属性的元数据（FuncMeta/ClassMeta/PropMeta）生成。
- `Instruction.hpp/cpp`：**95 条 TAC 指令**（X-macro 生成类族及变长操作数子类），仅保留 dump；
  编码由 `src/vm/Bytecode.cpp` 完成，执行统一走 `VMState::run()`。
- `Register/Label/ConstIdx`：`%n`、`@n`、`*n` 强类型操作数。
- `ControlFlowGraph` + `Optimizer`：CFG/BasicBlock、整数常量折叠、死 LoadImm 清理与比较/Test 融合，
  由 `src/gen/Optimizer.cpp` 的 `optimizeDefaultChunk()` 组装，通过 `IRGenerator` 构造参数注入并传递给嵌套 Chunk；NOP 压缩同步重映射跳转、try 表与行号。

### 2.4 `src/vm/` — 执行引擎

- `VMState`：解释器核心。字节码派发与调用帧调度；
  全局/this/闭包名字解析（`globalHas/getThis/setThis/getUpVal`）；异常 `unwind()`。
- `Chunk`：编译产物容器（指令/常量池/ThrowHandler 表/寄存器数/惰性 `_code`）。
- `Bytecode`：TAC→u8 编码器（uLEB128/sLEB128+ZigZag/定宽小端；跳转绝对地址回填）。
- `CallFrame`：闭包链（`closure` 指向定义帧）/chunk/funcMeta/thisObj/返回寄存器 `ret`/pc；
  寄存器窗口 `_sp` 由 `FastRegisterPool` 分配（**实参区在 `%0..arity-1`**）。
- `Constant.hpp`：常量池条目 + `FuncMeta`（名字/arity/chunk/localVars）、`ClassMeta`
  （slot 化成员表 + 按名继承）、`PropMeta`（getter/setter）。
- `VM.hpp`：宿主 API 门面，`eval/evalExpr<T>` 返回 GC 根保护的 `Handle<T>`。
- `src/cli/`：argparse 参数校验、编译诊断、脚本/字节码执行、REPL；`src/main.cpp` 负责进程入口。

### 2.5 `src/runtime/` + `src/gc/` — 运行时

- `Runtime`：运行时资源所有者——AtomTable、MarkSweep GC（固定 512B 节点 × 自由链表）、
  GC 根句柄集合、`create<T>()`（返回 `RootGuard<T>`，析构摘根；生命周期见 `Runtime.hpp` 注释）。
- `Context`：执行上下文——调用栈（`_callStack`，深度上限 1024）、寄存器池、GlobalObject、
  原生方法注册表。
- `AtomTable`：字符串驻留（u32 `Atom` 句柄），运算符/属性名/变量名全部走 Atom。

### 2.6 `src/types/` — 动态类型系统

- `Value`：六类型动态值（**Void/Integer/Real/String/Octet/Object**，对标 TJS Variant），
  全部运算/比较/转换（宽松 `==` 与严格 `===` 分离：`equals`/`discernEquals`）。
- `Object`：虚接口 `hasProp/getProp/setProp/call/instanceOf`——所有对象统一走属性协议。
- `Class.hpp/cpp`：`ClassObject`（类对象，持有 ClassMeta）、`DataObject`（实例，slot 化成员，
  **方法访问时克隆 Function 并绑定 this**，见 `Class.cpp`）、`GlobalObject`。
- `Function`：TJS 函数（FuncMeta + thisObj）；`NativeFunction`：原生函数（traits 支持成员函数/lambda）。
- `Property`：getter/setter 属性对象（对象/global/this 成员读写可触发处理器；GLocal/DLocal 仅复制引用）。

### 2.7 `src/stdlib/`

各 `*Lib.cpp` 实现和注册 String、Array、Dictionary、RegExp、Math、Date、Random、
结构化存取及 Scripts；`StandardLibrary` 汇总安装，print/println 和 Exception 由宿主按需添加。
`NativeRegister` 按开放的 `NativeTypeKey` 分派，`TypeId` 保留给 VM 内部判型。
宿主普通 C++ 类型的绑定入口是 `Binding.hpp`，见 [embedding.md](embedding.md)。

## 3. 关键设计决策

| 决策 | 理由 |
|---|---|
| 自研 Pratt 解析器而非 bison | 无生成器依赖、错误恢复可控；产生式以 `doc/grammar/tjs.y` 为对齐基准 |
| TAC（多态对象）+ 扁平字节码两级 IR | TAC 便于实现优化 Pass；扁平字节码是热路径；惰性编译使慢路径零开销 |
| 寄存器机（非栈机） | 对标 TJS2 VM（[bytecode/tjs2-bytecode.md](bytecode/tjs2-bytecode.md)），也为字节码转译铺路（[bytecode/mapping.md](bytecode/mapping.md)） |
| 实参经寄存器池顶传递，被调帧 %0..arity-1 即实参 | 免独立实参栈；与 TJS "ra[-3] 向下"的对应关系在转译 prologue 解决 |
| GC 标记清除 + RootGuard | 根由句柄和 MarkRootProvider 显式枚举；见 `Runtime.hpp` |
| 常量池 index 0 恒 Void | 空/void 语义有免费表达；TJS 转译时 VOID/OBJECT(null) 直接映射 *0 |
| 类成员 slot 化 + 按名继承 | 源码路径在运行期解析父类名；字节码路径的动态类体边界见 [转译说明](bytecode/mapping.md#7-类与属性元数据) |

## 4. 与 TJSCompiler（旧版）逐段对照

| 阶段 | TJSCompiler（原版） | Ciallang |
|---|---|---|
| 预处理 | `@set/@if/@endif` 经 tjspp.y 表达式（`tjsCompileControl`） | `PreProcessor`（@set/@if，独立实现） |
| 词法 | `tTJSLexicalAnalyzer`（tjsLex.cpp） | `Lexer`（手写 DFA + Trie） |
| 语法 | **bison LALR**（tjs.y → tjs.tab.cpp），动作直接驱动代码生成 | Pratt 递归下降 → AST → 再生成 |
| 中间表示 | `tTJSInterCodeContext`：编译与执行同体，**CodeArea 即产物** | `IRGenerator` 产生 TAC 存 `Chunk`，编译/执行分离 |
| 指令集 | 128 操作码 int32 码字（[bytecode/tjs2-bytecode.md](bytecode/tjs2-bytecode.md)） | 95 种 TAC 与对应扁平操作码（[bytecode/ciallang-bytecode.md](bytecode/ciallang-bytecode.md)） |
| 值 | `tTJSVariant`（CopyRef 引用计数） | `Value`（短串内联、长串/Octet 共享负载、对象由 GC 管理） |
| 对象 | `iTJSDispatch2` COM 风格接口 | `Object` 虚接口（hasProp/getProp/setProp/call） |
| 函数 | context 本身是可调用对象（实现 iTJSDispatch2） | `Function`(FuncMeta 包装) / `NativeFunction` |
| 异常 | ENTRY/EXTRY try 栈 + C++ 异常穿越 | `ThrowHandler` 区间表 + `unwind()` 逐帧退栈 |
| 序列化 | `"TJS2" "100\0"` 字节码容器（tjsScriptBlock/tjsByteCodeLoader） | 已实现 TJS2 文件读取与转译；内部 Chunk 无磁盘导出（见 mapping.md） |
| GC | 无（纯引用计数） | 标记清除 + 根句柄 |
| 标准库 | Array/Dictionary/Date/Math(+MT 随机)/Exception/RegExp(oniguruma)/String/Octet | 各库状态及偏差见 [compatibility.md](compatibility.md) |

## 5. 目录↔职责速查（改代码定位用）

```
改词法/新字面量      → src/parser/Lexer.cpp, Token.hpp, OperatorTrie.hpp
改语法/新语句        → src/parser/Parser.cpp（+ doc/grammar/tjs.y 对照）
加 AST 节点          → src/parser/ast/*（X-macro + 节点类 + IRGenerator 回调）
改指令发射/寄存器    → src/gen/IRGenerator.cpp
加/改指令            → src/gen/Instruction.hpp(.cpp) + src/vm/Bytecode.cpp + src/vm/VMState.cpp
改调用/帧/实参       → src/vm/CallFrame.hpp, src/types/Function.cpp, src/vm/VMState.hpp
改属性/方法绑定      → src/types/Class.cpp（DataObject::getProp）
改类/slot 语义       → src/types/Class.cpp + src/vm/Constant.hpp（ClassMeta）
改异常/try           → src/vm/VMState.cpp（unwind）, src/vm/Chunk.hpp（ThrowHandler）
改全局查找/作用域    → src/vm/VMState.cpp（getThis/setThis/getUpVal）
加原生函数/标准库    → src/stdlib/*（NativeRegister 注册）
GC/内存              → src/gc/gc.cpp, src/runtime/Runtime.*
```

## 6. GC 终结与访问缓存

`src/gc/gc.hpp` 的 collect 在标记根后先快照不可达且需要 finalize 的对象，再标记整个队列及其依赖，
sweep 不运行脚本。`src/vm/VMState.cpp` 在指令安全点 drain 所属 VM 的队列并保存 `_zf`；
`src/types/Class.cpp` 通过 `VM::callValue` 同步执行一次终结回调。队列是 Runtime 的根，支持环、
回调内再次 GC 和复活；异常经过现有脚本异常协议传播。没有活动 VM 的 Runtime 销毁不运行脚本。

`src/types/Value.hpp` 内联不超过 14 字节的字符串，较长字符串仍用共享 payload；`String` 与
`Value` 均为 16B（当前 64 位构建）。字符串裸指针仅在所属 Value 存活期间有效。

原生成员调用用 GMethodC/CallMethodV 分离函数与接收者，读取成员先于计算实参，避免每次调用复制
NativeFunction。提取方法作为一等值时仍独立绑定，见 `src/gen/IRGenerator.cpp`、`src/types/Function.cpp`。
VM 含 64 项原生成员缓存（按 NativeRegister 注册版本失效）和 64 项全局槽缓存（按 ClassObject
插入/删除版本失效）。全局赋值更新同一槽，缓存读取最新值；不持有脱离全局对象的 Value，见
`src/stdlib/NativeRegister.hpp`、`src/types/Class.cpp`、`src/vm/VMState.hpp`。
