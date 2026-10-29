# 按需 SSA 分析

`cial --dump-ssa script.tjs` 编译脚本并导出顶层与嵌套函数的 SSA 文本。它与 `--dump-cfg` 共用编译和函数遍历入口，不执行被分析脚本。默认 `IRGenerator::parseAst()` 优化管线不额外构造 SSA，现有 TAC 与字节码仍直接供解释器使用（`src/main.cpp`、`src/gen/IRGenerator.cpp`）。

`SsaBuilder` 基于基本块 CFG 构造直接支配者、支配树、支配边界和活跃性裁剪的 phi，再沿支配树解析每个寄存器使用点的到达定义及反向 def-use 链。phi 的输入对应前驱块顺序；循环需要同时保留进入循环前的定义与回边携带的定义。分析只读，不改写指令或重排异常区间（`src/gen/SSA.hpp`、`SSA.cpp`）。

```sh
build/Windows/Debug/cial.exe --dump-ssa script.tjs
```

输出使用块号、TAC 下标和定义引用，适合检查分支汇合与循环中的数据来源。这是 SSA 分析基础，尚未把执行 IR 切换为 SSA，也不做 phi 消除、寄存器重新分配或基于 SSA 的代码优化。可选整数 JIT 直接读取扁平字节码，不使用此分析结果（`src/gen/SSA.cpp`、`src/vm/IntegerJit.cpp`）。

独立集成验收检查只在返回汇合处活跃的变量、入口参数定义、循环初始值与回边值，以及分析前后 TAC 不变。构造层的其他用例见 `tests/ssa_test.cpp`；集成用例见 `tests/ssa_integration_test.cpp`。
