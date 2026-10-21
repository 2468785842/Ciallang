# AGENTS.md — AI 协作规范与项目导航

> 本文件是 AI（或新成员）接触本项目的第一入口。目标：**3 分钟定位到要改的代码，改完知道怎么验证**。
> 全部文档索引见 [doc/README.md](doc/README.md)。

## 项目定位

**Ciallang** 是用 C++20 从零实现的 TJS2 兼容脚本语言解释器。兼容目标是原版 Kirikiri TJS2 引擎
（参考实现在 `D:\Users\LiDon\CLionProjects\TJSCompiler`，本文档称其为 **TJSCompiler / 旧版**）。

兼容分三层，当前状态：

| 层 | 目标 | 状态 |
|---|---|---|
| 源码兼容 | 直接运行 .tjs 源码 | 部分实现（核心语句/函数/类/property 已通，缺 Array/Dictionary 字面量、with、RegExp 等，见 [doc/compatibility.md](doc/compatibility.md)） |
| 字节码兼容 | 加载 TJSCompiler 编译出的字节码文件（`"TJS2" "100\0"` 格式），**转译**为本项目内部指令后执行 | 设计定稿（路线：转译，非独立解释器），见 [doc/bytecode/mapping.md](doc/bytecode/mapping.md) |
| 标准库兼容 | Array/Dictionary/String/Math/Date/RegExp/Exception/Octet | 仅 String 部分 |

