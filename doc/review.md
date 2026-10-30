# 工程整理与运行时审查（2026-10-30）

本次基线为 Ciallang `b04e6233`；参考源码为兄弟目录 TJSCompiler `1427b36d`。审查覆盖 VM 入口与宿主调用、GC 根与调用帧、原生注册、命令行、CMake 依赖边界和文档一致性。具体使用方法见 [building.md](building.md)、[cli.md](cli.md)、[embedding.md](embedding.md)。

## 已修复问题

| 问题 | 修复与来源 |
|---|---|
| CLI 按分支手工解析参数，缺少统一数量/模式校验 | argparse 统一解析并校验；编译失败返回非零，`--check-json` 独立输出结构化诊断（`src/cli/Options.cpp`、`Application.cpp`、`Diagnostics.cpp`） |
| 入口承担编译、执行、分析和输出，库源码被可执行目标重复编译 | 入口缩减为参数和异常边界；职责拆到 `src/cli/`；CLI 链接库目标（`src/main.cpp`、`CMakeLists.txt`） |
| 库引入时强制安装测试/CLI 依赖并创建工具目标 | CLI/测试/JIT 映射到独立 vcpkg feature，作为子项目默认关闭工具与测试；C++ 标准/头文件/选项使用目标传播（`CMakeLists.txt`、`vcpkg.json`） |
| 原生类单独写全局属性，Math 尚未保根便分配子对象 | `registerClass`/`registerGlobal` 统一注册；先注册 Math 再构造 RandomGenerator（`src/runtime/Context.*`） |
| 句柄复制未增加 GC 根，Value 句柄未保护对象 | RootGuard/Handle 明确复制和移动语义，Value 句柄也保根（`src/runtime/Runtime.hpp`、`src/vm/VM.hpp`） |
| 将编译产物移入 GC 堆时，触发 GC 可提前回收常量中的元数据 | `Chunk::toHeap` 在分配期间临时保根；宿主表达式也使用堆 Chunk（`src/vm/Chunk.*`、`VM.hpp`、`VMState.cpp`、`src/tjsbc/TjsTranslator.cpp`） |
| 连续编译函数时禁止 GC 的分配返回空指针，导致 REPL 崩溃 | 编译阶段的元数据单独保根，允许分配时正常回收，离开编译阶段后释放临时根；覆盖 300 次定义/执行（`src/runtime/Runtime.hpp`、`src/gen/IRGenerator.cpp`、`tests/cli_test.py`） |
| 调用帧析构前重建自身，跳过旧 thisObj 的资源释放；移动遗漏调用元数据 | 析构正常释放成员，空槽重建放在 freeCallFrame；移动传递缓存与实参/with 深度并转移池所有权（`src/vm/CallFrame.hpp`、`VMState.hpp`） |
| 全局对象不在 GC sweep 池内，第二次回收可能遗漏 proxy | 每次标记显式保留全局 proxy（`src/runtime/Context.cpp`） |
| AtomTable 未释放保留的空 Atom；移动赋值遗漏原有资源 | 统一释放其拥有的 Atom（`src/runtime/AtomTable.hpp`） |
| 只有 setter 的属性留下未初始化 getter 指针 | 显式初始化为空，覆盖全局与类属性的 GC/写入/读取错误（`src/gen/IRGenerator.cpp`、`tests/runtime_lifetime_test.cpp`） |
| 源码切片错误选择 C 字符串重载，读取未终止的缓冲区 | 使用显式字节指针与长度并检查边界，覆盖嵌入 NUL（`src/common/SourceFile.cpp`、`tests/runtime_lifetime_test.cpp`） |
| 空 Octet 调用 memcpy 时传入空指针；持久化头可能未对齐 | 零长度复制跳过，头部改用 memcpy（`src/types/Octet.cpp`、`tests/runtime_lifetime_test.cpp`） |
| 原生调用抛异常时跳过栈上实参副本析构，泄漏字符串负载 | 用 scope guard 对成功/异常路径统一清理已构造的实参，覆盖连续异常与恢复（`src/vm/VMState.cpp`、`tests/runtime_lifetime_test.cpp`） |
| 整数返回值直接改写槽标签，遗漏旧字符串/字节串释放 | 快路径仅复用整数槽，其余使用 Value 赋值释放旧资源（`src/vm/VMState.cpp`） |
| 源码重新加载沿用旧读取位置和回溯标记 | load 重置位置/宽度/mark 栈（`src/common/SourceFile.cpp`） |
| 注释空输入误报、部分解析失败未设置错误、块注释吞读结束符、未闭合正则被接受 | 修正空输入与失败传播、嵌套注释扫描和正则闭合检查；词法器暴露续行状态供 REPL 使用（`src/parser/Parser.cpp`、`Lexer.*`） |
| 宿主/REPL 后缀自增未请求表达式值；isvalid 写未分配的结果寄存器 | 明确请求结果并分配 isvalid 目标寄存器（`src/vm/VM.hpp`、`VMState.cpp`、`src/cli/Compilation.cpp`、`src/gen/IRGenerator.cpp`） |

