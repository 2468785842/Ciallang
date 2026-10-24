# 后续任务清单

> 已完成项只保留一句话摘要，细节见各提交信息与 [doc/bytecode/ciallang-bytecode.md](doc/bytecode/ciallang-bytecode.md)、[AGENTS.md](AGENTS.md)。
> 当前性能：与原版 TJS2（Release）wall-time 比值（Ciallang/TJS min，2026-10-24，55bea90 后）——
> 类实例化 105% / 整数循环 124% / fib(25) 128% / 字符串拼接 184%。
> TJS2 语料对拍（2026-10-24）：TJS_ONLY 0 / MATCH 3（class/function/with 全一致）/
> DIFF 3（test_misc、test_variant 为异常消息格式差异，Ciallang 行为正确；
> test_string 卡正则字面量）/ CIAL_ONLY 1（参考实现自身崩溃）。

## 一、正确性（已知未修）

- [ ] `&` 引用存入全局变量后读取会触发 getter（GGlobal 对 Property 的触发与
      property 声明槽位无法区分；语料中引用均为局部变量，暂记已知限制）
- [ ] GC 期 finalize 语义：当前 ~DataObject 禁止 sweep 中重入解释器；
      若需 TJS2 完整语义需"回收队列 + 安全区 drain"
- [ ] 正则字面量 `/re/`（依赖 RegExp 标准库；test_string 卡点）
- [ ] `main0.cpp`（cial_bc）对空脚本仍会走到 assert（未验证）
- [ ] 未捕获的原生 C++ 异常顶层报错走 unwind 路径，无源码行号（Rlc 未接线）

## 二、性能（剩余方向）

- [ ] fib 剩余差距（~150%）：实参 Push 拷贝与指令条数（TJS 为密集指令编码）；
      可选：参数直传被调帧窗口、调用点内联缓存
- [ ] string_concat 剩余（~180%）：payload+String 单块分配、Value 内联 SSO

## 三、语言特性缺口（对照 README）

- [ ] Array 标准库剩余：sort/split/save/load/pack 系/Assignment 联动
- [ ] Dictionary 标准库剩余：save/load（BinarySerializer）
- [ ] `(const)[...]` / `(const)%[...]` 的只读共享实例语义（当前按运行时新建处理，行为兼容）
- [ ] 十六进制浮点字面量 `0x1.07CCp14`、二进制浮点 `0b1.1`（test.tjs 后段依赖）
- [ ] `with` 语句
- [ ] 标准库：Date / Math / RegExp
      （String 已部分实现：replace/split 待 RegExp；
       Array/Dictionary 已支持字面量、下标读写与核心方法，sort/split 待补）
- [ ] const 只读性检查（当前 const 与 var 等价，仅语法接入）

## 四、工程化

- [ ] vcpkg 无 dumpbin 环境无法自动部署 DLL：补手动拷贝说明或改静态 triplet
- [ ] CI 可跑 Debug 配置（CRT 断言已重定向 stderr，压力模式已可开）

## 已完成（摘要）

- 2026-10-24 Array/Dictionary 批次：`[...]`/`%[...]` 字面量（sym: 与 "k" >= v 两种键形式，
  => 词形即 >= 由上下文区分）、通用下标读写 `a[i]`（新增 NewArray/NewDict/GetIndex/SetIndex，
  指令 64→68；数组负下标/越界写自动扩展/越界读报脚本异常；字符串下标保持 charAt 语义）、
  Array/Dictionary 运行时容器（GC 接入）+ 方法表（push/pop/unshift/shift/insert/remove/
  erase/reverse/join/assign/clear/erase + count/length 属性）、全局 Array/Dictionary
  类对象（Dictionary.assign incontextof 惯用法）；连带修复 ChgThis 不认 NativeFunction 段错误

- 2026-10-24 property 引用批次：前缀 `&`/`*`（新增 GPropRaw/GRefRaw/DPropRaw/DerefGet/DerefSet，
  指令 59→64）、成员后缀 `obj.prop++`（getter+setter 链）、后缀 `i++` 旧值语义；
  连带修复 super.<Ctor>() 段错误（_lastSuperClass 未记录时按 extends 解析 + 构造器
  经 getProp 可访问返回类对象）、GLocal/DLocal 改纯读写（局部变量=VM_CP 语义，
  Property 可作一等值传递）、DProp 改 setter 感知（槽位为 Property 调 setter）；
  test_class 语料 MATCH

- 2026-10-24 兼容性批次：前缀 `++ -- # $ delete`、`const` 声明与 `for(const)`、
  `str.length` 原生属性、后缀 if 绑定修正（假条件整体不赋值）、三元优先级修正、
  `#` 对非字符串先字符串化、octet 算术改为报错、一元 `+` 转数值、
  字符串→数值支持负号与小数写法、`void` 字面量发射、instanceof 基础类型匹配、
  大整数字面量不经 double 往返、Exception 内建类（TJS 源自举）、
  原生异常转脚本可捕获异常（包装为 Exception 对象）、
  未初始化 `var a,b,c;` 寄存器别名修复、函数默认参数 TestVoid 守卫（修实参覆盖）+
  构造器 arity 用 FuncMeta::arity 的修复、sLEB128 maxLen 8→10（i64 字面量）
- 2026-10-22 语法批次：`^ | &` 及复合赋值、后缀 `expr if cond`、下标 `s[i]`、`typeof`（新增 TypeOf 指令）、`(int)x` 转型、前缀 `+`/`~`、octet 字面量、函数提升、`catch()`；连带修复 zigzagDecode 负立即数差 1、Constant 拷贝语义、octet 词法
- 2026-10-22 二轮热点：GGlobal 零拷贝、池水位线收缩、dispatch 整数特化、Ret 整数直写、this 快通道
- 2026-10-22 崩溃修复：generateFuncMeta 空 Opt 段错误、未捕获异常静默 terminate、异常后 VM 状态残留
- 2026-10-21 性能四项：run0 扁平字节码 55/55 唯一路径、GC 根体系（MarkRootProvider + 6 处标记缺陷）、Value SharedPayload、thisData 缓存
- 2026-10-21 设施：bench/ 对拍脚本、CMake 化基准与 conformance 报告、CMakeUserPresets、main.cpp 补 initNativeMethod
- 2026-10 上旬：Ret 惰性 Err、整数快路径、Debug 构建修复、for(var)/增减运算符、Atom 跨 Runtime 缓存、FastRegisterPool `_sp`
