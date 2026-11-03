# FAQ

## 是否完全兼容 Kirikiri

Ciallang 实现 TJS2 语言与标准库，不包含 Kirikiri 的完整宿主 API、资源系统或游戏引擎。
字节码走加载后转译执行的路线。字符串内部采用 UTF-8/WTF-8，基础索引与正则位置按 UTF-16 码元计数；
这不等于已通过全部 TJS2、宿主分派或真实游戏验收。
Scripts 等宿主接口也有明确的实现范围，见[兼容性说明](compatibility.md)。

## JIT 或 SSA 是否意味着已经达到原版性能

JIT/Trace JIT 目前只覆盖受限的 x64 整数路径，默认关闭；SSA 用于按需分析，尚未成为执行 IR。
功能测试通过不能替代性能测量。比较性能时使用同一机器、构建配置、语料与统计口径；
避免将不同批次比值相乘。方法与历史数据见[性能记录](performance.md)、[JIT](jit.md) 和 [SSA](ssa.md)。

## 宿主持有 Value 或 shared_ptr 就能保住脚本对象吗

普通 C++ 局部变量、容器、lambda 捕获和外部 `shared_ptr` 不会自动进入脚本 GC 根集合。
保留 `RootGuard` / `VM::Handle`，或为绑定 payload 提供 `.trace()`；Runtime 必须覆盖所有句柄的生命周期。
裸指针及 `Value::asString()` 返回值是借用，不能超过所属对象或值的有效期。
API 与示例见[宿主集成](embedding.md)，实现约束见 `src/runtime/Runtime.hpp`、`src/types/Value.hpp`。

## GC 池耗尽时是什么行为

GC 池默认 512 个固定节点（每节点 512 字节，对象数而非字节数受限）。标记清扫后仍无空闲节点时：
有待终结对象先扩池 64 节点；仍不足则抛出 `std::runtime_error`，由宿主按运行期错误处理
（CLI 报告错误并以非零码退出），不会 abort 或解引用空节点。实现见 `src/gc/gc.cpp` 的 `findIdleNode`；
存活对象数接近池上限的行为由 `tests/runtime_lifetime_test.cpp` 覆盖。

## Windows 构建为什么不需要复制依赖 DLL

仓库预设使用 `x64-windows-static` 与静态 CRT。自定义 triplet 时要自行保持依赖和运行库一致
（`CMakePresets.json`）。Windows 的 CTest 将整个 Catch2 套件注册为一项，避免中文用例名经过
ANSI 代码页后过滤失配；其他平台按用例注册，因此测试项数量不应直接比较（`tests/CMakeLists.txt`）。

## ASan 报错或无法启动怎么办

先用同一编译器验证最小程序，区分工具链启动故障与项目代码错误。ASan 地址检查和
LeakSanitizer 泄漏检查是不同功能，不应因泄漏检查受限就推断整个平台不支持 ASan。

### ASan 在 macOS ARM64 上无法启动

Apple clang 17.0.0 与 macOS 26.5.1 的一次验证中，空 `main` 也在运行库初始化阶段死锁；
该现象与 [LLVM #200447](https://github.com/llvm/llvm-project/issues/200447) 的报告吻合，
不能推广为所有 ARM64 环境都不可用。可先运行：

```sh
printf 'int main() { return 0; }\n' > /tmp/cial-asan-smoke.cpp
clang++ -fsanitize=address -g /tmp/cial-asan-smoke.cpp -o /tmp/cial-asan-smoke
ASAN_OPTIONS=detect_leaks=0 /tmp/cial-asan-smoke
```

空程序也无法启动时，应先更换或修复工具链；这次运行不能记为项目测试通过。

### Windows ASan 与异常处理

Clang 19 Windows 运行库曾在 C++ 异常路径崩溃，纯标准库最小程序同样复现。
项目已验证过 MSVC `/fsanitize=address` 组合；静态 CRT、容器注解设置及 ASan 运行库部署
见[构建指南](building.md#asanwindows-msvc)。

### Windows 上为什么跑不了 libFuzzer 有界 fuzz

除上述 ASan 异常问题外，Windows Clang 还有一处独立故障：只要链接任一 `clang_rt`
运行库（含纯覆盖率的 `clang_rt.fuzzer*`），`catch(const std::runtime_error &)` 这类
对 std 类型的具名捕获在 RTTI 匹配阶段会跳入类型描述符数据段直接崩溃；`catch(...)`
与项目自定义类型不受影响，Linux 下同一构建完全正常。已验证该现象与 Clang 18/19、
MSVC CRT 14.29/14.44、lld-link/link.exe 及静态/动态运行库组合均无关。
`src/parser/Lexer.cpp` 需捕获预处理抛出的 `std::runtime_error`，且 `fuzz/tjs.dict`
含 `"@set("`，语料回归数秒内必然触发，因此 fuzz 构建只在 Linux（含 WSL）运行，
流程见[构建指南](building.md#有界-fuzz)。

## 对拍前怎样确认参考引擎可用

TJSCompiler 的旧入口曾使用固定脚本路径；`d8044c3` 已提交 `TJSCompiler <script>` 和
`--dump <script> <output>` CLI（参考仓库 `src/main.cpp` / `CMakeLists.txt`）。不同版本的入口能力仍需实际确认。
指定 `CIAL_TJS_EXE` 前，检查程序确实执行输入脚本并支持所需导出接口。
`bench/bytecode_conformance.py --reference` 会探测参考 CLI；已有源码与字节码 fixture 可独立使用，
无需参考程序。命令与报告语义见[构建指南](building.md#测试与基准)和[转译文档](bytecode/mapping.md#9-对拍与工具链)。

## 编辑器告警需要逐条照改吗

先核对控制流与类型约束。受 `TypeId`/opcode 保护的热路径转换、内部脚本异常载体、
测试中的自赋值和生命周期检查都有明确用途；拼写、const 或 ranges 建议不等同于运行时缺陷。
局部协议见 `src/types/TypeId.hpp`、`src/vm/VMState.hpp`；历史诊断数据保留在 [reports/](reports/)。

## Windows CI 的深数组压力测试为什么配置较大栈？

当前 `Array::marked()` 等对象图遍历仍有 C++ 递归，不能把深数组测试通过理解为无栈深限制。
2 万层数组用例在 Linux 的 1 MiB 栈子进程可复现溢出、8 MiB 可通过；Windows 默认线程栈更小。
`CiallangTests` 在 Windows 显式预留 16 MiB，保证压力测试有明确资源预算；运行库、`cial`、
`cial-fmt` 与宿主 SDK 的默认栈未因此调整。后续应将对象图标记迁到显式工作队列。
