# AGENTS.md — AI 协作规范与项目导航

> 本文件是 AI（或新成员）接触本项目的第一入口。目标：**3 分钟定位到要改的代码，改完知道怎么验证**。
> 全部文档索引见 [doc/README.md](doc/README.md)。

## 项目定位

**Ciallang** 是用 C++20 从零实现的 TJS2 兼容脚本语言解释器。兼容目标是原版 Kirikiri TJS2 引擎
（参考实现在 `../TJSCompiler`，本文档称其为 **TJSCompiler / 旧版**）。

兼容分三层，当前状态：

| 层 | 目标 | 状态 |
|---|---|---|
| 源码兼容 | 直接运行 .tjs 源码 | 核心语法已实现（含 Array/Dictionary 字面量、with、RegExp；已知差异见 [doc/compatibility.md](doc/compatibility.md)） |
| 字节码兼容 | 加载 TJSCompiler 编译出的字节码文件（`"TJS2" "100\0"` 格式），**转译**为本项目内部指令后执行 | M1–M4 完成（转译路线，`cial --tjsbc`；含 collapse/展开调用），见 [doc/bytecode/mapping.md](doc/bytecode/mapping.md) |
| 标准库兼容 | Array/Dictionary/String/Math/Date/RegExp/Exception/Octet | 均已接入，语义差异见 `doc/compatibility.md` |

