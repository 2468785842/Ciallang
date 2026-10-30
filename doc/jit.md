# 可选整数 JIT / Trace JIT

本机代码后端使用 AsmJit，由 vcpkg 的可选 `jit` feature 管理。配置 `-DCIAL_ENABLE_JIT=ON` 会自动启用该 feature；目前只支持 x64。默认构建不依赖 AsmJit，默认运行仍为解释执行（`CMakeLists.txt`、`vcpkg.json`）。

```sh
cmake --preset "Windows Release" -B build/Windows/Jit -DCIAL_ENABLE_JIT=ON
cmake --build build/Windows/Jit --target cial CiallangTests
build/Windows/Jit/cial.exe --jit script.tjs
build/Windows/Jit/cial.exe --trace-jit script.tjs
```

普通 JIT 在首次遇到受支持位置时编译最长 32 条无分支整数指令段。Trace JIT 在同一位置达到热点阈值后记录实际执行路径，至回到起点或累计 32 条指令时编译；记录可跨条件跳转，生成的机器码会检查观察到的分支结果。默认热点阈值为 32，宿主可通过 `VMState::setJitMode(mode, threshold)` 修改（`src/vm/IntegerJit.cpp`、`VMState.hpp`）。

支持立即整数加载、局部寄存器拷贝、Add/Sub/Mul、整数比较、Test 与融合比较；热路径另外支持直接和条件跳转。外部输入寄存器必须是 Integer。生成的代码使用独立 i64 暂存窗口，检查有符号溢出及路径守卫；只有完整成功才提交所有输出、ZF 和 PC。失败时原状态不变，继续解释器。代码缓存最多 128 项，热点计数位置最多 512 项，身份使用 Chunk 的惰性数值 ID，不保留 GC 指针（`src/vm/IntegerJit.cpp`、`Chunk.hpp`）。

带 try/catch 的整个 chunk、动态对象操作、属性、调用等不支持区域保守回落。编译失败也保留解释路径。启用精确执行路径记录时优先使用解释循环，避免跳过指令使计数失真。命令行在 stderr 报告编译数、原生执行数、类型退出、路径/溢出退出和已记录 trace 数，可确认实际命中；未达到热点阈值的短程序可能完全不编译（`src/cli/Application.cpp`、`src/cli/Compilation.cpp`、`VMState.cpp`）。

这是初始整数后端，不是全语言 JIT：不编译对象模型、浮点、原生调用或 SSA 结果，也不保证每个脚本更快。启用会引入编译、暂存和类型检查成本。该模式独立于默认解释器的性能结论（`src/vm/IntegerJit.cpp`）。

`tests/jit_test.cpp` 验证实际机器码执行及全部输出提交、类型变化、溢出时的无部分提交、循环分支退出、try 区间回落和精确记录的优先级。未启用后端时覆盖明确的不可用错误；启用后端的构建必须另外运行 CTest，不能只凭默认构建通过宣称 JIT 已验证。

AsmJit API 参考：[Compiler](https://asmjit.com/doc/classasmjit_1_1x86_1_1Compiler.html)。实际 API 以 vcpkg 安装版本的头文件为准，避免跟随上游接口更名造成漂移。
