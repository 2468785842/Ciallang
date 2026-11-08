# 反编译器函数内 CFG 状态机降级首版

Status: implemented

## 目标与完成范围

结构化恢复失败时，函数内受支持的指令集可以按基本块状态机恢复，包括多入口
强连通分量（不可约 CFG），不再依赖 if/while/switch 区域归约成功。
这是 M5 的 CFG 降级首版完成项，不代表整个 M5 或全部 TAC 协议都已完成。

当前 51 类指令包括标量常量、寄存器拷贝、全局/this 读写、算术/位运算/比较、
整数/实数转换、typeof、Test、条件/无条件跳转及 return。Load 仅接受
None/Real/Atom 常量。具体白名单以 ExprBuilder::runStateMachine 为准。

以下保持 Unsupported：顶层无局部作用域、缺少 FuncMeta、重复参数名、
参数与动态名字查找冲突、with/eval、变长/展开实参、
函数/类/property 声明常量，以及其它不在白名单中的指令。闭包提升和
声明常量是后续协议扩展，不能因为 CFG 已可遍历就猜测它们的生命周期或隐式
写入。首版记录时保持 Unsupported 的 throw handler 与调用族/容器/成员语句/
字符串及函数字面量 Load 已由后续批次支持（见
[异常区域实现记录](2026-11-07-decompiler-exception-regions.md)）；仍拒：
ChgThis、TestVoid、With/EvalStr、CallExpand/CollapseArgs、Super、
ClassMeta/PropMeta 声明常量。

## 实现与不变量

- [ExprBuilder.cpp](../../../../src/decompile/ExprBuilder.cpp)：优先原结构化路径，
  失败后才尝试状态机；失败时保留原原因并补充降级拒绝原因。
- 每个物理寄存器对应函数局部存储，入口参数先复制到对应槽，再清除入口
  参数，避免形成额外的旧值保活副本。赋值在原指令位置执行；不做逆融合、
  死加载删除、跨块内联或值合并，不将未执行路径上的值用于后续读取。
- 局部和状态名字避开参数、原局部与所有 Atom；不存在合成全局变量。
- 每次 dispatch 执行一个基本块，按原跳转目标或 fallthrough 更新状态。
  JmpE/JmpNE 的方向显式区分；flag 比较仍要求相邻消费。return 直接退出函数。
- [ExprBuilder.hpp](../../../../src/decompile/ExprBuilder.hpp) 新增 synthetic
  标志；框架、寄存器声明和参数清除不伪造 TAC 来源。实际寄存器计算和跳转
  保留来源。Decompiler 两条行表装配路径均处理合成行，不扩展成越界区间。

## 验证与证据

基线 ce18c00 + 前批未提交的参考嵌套 switch / ExprFunction 修复；本批 dirty。
Windows Debug：clang++，CLI/TESTS/FORMATTER ON、JIT OFF，x64-windows-static，
vcpkg D:/cpp_tool/vcpkg。构建命令 cmd /c out\m5-build.cmd，退出 0；
最终日志 out/m5-cfg/build-accepted.log。

[decompile_test.cpp](../../../../tests/decompile_test.cpp) 新增/扩展：

- 双入口 SCC 的循环携带值，和原始 TAC VM 及独立逻辑结果逐输入比较；
  参数 x/v0、全局 v1 写入变体同时检查符号冲突与可观察结果。
- 8 组有界生成图、32 组输入，改变权重和迭代上限，与原始 VM 及独立交替
  求和模型一致；测试确认确实生成 switch 状态机，而不是沿用旧结构化输出。
- 原始/恢复函数接收对象实参，返回后强制 GC，两个对象均如期回收。
- 四种相邻 flag 比较的真跳/假跳语义；顶层、eval、名字冲突和调用协议拒绝。
- 合成行的 TAC 空区间、所有行的区间上界均检查。

直接运行 CiallangTests.exe "decompile M5*"：**552 assertions / 4 cases 通过**，
日志 out/m5-cfg/m5-accepted.log。Windows CTest 仍注册 245 项；全量
ctest --test-dir build/Windows/Debug --output-on-failure --no-tests=error -j 10
**245/245 通过**（22.00s，out/m5-cfg/ctest-accepted.log）。
clang-format 20.1.0 dry-run 与 git diff --check 通过。

参考 TJSCompiler commit 40a736c，二进制 SHA-256
8f370f2bb7460c8b7b8090a8bc71b7d0fb94a25e98ee2ff20d01bf3df358c60b。
从成功测试日志提取实际生成的 4 份函数文本（参数/全局写入组合），与独立逻辑
参考程序逐输出记录比较，4/4 一致、参考进程退出 0。
out/m5-cfg/reference-check.py / reference-report.json 保存输入哈希和原始输出。
原始不可约输入是 C++ 构造 TAC，不是参考引擎字节码，因此没有宣称参考原字节码
直接加载环通过，也没有把参考解释器生成的可约 CFG 冒称为原始不可约 CFG。

## 首轮失败与修正

首轮全量 CTest 为 244/245；s7_ext 的行表测试用 dict 按单元名建索引，两个类
的 describe 方法相互覆盖，导致合法的行 8 覆盖行 4。改为汇总同名单元，
manifest 同时要求行 4 和行 8，未跳过检查或改源位置数据。最终全量通过。

## 后续边界

异常区域与重抛、调用族/容器/成员语句/字符串及函数字面量 Load 已由后续批次
支持（见[异常区域实现记录](2026-11-07-decompiler-exception-regions.md)）；
闭包提升与声明常量需各自建立状态保存和求值时机协议后，才能扩展本白名单。
M5 整体状态仍见活动 note 与方案当前状态表。