官方语言参考（语法权威）：[TJS2 リファレンス](https://hydrozoa.felisworks.com/doc/tjs2doc/contents/index.html)。

## 目录地图

```
Ciallang/
├─ AGENTS.md            ← 本文件（AI 入口）
├─ CLAUDE.md            ← 指向 AGENTS.md
├─ .github/workflows/ci.yml ← Linux/macOS/Windows 双配置 + Windows 可选 JIT CI
├─ TODO.md              ← 后续任务清单（性能/正确性/特性缺口，改代码前先看）
├─ doc/                 ← 全部文档，索引见 doc/README.md
│   ├─ building.md              构建选项、vcpkg feature 与验证
│   ├─ cli.md                   命令行、退出码与 REPL
│   ├─ embedding.md             CMake 子项目与宿主注册 API
│   ├─ architecture.md          架构设计（流水线/模块/关键类）
│   ├─ syntax.md                TJS2 语法参考 + 本项目实现状态矩阵
│   ├─ compatibility.md         兼容性矩阵与路线图
│   ├─ bytecode/
│   │   ├─ tjs2-bytecode.md     旧版 TJS2 字节码规范（128 操作码/序列化格式）
│   │   ├─ ciallang-bytecode.md 本项目指令集规范（TAC 95 + 扁平字节码 95）
│   │   └─ mapping.md           TJS2→Ciallang 转译设计（后续实现字节码兼容的蓝本）
│   ├─ grammar/                 原版 bison 语法副本（tjs.y / tjsdate.y / tjspp.y）
│   └─ language/                类语义/表达式运算符笔记
├─ src/                 ← 源码（含 cli/ 命令行与 REPL，模块职责见下表）
├─ tests/               ← Catch2 测试 + test_files/*.tjs 脚本语料
├─ tools/lsp/          ← Python stdio LSP 与协议集成测试（doc/lsp.md）
├─ editors/vscode/     ← VSCode LSP 客户端源码
├─ bench/               ← 性能基准语料（*.tjs）+ 字节码对拍工具（bytecode_conformance.py，见 doc/performance.md、doc/bytecode/mapping.md §9）
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
| `src/gen/` | AST→中间代码 | `IRGenerator`(寄存器分配/作用域/回填)、`Instruction.hpp`(**95 条 TAC 指令 X-macro**)、`Register.hpp`(%n)、`Label.hpp`(@n)、`LocalVariable`、`SSA`(按需静态单赋值分析，`--dump-ssa`，见 doc/ssa.md)、`ControlFlowGraph`+`Optimizer`(基本块 CFG+自然循环识别（支配树回边检测，getLoops()）、拷贝传播（GLocal/DLocal 纯拷贝改写+死拷贝清除，管线首位）、整数常量传播（块内折叠+唯一前驱跨块继承）、死 `LoadImm`/死拷贝清理、CompareTest 融合；由外部 `optimizeDefaultChunk` 组装，`parseAst()` 调用注入策略) |
| `src/vm/` | 执行引擎 | `VMState`(字节码派发与调用帧调度)、`Chunk`(指令+常量池+双空间 try 表)、`Bytecode`(扁平 u8 编码, **95/95 全实现**)、`CallFrame`(含 thisData 缓存 + argWindowDirect 直写布局)、`FastRegisterPool`、`Constant.hpp`(FuncMeta/ClassMeta/PropMeta)、`Rlc`(行号表)、`VM.hpp`(宿主门面)、`ExecutionTrace`(真实路径 DOT)、`IntegerJit`(可选 x64 整数 JIT/热路径守卫；默认关闭) |
| `src/runtime/` | 全局运行时 | `Runtime`(GC/RootGuard/atomTable)、`Context`(调用栈/全局对象, 深度上限 1024)、`AtomTable`(字符串驻留 Atom) |
| `src/types/` | 动态类型系统 | `Value`(六类型动态值+全部运算)、`String`/`Octet`/`Real`、`Object`(hasProp/getProp/setProp/call 虚接口)、`Class`(ClassObject/DataObject/GlobalObject, slot 化)、`Function`+`NativeFunction`、`Property`(getter/setter)、`TypeConverter`、`Array`/`Dictionary`(容器)、`RegExp`(std::regex+TJS2 匹配状态机) |
| `src/gc/` | 内存管理 | `MarkSweepHeader`、固定 512B 节点自由链表 + 标记清除 |
| `src/stdlib/` | 内置库 | `Print`(print/println)、`StringLib`、`ArrayLib`/`DictLib`/`RegExpLib`(容器与正则方法)、`SerializeLib`(结构化存取 save/load 系 + Octet pack/unpack)、`MathLib`、`DateLib`+`DateParser`、`RandomLib`、`NativeRegister`(按 TypeId 分派原生方法)、`StandardLibrary`(汇总各 `*Lib` 的 Context 注册入口)、`Debug` |
| `src/tjsbc/` | TJS2 字节码转译（阶段 2，M1–M4 完成） | `TjsBytecodeReader`(格式解析)+`TjsTranslator`(恒等平移/标志物化与 M3 前瞻融合（Cmp 族直写 zf）/属性全族（含整数成员名；direct 名走 *C Atom 直引指令）/类 context 加载期解释/NEW→Call/P 变体与 GETP·SETP/DELD·TYPEOFD/proxy 回落链/省略透传·展开调用·collapse 参数/BNOT·NUM·OCTET/dst=0 丢弃语义——tjs2 语料 12 文件字节码路径全量通过，见 [doc/bytecode/mapping.md](doc/bytecode/mapping.md)) |
| `src/logging/` | 日志 | `Logger`（fmt 实际输出） |
| `src/main.cpp` / `src/cli/` | 进程入口；argparse 参数、编译/诊断、执行与 REPL 分模块，见 [doc/cli.md](doc/cli.md) |

## 构建与测试

```bash
# 配置+构建（Windows，需 VCPKG_ROOT；Linux/MacOS 预设同名）
cmake --preset "Windows Debug"     # 产物在 build/Windows/Debug
cmake --build build/Windows/Debug

# 运行测试（Catch2，通过 CTest 注册）
ctest --test-dir build/Windows/Debug/tests --output-on-failure

# 运行脚本
./build/Windows/Debug/cial.exe tests/test_files/fib_test.tjs

# fuzz（仅 Clang，可选）
# 见 doc/building.md 的“有界 fuzz”，默认每项 60 秒
bash fuzz/run_fuzz_lexer.sh
bash fuzz/run_fuzz_parser.sh
```

- 依赖经 vcpkg 管理（`vcpkg.json`）：fmt、frozen、utf8proc 为基础依赖；CLI 的 argparse、测试的 Catch2、JIT 的 asmjit 由 feature 控制，见 [doc/building.md](doc/building.md)。
- C++ 测试文件登记在 `tests/CMakeLists.txt`；Python CLI/LSP 集成测试也由 CTest 注册。
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
   编译期对象由 Runtime 的编译作用域临时保根，允许编译分配触发 GC；移入堆的 Chunk 使用 `toHeap()` 保护常量图。
   **新增持有 GC 对象的全局数据时必须接入标记**；**所有 `marked()` 覆盖必须以
   `if(_marked) return;` 开头并调用基类**（对象图有环：FuncMeta↔Chunk、DataObject↔ClassObject；
   不检查标志会无限递归，漏调基类则对象必被清扫）。
2. **指令语义**：修改指令时同步检查 `Bytecode.cpp` 编码与 `VMState.cpp` 派发，
   并覆盖异常恢复、属性访问和原生回调。
3. **惰性 toBytecode**：`Chunk::toBytecode()` 幂等，run 取帧点触发，同时生成字节码空间的
   `_bcThrowHandlers`（unwind 用）。不要在编译期提前调。
4. **压帧不执行的三类 dispatch**：`DLocal/DThis/DGlobal`（属性 setter）、`Inv`（finalize）、
   `Call`——检测 `_stackTop` 变化后返回 `DispatchRet::Call` 让主循环接管；否则新帧永远不会执行。
   `Property::invokeGet` / `Class.cpp` 初始化与构造则用**嵌套 run**（entryStackTop 语义）。
5. **GC sweep 不得重入解释器**：`~DataObject` 不再触发 finalize（曾在 sweep 批量析构时压入
   数百个无人执行的帧）。不可达对象先进入保活队列，由所属 VM 的指令安全点执行；
   回调期间再 GC、环和复活均受保护，终结一次，异常经脚本异常协议传播。
   队列由 `Runtime::collectMark()` 标记，见 `src/gc/gc.hpp`、`src/vm/VMState.cpp`。
   Runtime 销毁阶段不执行脚本；宿主主动收集后应让所属 VM 再运行或在有效帧中 drain。
6. **优化器范围有限**：`src/gen/Optimizer.cpp` 的默认策略已接入 CFG、溢出安全的整数 `Add/Sub/Mul` 常量传播（块内折叠 + 唯一前驱块间继承出口状态；回边与多前驱保守放弃）、死 `LoadImm` 清理、保留结果寄存器的 CompareTest 融合和 NOP 压缩；常量池 `Load` 会实例化运行时对象，DCE 不得删除。压缩会重映射跳转、try 区间与 RLC 行号。DCE 对 try 保护块保守保留加载，其他指令按活跃性屏障处理。默认优化管线尚无 SSA、跨块汇合点传播或 `LoadSub`，性能结果以 `doc/performance.md` 的同机对照为准；扩展优化前先做同机 Release 对拍基准并覆盖外部可观察寄存器、异常边与元数据重映射。
7. **Value 字符串分短串内联和长串共享两种**：14 字节内直接存 String，长串用
   `SharedPayload<String>`；见 `src/types/Value.hpp`、`String.hpp`。`asString()` 借用指针
   的寿命不能超过所属 Value，尤其禁止借用 `createValue()` 临时值（NewRegExp 曾因此丢 flags）。
   原生方法直接调用用 GMethodC/CallMethodV 分离接收者；提取为一等值时仍创建独立绑定。
8. **测试注册**：新增测试文件必须登记进 `tests/CMakeLists.txt` 的 SOURCES（历史上有 4 个
   文件被注释禁用导致"全绿 ≠ 全部通过"，2026-10-29 已全部修复启用）。
9. **Value 具有非平凡构造/析构**：C++ 栈上声明 `Value arr[N]` 会对全部 N 槽默认
   初始化并逐槽析构——热路径的变长实参缓冲必须 placement new 只构造实际数量
   （见 `invokeCallV` 原生快路径；初版全槽默认初始化曾致 native 基准回退 256%）。
   槽内 Value 的对象引用在调用期间由调用者寄存器槽保根，无需额外注册 GC 根。
10. **新增 Object 子类判型走 TypeId**：热路径禁止 `dynamic_cast`（RTTI 遍历），
   在 `TypeId`（`src/types/TypeId.hpp`）加专有值并 override `typeId()`，用一次
   虚调用判型（Function/NativeFunction/Property 先例）；Property/NativeFunction
   均为 final，static_cast 与 dynamic_cast 等价。

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

- 位置：`../TJSCompiler`（原版 Kirikiri TJS2 提取版，CMake 重组）。
- 关键源码：操作码定义 `src/syntax/lexer/tjsInterCodeGen.h:45-76`；执行器 `src/syntax/tjsInterCodeExec.cpp`；
  序列化 `src/syntax/tjsScriptBlock.cpp:460-510`（导出）/ `src/syntax/tjsByteCodeLoader.cpp`（加载）；
  常量池 `src/syntax/tjsConstArrayData.*`；语法 `src/syntax/bison/tjs.y`。
- 当前下载的 TJSCompiler 上游入口 `src/main.cpp` 使用硬编码启动路径，不接受脚本路径或 `--dump` 参数。
  历史记录中的对拍/导出命令依赖本地 CLI 适配入口，不能直接当成上游能力；公开导出 API 为 `tTJS::CompileScript`。
  本项目已有 `tests/test_files/tjsbc/` 样本，可独立验证加载、转译与执行。
- 本项目侧采用转译：加载 `"TJS2" "100\0"` 文件 → TAC → VM，见 [doc/bytecode/mapping.md](doc/bytecode/mapping.md)。
- 语义拿不准时，先对照参考实现源码；执行对拍前确认参考入口确实支持命令行参数，并记录所用版本。

## 文档维护约定

- 改指令集/字节码编码 → 同步更新 `doc/bytecode/ciallang-bytecode.md`。
- 实现新语法特性 → 更新 `doc/syntax.md` 状态矩阵与 `doc/compatibility.md`。
- 新增模块/移动文件 → 更新本文件目录地图与 `doc/README.md`。
- 文档语言：中文；事实性内容必须带源码文件引用，避免与代码漂移。
