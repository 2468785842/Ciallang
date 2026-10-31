# 按需 SSA 分析

`cial --dump-ssa script.tjs` 编译脚本并导出顶层与嵌套函数的 SSA 文本。它与 `--dump-cfg` 共用编译和函数遍历入口，不执行被分析脚本。默认 `IRGenerator::parseAst()` 优化管线不额外构造 SSA，现有 TAC 与字节码仍直接供解释器使用（`src/cli/Application.cpp`、`src/cli/Compilation.cpp`、`src/gen/IRGenerator.cpp`）。

`SsaBuilder` 基于基本块 CFG 构造直接支配者、支配树、支配边界和活跃性裁剪的 phi，再沿支配树解析每个寄存器使用点的到达定义及反向 def-use 链。phi 的输入对应前驱块顺序；循环需要同时保留进入循环前的定义与回边携带的定义。分析只读，不改写指令或重排异常区间（`src/gen/SSA.hpp`、`SSA.cpp`）。

```sh
build/Windows/Debug/cial.exe --dump-ssa script.tjs
```

输出使用块号、TAC 下标和定义引用，适合检查分支汇合与循环中的数据来源。这是 SSA 分析基础，尚未把执行 IR 切换为 SSA，也不做 phi 消除、寄存器重新分配或基于 SSA 的代码优化。可选整数 JIT 直接读取扁平字节码，不使用此分析结果（`src/gen/SSA.cpp`、`src/vm/IntegerJit.cpp`）。

独立集成验收检查只在返回汇合处活跃的变量、入口参数定义、循环初始值与回边值，以及分析前后 TAC 不变。构造层的其他用例见 `tests/ssa_test.cpp`；集成用例见 `tests/ssa_integration_test.cpp`。

## 前向汇合传播与后续 IR 评估

整数常量 pass 已将全部前向前驱的相同常量取交集；任一前驱缺失、值不同、
带未遍历回边或处于 try 保护区则不继承。异常边可能来自 try 块中途，不能用正常出口状态
代表异常入口。保留块内折叠、溢出检查和动态运算屏障；不移动指令，压缩仍统一重映射
跳转、异常表与行号（`src/gen/Optimizer.*`、`tests/optimizer_test.cpp`）。

本批评估结论：暂不替换执行 IR，也不启用循环代码移动。现有 SSA 只描述寄存器到达定义，
动态成员、原生调用、局部闭包和可能实例化对象的 Load 仍需内存/副作用模型；仅依据 SSA
值依赖不能证明循环外提安全。执行 IR 替换还需 phi 消除、临界边拆分、寄存器分配及
异常点映射，且现有整数 JIT 不消费 SSA。当前先落地可独立验证的汇合传播。

后续研究的进入条件是：具体语料能显示剩余指令开销，具备副作用/异常边模型，并在同机
Release 对照中取得超出 A/A 噪声的收益。已有回边、不同前驱、外部结果寄存器、Load
不可删除和行号/异常表回归继续保留。这是评估结论，不表示 SSA 执行 IR 或循环变换已实现。
