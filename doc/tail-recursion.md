# 数值自尾递归消除

通过 `cial --tail-recursion script.tjs` 或 `VMState::setTailRecursionEnabled(true)` 显式启用，默认关闭。`VMState::dispatchTailCallV()` 在实际被调函数元数据等于当前函数、后继为返回本次调用结果的 `Ret` 时，尝试原地复用当前帧。先快照所有参数，再清空寄存器窗口、写入参数并从函数入口重新执行；不改变父帧返回槽和参数求值顺序。没有新增 TAC 或字节码操作码（`src/vm/VMState.cpp`）。

目前仅优化可保守证明帧局部状态不可被外界观察的数值自递归子集：无 try/collapse，实参数严格等于形参数，无捕获帧，this 为全局或 void；函数体仅允许数值计算、分支、拷贝和读取自身函数名，所有调用目标寄存器只能由读取自身名字定义。所有其他全局访问、属性、容器、with、eval、函数声明与未知指令均回落原调用路径。运行时再次检查全局槽仍直存实际被调函数、寄存器与参数仅含 int/real/void（仅函数目标寄存器可持有自身对象）。方法、动态重绑定、对象参数、非尾位置和外部调用不会因此消除帧（`src/vm/VMState.cpp`、`src/vm/Constant.hpp`）。

这不是通用尾调用优化，也不处理互递归、展开/省略调用。`tailCallsEliminated()` 提供实际消除次数，测试用它与结果一起验证两万层自递归不再受 1024 层调用栈上限限制。参数交换、局部变量初始化、try/catch、全局副作用、对象参数、非尾 Fibonacci 和名称重绑定由 `tests/tail_recursion_test.cpp` 覆盖。

普通解释循环在模板实例化时移除尾递归检查，不满足条件的程序也回落普通调用，错误与异常处理协议不变。性能验证使用同机 Release 构建，未启用执行路径记录；基准和验证记录见 `doc/performance.md`。
