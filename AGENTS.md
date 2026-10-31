# 开发约定与项目导航

Ciallang 是 C++20 实现的 TJS2 解释器。先看 [README](README.md) 和 [待办](TODO.md)，
专题资料见[文档索引](doc/README.md)，常见使用限制见 [FAQ](doc/faq.md)。

## 代码地图

流水线：源码 → PreProcessor → Lexer → Parser → AST → IRGenerator → TAC → Chunk → VM。

| 位置 | 职责 |
|---|---|
| `src/common/` | 错误、源码位置、UTF-8 与通用工具 |
| `src/parser/` | 词法、预处理、Pratt 解析与 AST |
| `src/gen/` | 指令定义、代码生成、CFG、SSA 与优化 |
| `src/vm/` | 字节码、调用帧、执行、调试与可选 JIT |
| `src/runtime/` · `src/gc/` | Context、Runtime、Atom、根与标记清除 |
| `src/types/` | Value、对象、函数、属性与容器 |
| `src/Binding.hpp` | 宿主 Module / ClassBuilder 声明式绑定 |
| `src/stdlib/` | 各库的实现与注册入口；StandardLibrary 汇总安装 |
| `src/tjsbc/` | TJS2 字节码读取与转译 |
| `src/cli/` · `src/main.cpp` | CLI、REPL、编译与诊断输出 |
| `tests/` · `bench/` · `fuzz/` | 回归测试、基准、前端 fuzz 及语料 |
| `tools/lsp/` · `editors/vscode/` | stdio LSP 服务与 VSCode 客户端 |

## 构建与验证

设置 `VCPKG_ROOT`，按平台选择 `Linux`、`MacOS` 或 `Windows` 预设：

```sh
cmake --preset "Linux Debug"
cmake --build --preset "Linux Debug"
ctest --preset "Linux Debug"
```

基础依赖为 fmt、frozen、utf8proc；CLI、测试、JIT 的可选依赖和开关见[构建指南](doc/building.md)。
C++ 测试显式登记在 `tests/CMakeLists.txt`；纯语言/标准库行为放入 `tests/tjs/`，按功能归类，
用 TJS 断言并自动登记为 CTest；原生注册与内部状态检查保留 Catch2。见[脚本测试指南](tests/tjs/README.md)。
CLI、LSP 与字节码工具测试也通过 CTest 运行。
性能修改需做同机 Release 对照，并覆盖异常恢复、可观察结果与 GC 生命周期。

## 代码惯例

- C++20，命名空间 `cial`；遵循现有 `Integer`、`Vec<T>`、`Map<K,V>` 等别名。
- 修改的 C++ 文件使用仓库 `.clang-format` 格式化（指针声明为右对齐，
  `Fun &&f` / `const ScopeGuard &`）。提交前用 `tools/check-format.sh` 校验，
  `-i` 就地格式化；CI 以 `clang-format --dry-run -Werror` 强制检查，版本
  锁定 20.1.0（输出随版本变化，勿用未锁定版本重排全库）。
- 编译期错误使用 `Common::Result` / `Ret<T>`；运行期遵循现有 `Err` / 脚本异常传播协议。
- 标准库注册使用一次 `context.nativeType<T>()`，后续 `.method()` / `.property()` 链式调用。
- 新增指令同步修改 `Instruction.hpp` 与 `Bytecode.hpp` 的 X-macro、`DEF_INST` 声明、
  `Instruction.cpp` dump、`Bytecode.cpp` 编码和 `VMState.cpp` 执行派发。
- 修改前阅读相应接口注释；不要用编辑器风格建议替代对类型、GC 和异常协议的检查。

## 修改前先读

| 改动 | 入口及约束 |
|---|---|
| 语法 | [语法说明](doc/syntax.md)、`doc/grammar/tjs.y`、`src/parser/Parser.cpp` |
| 运算符 | `src/types/Value.cpp`、`src/gen/IRGenerator.cpp`、`src/gen/Instruction.cpp` |
| 指令与字节码 | [内部指令](doc/bytecode/ciallang-bytecode.md)、`Bytecode.cpp`、`VMState.cpp`；保持编码、派发与异常表一致 |
| TJS2 转译 | [转译设计](doc/bytecode/mapping.md)、`src/tjsbc/` 及 fixture；检查截断输入和索引边界 |
| 类与属性 | `tests/tjs/class-extends/`、`tests/tjs/property/`、`src/types/Class.cpp`、`Property.hpp` |
| GC 与持有关系 | `Runtime.hpp` 的 create/RootGuard、`gc.hpp` 的 marked/collect；新持有者必须接入标记，sweep 不执行脚本 |
| 字符串借用 | `src/types/Value.hpp` 的 asString；返回指针不能超过所属 Value 的有效期 |
| 调用与异常 | `src/vm/VMState.hpp` 的 DispatchRet/ScriptExceptionCarrier、`VM.hpp` 的 callValue；压帧后正确交还主循环 |
| Chunk | `src/vm/Chunk.hpp` 的 toBytecode、`Chunk.cpp` 的 toHeap；保留惰性编码与常量图根 |
| 热路径 | `src/types/TypeId.hpp`、`VMState.cpp` 的 invokeCallV；遵守判型守卫与 Value 实参按需构造 |
| 优化器 | `src/gen/Optimizer.hpp/.cpp`；Load 可实例化对象，不能作为无副作用加载删除；压缩须重映射跳转、try 与行号 |
| 宿主注册 | [宿主集成](doc/embedding.md)、`Binding.hpp`、`NativeTypeKey.hpp`；区分内部 TypeId 与外部类型身份 |

## 参考与文档维护

语法权威为 [TJS2 リファレンス](https://hydrozoa.felisworks.com/doc/tjs2doc/contents/index.html)。
旧版参考源码约定位于 `../TJSCompiler`；执行器与编译器在 `src/syntax/`，
字节码格式见 `tjsByteCodeLoader.cpp` / `tjsScriptBlock.cpp`。
对拍前确认参考入口接受脚本路径和所需导出参数，见 [FAQ](doc/faq.md#对拍前怎样确认参考引擎可用)。

- 改指令编码同步更新内部字节码文档；改语言行为同步更新语法与兼容性说明。
- 新增模块或文档时更新对应索引；事实性说明引用实现文件，文档使用中文。
- 局部约束放接口或实现注释，使用方法放专题指南，常见排错放 FAQ。
- TODO 只保留未完成项；删除已完成任务与重复修复流水。性能记录保留测量环境和基线。
