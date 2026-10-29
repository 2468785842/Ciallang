# 执行路径分析

`cial --trace-execution script.tjs output.dot` 执行脚本，并把实际执行过的指令和相邻执行位置导出为 Graphviz DOT。程序输出仍写到 stdout；记录写到指定文件。未捕获的运行期错误返回非零退出码，同时保存错误前的路径（`src/main.cpp`）。

节点含 chunk 身份、扁平字节码偏移、操作码、函数名、可用时的源码行号和派发次数。边标注相邻执行位置的转移次数；同 chunk 内目标偏移不大于起点的边标红。跨函数调用、返回、异常恢复和原生回调形成真实的执行顺序边；它们不等同于静态 CFG 边，红边也不保证是 CFG 意义的自然循环（`src/vm/ExecutionTrace.hpp`、`src/vm/VMState.cpp`）。

记录的是尝试派发的指令次数，不是完成指令数、耗时采样或有序事件日志。只输出运行中观察到的节点和边；没有执行到的分支不会出现在图中。不同的最外层 `run0()` 调用之间不连接虚构的边；嵌套 `run0()` 回调保持顺序连续（`src/vm/ExecutionTrace.hpp`）。

普通运行默认不启用。宿主可调用 `VMState::setExecutionTrace()` 借用记录器，调用方须保证其覆盖全部执行期，结束后可设置为空。记录器保存文本和数值快照，不持有 GC 对象指针；Chunk 的记录身份按需分配。内存随观察到的代码位置和边数增长，而非总执行步数；启用记录会增加运行开销，不能用该模式衡量普通执行性能（`src/vm/Chunk.hpp`、`src/vm/ExecutionTrace.hpp`）。

`tests/execution_trace_test.cpp` 覆盖精确循环计数、独立 eval 边界、关闭记录、递归与异常、GC 后导出，以及原生回调的嵌套执行。

静态结构使用 `cial --dump-cfg script.tjs`，动态路径使用上述命令。需要图片时可用本机 Graphviz 将 DOT 转为 SVG；程序本身不依赖 Graphviz。