官方语言参考（语法权威）：[TJS2 リファレンス](https://hydrozoa.felisworks.com/doc/tjs2doc/contents/index.html)。

## 目录地图

```
Ciallang/
├─ AGENTS.md            ← 本文件（AI 入口）
├─ CLAUDE.md            ← 指向 AGENTS.md
├─ TODO.md              ← 后续任务清单（性能/正确性/特性缺口，改代码前先看）
├─ doc/                 ← 全部文档，索引见 doc/README.md
│   ├─ architecture.md          架构设计（流水线/模块/关键类）
│   ├─ syntax.md                TJS2 语法参考 + 本项目实现状态矩阵
│   ├─ compatibility.md         兼容性矩阵与路线图
│   ├─ bytecode/
│   │   ├─ tjs2-bytecode.md     旧版 TJS2 字节码规范（128 操作码/序列化格式）
│   │   ├─ ciallang-bytecode.md 本项目指令集规范（TAC 55 + 扁平字节码 17）
│   │   └─ mapping.md           TJS2→Ciallang 转译设计（后续实现字节码兼容的蓝本）
│   ├─ TJS2_OPCODE.md           （历史）TJS2 VM ISA 学习笔记，写作时的原始参考
│   ├─ grammar/                 原版 bison 语法副本（tjs.y / tjsdate.y / tjspp.y）
│   └─ language/                类语义/表达式运算符笔记
├─ src/                 ← 源码（模块职责见下表）
├─ tests/               ← Catch2 测试 + test_files/*.tjs 脚本语料
├─ fuzz/                ← libFuzzer 语料（fuzz_lexer/fuzz_parser）
├─ bin/                 ← 历史构建产物
└─ build/               ← 构建输出（Windows/Linux/MacOS × Debug/Release）
```

### src/ 模块职责与关键文件

数据流：**源码 → PreProcessor → Lexer → Parser → AST → IRGenerator → TAC 指令 → Chunk → VM 执行**。

| 模块 | 职责 | 关键文件 / 类 |
|---|---|---|
| `src/common/` | 基础设施 | `Result.hpp`(错误收集)、`Ret.hpp`(ErrCode/Ret<T>)、`SourceFile`、`SourceLocation`、`UTF8`、`Hash.hpp`、`Defer.hpp` |
| `src/parser/` | 词法+语法+预处理 | `Lexer`(手写 DFA)、`Parser`(Pratt 优先级)、`PreProcessor`(@set/@if)、`Token.hpp`、`OperatorTrie`、`ast/*`(AstNode 基类 + X-macro 节点枚举) |
| `src/gen/` | AST→中间代码 | `IRGenerator`(寄存器分配/作用域/回填)、`Instruction.hpp`(**55 条 TAC 指令 X-macro**)、`Register.hpp`(%n)、`Label.hpp`(@n)、`LocalVariable`、`ControlFlowGraph`+`Optimizer`(**已建未接线**) |
| `src/vm/` | 执行引擎 | `VMState`(**run0() 扁平字节码解释，唯一执行路径；多态 run() 已删除**)、`Chunk`(指令+常量池+双空间 try 表)、`Bytecode`(扁平 u8 编码, **55/55 全实现**)、`CallFrame`(含 thisData 缓存)、`FastRegisterPool`、`Constant.hpp`(FuncMeta/ClassMeta/PropMeta)、`Rlc`(行号表)、`VM.hpp`(宿主门面) |
| `src/runtime/` | 全局运行时 | `Runtime`(GC/RootGuard/atomTable)、`Context`(调用栈/全局对象, 深度上限 1024)、`AtomTable`(字符串驻留 Atom) |
| `src/types/` | 动态类型系统 | `Value`(六类型动态值+全部运算)、`String`/`Octet`/`Real`、`Object`(hasProp/getProp/setProp/call 虚接口)、`Class`(ClassObject/DataObject/GlobalObject, slot 化)、`Function`+`NativeFunction`、`Property`(getter/setter)、`TypeConverter` |
| `src/gc/` | 内存管理 | `MarkSweepHeader`、固定 512B 节点自由链表 + 标记清除 |
| `src/stdlib/` | 内置库 | `Print`(print/println)、`StringLib`、`NativeRegister`(按 TypeId 分派原生方法)、`Debug` |
| `src/logging/` | 日志 | `Logger`（fmt 实际输出） |
| `src/main.cpp` | 可执行 `cial`：`cial <script.tjs>`，走 run0() | |
| `src/main0.cpp` | 可执行 `cial_bc`：fib 基准，走 run0() 扁平字节码路径 | |

## 构建与测试

```bash
# 配置+构建（Windows，需 VCPKG_ROOT；Linux/MacOS 预设同名）
cmake --preset "Windows Debug"     # 产物在 build/Windows/Debug
cmake --build build/Windows/Debug

# 运行测试（Catch2，通过 CTest 注册）
ctest --test-dir build/Windows/Debug --output-on-failure

# 运行脚本
./build/Windows/Debug/cial.exe tests/test_files/fib_test.tjs

# fuzz（仅 Clang，可选）
# 见 fuzz/run_fuzz_lexer.sh
```

- 依赖经 vcpkg 管理（`vcpkg.json`）：Catch2、fmt、spdlog、utf8proc。
- 测试共 100 个 TEST_CASE（17 文件，其中 5 个文件因历史原因注释在 `tests/CMakeLists.txt`）。
- `tests/test_files/tjs2/` 下是原版 kirikiri 测试脚本，是兼容性的行为基准。

## 代码风格与惯例

- C++20；命名空间 `cial`（子空间 `cial::inter` / `cial::vm` / `cial::types`）。
- 类型别名：`u8/u16/u32/u64/i64`、`Vec<T>`、`Map<K,V>`、`Box<T>`、`Opt<T>`（见 `src/types/Types.hpp`）。
- **指令集用 X-macro**：`CIAL_TAC_OPCODE_ENUMS` / `CIAL_BYTECODE_OPCODE_ENUMS` 定义一次，
  枚举/类/dispatch/encode 全部由宏展开生成。**新增指令必须同时补**：
  `Instruction.hpp` X-macro + `DEF_INST` 声明、`Instruction.cpp` execute/dump、
  `Bytecode.cpp` encode、`VMState.cpp` dispatchXXX（若进扁平字节码）。
- 错误处理：编译期用 `Result`（收集多条消息）/ `Ret<T>`；运行期抛 `Err`（`Ret.hpp`）或 `std::runtime_error`。
- 寄存器 `%n`（`Register`）、标签 `@n`（`Label`）、常量 `*n`（`ConstIdx`）均为强类型包装。
- 格式化统一 clang-format（README 末尾有命令）。

## 已知坑位（改代码前必读）

1. **GC 根与标记协议**：`Runtime::create<T>()` 返回 `RootGuard<T>` 临时对象，析构即摘根；
   执行期 GC 由 `Runtime::collectAllMark()` 驱动——RootGuard 句柄 + `MarkRootProvider`
   （`Context` 构造时注册，覆盖寄存器池/调用栈/全局对象/原生方法表）。
   **新增持有 GC 对象的全局数据时必须接入标记**；**所有 `marked()` 覆盖必须以
   `if(_marked) return;` 开头并调用基类**（对象图有环：FuncMeta↔Chunk、DataObject↔ClassObject；
   不检查标志会无限递归，漏调基类则对象必被清扫）。
2. **唯一执行路径 run0**：`VM::eval`/`Property::invokeGet`/`Class.cpp` 全部走 `run0()` 扁平字节码。
   `run()` 多态路径与 `Instruction::execute` 已删除。改指令语义 = 改 `Bytecode.cpp` encode +
   `VMState.cpp` dispatchXXX 两处。
3. **惰性 toBytecode**：`Chunk::toBytecode()` 幂等，run0 取帧点触发，同时生成字节码空间的
   `_bcThrowHandlers`（unwind 用）。不要在编译期提前调。
4. **压帧不执行的三类 dispatch**：`DLocal/DThis/DGlobal`（属性 setter）、`Inv`（finalize）、
   `Call`——检测 `_stackTop` 变化后返回 `DispatchRet::Call` 让主循环接管；否则新帧永远不会执行。
   `Property::invokeGet` / `Class.cpp` 初始化与构造则用**嵌套 run0**（entryStackTop 语义）。
5. **GC sweep 不得重入解释器**：`~DataObject` 不再触发 finalize（曾在 sweep 批量析构时压入
   数百个无人执行的帧）。finalize 仅在脚本显式 `invalidate` 时执行。
6. **Optimizer 未接线**：`IRGenerator.cpp` 中对 CFG/Optimizer 的调用被注释；`gen/ControlFlowGraph.cpp`、`gen/Optimizer.cpp` 处于"建成未使用"状态，不要误以为优化生效。
7. **Value 字符串是共享负载块**（`SharedPayload<String>`：Value→payload(引用计数+String*)），
   拷贝零堆分配；旧三层 RefCountPointer 布局已删除。
8. **tests/CMakeLists.txt 有 5 个被注释的测试文件**（statement/type_system/ieee/integration），
   全绿 ≠ 全部测试通过，新增测试记得登记。

## "我想改 X，先读什么"

| 任务 | 先读 |
|---|---|
| 加/改语言语法 | [doc/syntax.md](doc/syntax.md)（语义+状态矩阵）→ `doc/grammar/tjs.y`（产生式权威）→ `src/parser/Parser.cpp` |
| 加/改运算符语义 | `src/types/Value.cpp`（运算实现）→ `src/gen/IRGenerator.cpp`（发射）→ `Instruction.cpp` |
| 加/改指令 | [doc/bytecode/ciallang-bytecode.md](doc/bytecode/ciallang-bytecode.md) → `src/gen/Instruction.hpp` X-macro（含"新增指令清单"） |
| 做字节码兼容（TJS 字节码加载） | [doc/bytecode/tjs2-bytecode.md](doc/bytecode/tjs2-bytecode.md) → [doc/bytecode/mapping.md](doc/bytecode/mapping.md)（转译设计） |
| 改类/继承语义 | `tests/class_extends_test.cpp`（15 个 TJS2 语义用例）→ `src/types/Class.cpp` |
| 改 GC/内存 | `src/gc/gc.cpp` + `src/runtime/Runtime.hpp` + TODO.md 已知问题 |
| 做性能优化 | `TODO.md` 一节（按预期收益排序的任务清单） |
| 了解整体架构 | [doc/architecture.md](doc/architecture.md) |

## 对 TJSCompiler（旧版参考项目）的约定

- 位置：`D:\Users\LiDon\CLionProjects\TJSCompiler`（原版 Kirikiri TJS2 提取版，CMake 重组）。
- 关键源码：操作码定义 `src/syntax/lexer/tjsInterCodeGen.h:45-76`；执行器 `src/syntax/tjsInterCodeExec.cpp`；
  序列化 `src/syntax/tjsScriptBlock.cpp:460-510`（导出）/ `src/syntax/tjsByteCodeLoader.cpp`（加载）；
  常量池 `src/syntax/tjsConstArrayData.*`；语法 `src/syntax/bison/tjs.y`。
- 已立项（未实现）：给 `TJSCompiler.exe` 加 `--dump` 选项导出字节码，作为本项目对拍样本生成器。
  本项目侧的字节码兼容走**转译路线**（加载 `"TJS2" "100\0"` 文件 → 转成 TAC → 现有 VM 执行），
  设计见 [doc/bytecode/mapping.md](doc/bytecode/mapping.md)。
- 修改本项目语义拿不准时，**以 TJSCompiler 实际行为为准**（跑 `TJSCompiler.exe <script>` 对拍）。

## 文档维护约定

- 改指令集/字节码编码 → 同步更新 `doc/bytecode/ciallang-bytecode.md`。
- 实现新语法特性 → 更新 `doc/syntax.md` 状态矩阵与 `doc/compatibility.md`。
- 新增模块/移动文件 → 更新本文件目录地图与 `doc/README.md`。
- 文档语言：中文；事实性内容必须带源码文件引用，避免与代码漂移。