字节码执行入口统一为 `VMState::run()`，删除独立基准入口。移除未使用的 spdlog，测试头文件对齐 Catch2 v3。文档统一当前编译/执行流水线，移除过期会话交接文件，补齐构建、CLI/REPL、库集成与预设（`AGENTS.md`、`doc/README.md`、`CMakePresets.json`）。

## 回归覆盖

`tests/runtime_lifetime_test.cpp` 覆盖句柄复制/移动、GC 满池时的 Chunk 元数据、注册后的类与属性、调用帧移动、重复 GC 的 proxy、源码重载和后缀表达式结果。`tests/cli_test.py` 的 14 个场景通过子进程覆盖参数/退出码/输出协议、编译不执行、图输出、异常路径导出、REPL 多行/预处理/状态/恢复与未闭合词法输入，并运行 12 个字节码 fixture。`tests/embedding/` 在禁止查找 argparse/Catch2、父项目 C++17 且通用 ENABLE_TESTS/BUILD_EXECUTABLE/ENABLE_FUZZ 全开的环境下验证库目标的 C++20 传递及默认目标边界。

本地环境：Linux x64 / GCC 14.2，vcpkg 使用仓库固定 baseline。

| 配置或检查 | 结果 |
|---|---|
| Debug，JIT 关闭 | 214/214 CTest 通过 |
| Release，JIT 开启 | 214/214 CTest 通过 |
| C++17 父项目引入库，禁止 argparse/Catch2 查找 | 构建及运行 1/1 通过 |
| 纯库 manifest 安装 | 只有 fmt、frozen、utf8proc 和 vcpkg 的两个构建辅助包 |
| 父项目开启通用旧开关、使用自定义 Release 优化标志 | 新目录配置通过，不创建 CLI/测试/fuzz 目标 |
| CLI/REPL 进程测试 | 14 场景通过，包含 300 次函数重新定义与执行 |
| 字节码 CLI fixture | 12/12 通过 |
| JSON、文档相对链接、diff 空白 | 通过 |
| ASan/UBSan（含泄漏检测） | 214/214 CTest 通过；ASAN_OPTIONS=detect_leaks=1，UBSAN_OPTIONS=halt_on_error=1 |

当前上游 TJSCompiler 没有文档历史记录中使用的 CLI，故不把本次 fixture 测试视为实时双引擎对拍；本次未在 Windows/macOS 运行，仓库中的对应 CI 配置将在推送后执行。

## Cloud fuzz 增量验证（2026-10-30）

使用 Linux x64 / Clang 19.1.7、libFuzzer、ASan/UBSan 与泄漏检测。为核心库和两个解析入口同时插桩，语料取自现有 `.tjs` 样本与定向边界输入，并加入 TJS 关键字字典。入口只解析，不执行脚本；构建和运行方法见 [building.md](building.md#有界-fuzz)。

先重放边界输入，再做随机变异。发现并修复：

| 输入或问题 | 处理 |
|---|---|
| `@set(x=1%0)` 触发 UBSan 除零；`@set(x=1/0)` 的异常逸出解析入口 | 模除检查零；Lexer 将预处理运行时错误转为错误 token，CLI `--check-json` 返回失败诊断 |
| 类声明名不是标识符，后续构造函数比较读取空字符串指针 | 类/函数/属性声明先校验名字；属性体在 EOF 时也安全报错 |
| 随机变异得到超大十进制指数，累积 `int` 溢出 | 指数饱和累积，仍消费全部数字；非十进制指数同步处理 |

关联审查同时消除了预处理 32 位整数累积/加减乘/取负的 C++ 有符号溢出，显式采用补码回绕；处理最小整数除以或模除 `-1`；非十进制 64 位整数字面量改为无符号累积。回归覆盖这些边界及诊断协议（`tests/preprocessor_test.cpp`、`tests/fuzz_regress_test.cpp`、`tests/cli_test.py`）。

修复后的有界随机测试结果：

| 入口 | 时长 | 执行输入次数 | 峰值 RSS | 结果 |
|---|---|---|---|---|
| lexer | 61 秒 | 200,771 | 471 MB | 无新增异常报告 |
| parser | 61 秒 | 60,687 | 526 MB | 无新增异常报告 |

两个单进程测试并行，最多使用两个 CPU 核心；每项设定 60 秒总时长、5 秒单输入超时、1024 MB RSS 上限、4096 字节输入上限。libFuzzer 在退出检查时记录了 61 秒；RSS 参数为周期检查，不是操作系统硬限制。首次 Clang/插桩构建和定位问题的前置运行另计。最终完整 Debug、Release+JIT、ASan/UBSan 测试均为 214/214。

可读的触发/边界输入保存在 `tests/fuzz_regress/*.tjs`，普通测试同时重放 lexer 和 parser。生成的覆盖率语料与原始异常文件留在构建目录，不加入源码。短时测试只覆盖源码前端，不能据此断言 VM/JIT 或所有输入都没有问题。
