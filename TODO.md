# 后续任务清单

> 基于 2026-10 性能分析与 Debug 修复会话的结论整理；一节已于 2026-10-21 完成并更新。
> 当前状态：run0 扁平字节码为唯一执行路径；Release 下 fib(25) ≈ 58ms、300 万次整数循环 ≈ 412ms
> （本次优化前分别为 ≈ 77ms / ≈ 547ms，提升约 25%）。
> 与原版 TJS2 对拍（TJSCompiler Release，三方 wall-time 交替实测，2026-10-21）：
> fib(25) TJS≈54ms / 旧 Ciallang≈88ms / 新 Ciallang≈75ms（新差距从 1.63x 缩到 1.39x）；
> 3M 循环 TJS≈670ms / 旧≈1080ms / 新≈793ms（从 1.61x 缩到 **1.18x**）。
> TJSCompiler Release 构建方法：cmake -DBUILD_SHARED_LIBS=OFF（否则 onig 链接失败），
> 产物另存 bin/Windows/TJSCompiler-release.exe。

## 一、性能（✅ 全部完成 2026-10-21）

> 简要留痕（细节见 [doc/bytecode/ciallang-bytecode.md](doc/bytecode/ciallang-bytecode.md)、
> [AGENTS.md](AGENTS.md) 坑位、以及对应提交信息）：
>
> - **run0 扁平字节码补全为唯一执行路径**：55/55 编码器与 dispatch；跳转回填列表化；
>   try 表换算到字节码 pc 空间；删除 `run()`/`Instruction::execute` 多态路径。
>   顺带修复 while 崩溃根因（`decodeUleb128` 位移单位 bug，≥8192 立即数误抛）与
>   IfStmtNode else 跳转误用 JmpNE。
> - **偶发段错误根除**（8 连跑全量测试无失败）：执行期 GC 此前只标记 RootGuard 句柄——
>   引入 `MarkRootProvider` 接入寄存器池/调用栈/全局对象；另修 `freeCallFrame` 越界写、
>   以及 GC 真正工作后暴露的 6 处标记协议缺陷（漏标 constructor/initDefaultVal/PropMeta、
>   `Property::marked` 漏调基类、marked() 缺幂等守卫、寄存器扫描类型不判、
>   sweep 中 finalize 重入压僵尸帧）。
> - **Value 字符串/字节串共享负载块**（`SharedPayload`）：拷贝零堆分配，取代三层
>   RefCountPointer。
> - **变量访问热路径**：`CallFrame::thisData` 缓存消除 getThis/setThis 的 dynamic_cast；
>   完整编译期解析需类系统静态化后再做。
> - **基准（Release，三方 wall-time 交替对拍）**：fib(25) 旧 88ms→新 75ms
>   （与 TJSCompiler Release 54ms 差距 1.63x→1.39x）；3M 循环 旧 1080ms→新 793ms
>   （与 TJS 670ms 差距 1.61x→**1.18x**）。

## 二、正确性（已知未修）

- [x] `解释器 - 脚本执行性能` 偶发段错误（已修，见一节；CI 可开压力模式）
- [ ] GC 期 finalize 不再触发（~DataObject 已禁止重入解释器）；
      若需 TJS2 完整语义（GC 回收对象也调 finalize），需实现"回收队列 + 安全区 drain"
- [ ] 二元 `^ | &` 运算符 Parser 未注册（IRGenerator 的 BXor/BOr/BAnd 不可达，
      报 "isn't support assign operator"）；`str.length` 属性未实现（仅方法表）
- [x] `tests/test_files/tjs2/test_class.tjs` 段错误（已修 2026-10-22：generateFuncMeta
      对 parseAst 失败只 assert，Release 下空 Opt 解引用；另修未捕获运行期异常
      静默 terminate 与异常后 VM 状态残留，`9123362`）
- [x] conformance stdout 语义对拍（已通 2026-10-22：cial 注入 Debug 垫片 +
      Conformance.cmake 归一化逐行比对，`22eb483`；tjs2 语料基线 MATCH=2/DIFF=4；
      TJSCompiler 侧 DebugInterface 控制台补换行——在其工作区未提交）
- [x] IfStmtNode else 跳转误用 JmpNE（已修 2026-10-21：真假分支曾都会执行）
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
- [x] 基准与对拍设施入库（CMake 化，2026-10-21）：
      `bench/*.tjs` 双引擎基准脚本（throw 自校验）、`cmake/Bench.cmake`（`cmake -E time`
      交替对拍，min/mean 报告落盘 bench/results/）、`cmake/Conformance.cmake`
      （tjs2 语料双引擎成败对拍报告）。用法：`cmake --preset "Windows Local Bench" &&
      cmake --build --preset "Windows Local Bench"`（bench 目标）；
      `--target conformance-report` 出兼容性报告。TJS 路径经 UserPresets 的
      CIAL_TJS_ROOT 环境变量注入
- [x] 第二轮热点优化（2026-10-22，commit 序列见 git log）：
      GGlobal 零拷贝（findPropRef）、池水位线收缩、dispatch 层整数特化
      （Add/Sub/Mul/LT 绕开 Ret 装配）、Ret 整数直写、全局函数 this 快通道。
      比值演进：loop 123%→113%、fib 165%→148%、class 113%→111%、string 228%→178%
- [ ] 剩余 fib 差距（148%）：微基准定位每层调用的实参 Push 拷贝与指令条数
      （TJS 为密集指令编码）；可选方向：参数直传被调帧窗口（免 Push/读回）、
      调用点内联缓存（CIC 缓存 Function 指针）
- [ ] string_concat 剩余（178%）：payload+String 单块分配、Value 内联 SSO
- [x] CMakeUserPresets.json 固化本机工具链路径（clang/rc/ninja 完整路径，
      无 VS 环境变量的终端可直接 `cmake --preset "Windows Local Release"`；
      文件已加入 .gitignore 不入库）

## 已完成（2026-10-21 性能会话）

- [x] 一节四项全部完成（摘要见上）；另：main.cpp 补 initNativeMethod 与 stdout 无缓冲、
      CMakeUserPresets.json 固化本机工具链（不入库）

## 已完成（2026-10 会话）

- [x] 热路径性能优化：Ret 惰性 Err、整数快路径、指令去拷贝、寄存器池移位寻址（`6323cd0`）
- [x] Debug 构建修复：OperatorTrie 悬垂引用、SourceFile EOF 推进、rewind 回退语义、
      字节码按需编译（`bc4b957`）
- [x] for 首子句 var 声明、局部变量 i++ 写回、后缀 -- 操作数顺序、错误路径优雅返回（`672ef45`）
- [x] 回归测试：边界/错误路径 + for/增减用例（`7b15aba`）
- [x] invalidate 的 static Atom 跨 Runtime 缓存 bug（顺序依赖测试失败根因）
- [x] FastRegisterPool `_sp` 未初始化
