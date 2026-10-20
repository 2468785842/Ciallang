# 后续任务清单

> 基于 2026-10 性能分析与 Debug 修复会话的结论整理。
> 当前状态：fib(25) 比旧版 TJSCompiler 慢约 2.3~2.5 倍（性能优化前为 4~5 倍）。

## 一、性能（按预期收益排序）

- [ ] **补全 run0 扁平字节码解释器**（预期收益最大，目标追平/超越旧版）
  - `src/vm/Bytecode.cpp` 的 `encode()` 有 38/55 个操作码未实现（`assert(false)` 占位，如 `DThis`/`DLocal` 等）
  - `VMState::run0()` 的 `CIAL_BYTECODE_OPCODE_ENUMS` 只覆盖 17 个操作码
  - 已就绪的基础设施：`IRGenerator` 已按需调用 `toBytecode()`（惰性编译在 `run0` 取帧点，幂等）；
    `while` 循环在 run0 下曾崩溃，需排查编码正确性
  - 完成后将 `src/vm/VM.hpp` 的 `eval()` 从 `run()` 切换到 `run0()`，删除多态指令执行路径
  - 验证基准：`fib(25)`、300 万次整数循环（基准脚本在分析会话的 bench 目录）
  - 旧版参照：`D:\Users\LiDon\CLionProjects\TJSCompiler`（原版 Kirikiri TJS2）

- [ ] **性能测试偶发段错误**（约 1/8 概率，基线同样存在）
  - `tests/interpreter_test.cpp` 的 fib25 Catch2 benchmark 偶发 SIGSEGV
  - 怀疑方向：`Runtime::create` 返回的临时 `RootGuard` 析构后句柄即移除，
    期间 GC sweep 可能回收仍在使用的对象（`src/runtime/Runtime.hpp` + `src/gc/gc.cpp`）
  - 修复后才能在 CI 开压力模式

- [ ] **Value 字符串布局优化**
  - 现状三层间接：`Value → 堆RefCountPointer → 堆RefCountBox → 堆String`，
    每次字符串 Value 拷贝一次堆分配（`src/types/Value.cpp` 拷贝构造）
  - 目标：引用计数直接挂在 String 头上（单层间接）
  - 注意：字符串路径目前已比旧版快 3.7 倍，此项优先级可放低

- [ ] **编译期解析变量访问**
  - `VMState::getThis`（`src/vm/VMState.cpp`）每次访问 `dynamic_cast` + 线性扫描
    closure 的 `localVars` + 全局哈希查找；应在 IRGenerator 阶段解析为寄存器

## 二、正确性（已知未修）

- [ ] `解释器 - 脚本执行性能` 偶发段错误（同上，性能节的 GC 问题）
- [ ] 前缀 `++`/`--` 未实现（会报 "unknow prefix unary operator"）；
      README 中前缀单目运算符 `~ -- ++ delete typeof # $ + & *` 大部分未实现
- [ ] `for(var ...)` 已支持，但 `for(const ...)` 未支持（tjs.y 的 variable_def_inner 含 const）
- [ ] 空 chunk 时 `Chunk::toBytecode()` / 空脚本行为已优雅返回，但 `main0.cpp`（cial_bc）
  对空脚本仍会走到 assert（未验证）

## 三、语言特性缺口（对照 README）

- [ ] `[]` Array 声明、`%[]` Dictionary 声明、`<% %>` Octet 声明
- [ ] `with` 语句
- [ ] 后缀单目 `[] !`；`A <-> B` 之外的复合赋值边缘情况
- [ ] 标准库：Exception / Array / Dictionary / Date / Math / RegExp
      （String 已部分实现：replace/split 待 RegExp 和 Array）

## 四、工程化

- [ ] vcpkg 在无 dumpbin 的环境下无法自动部署 DLL（构建日志有警告），
      文档中补充手动拷贝说明或改用静态链接 vcpkg triplet
- [ ] `CiallangTests` 已将 CRT 断言重定向到 stderr（`tests/fuzz_regress_test.cpp`），
      CI 里可直接跑 Debug 配置
- [ ] 基准脚本与旧版对照方法沉淀（bench 目录会话结束后未入库，可按需重建）

## 已完成（2026-10 会话）

- [x] 热路径性能优化：Ret 惰性 Err、整数快路径、指令去拷贝、寄存器池移位寻址（`6323cd0`）
- [x] Debug 构建修复：OperatorTrie 悬垂引用、SourceFile EOF 推进、rewind 回退语义、
      字节码按需编译（`bc4b957`）
- [x] for 首子句 var 声明、局部变量 i++ 写回、后缀 -- 操作数顺序、错误路径优雅返回（`672ef45`）
- [x] 回归测试：边界/错误路径 + for/增减用例（`7b15aba`）
- [x] invalidate 的 static Atom 跨 Runtime 缓存 bug（顺序依赖测试失败根因）
- [x] FastRegisterPool `_sp` 未初始化
