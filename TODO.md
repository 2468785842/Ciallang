# 后续任务清单

> 已完成项只保留一句话摘要，细节见各提交信息与 [doc/bytecode/ciallang-bytecode.md](doc/bytecode/ciallang-bytecode.md)、[AGENTS.md](AGENTS.md)。
> 当前性能：与原版 TJS2（Release）wall-time 比值（Ciallang/TJS，2026-10-22）——
> 类实例化 111% / 整数循环 111-115% / fib(25) 148-153% / 字符串拼接 175-183%。

## 一、正确性（已知未修）

- [ ] `Exception` 内建类（test_misc/test_class 运行期依赖；含 `e.message`）
- [ ] `&property1` / `*p` 引用语义（test_class；需 property 引用透传设计）
- [ ] GC 期 finalize 语义：当前 ~DataObject 禁止 sweep 中重入解释器；
      若需 TJS2 完整语义需"回收队列 + 安全区 drain"
- [ ] 前缀 `-- ++ delete # $ *` 未实现
- [ ] `for(const ...)` 未支持
- [ ] 正则字面量 `/re/`（依赖 RegExp 标准库）
- [ ] `str.length` 属性（当前仅方法表）
- [ ] `main0.cpp`（cial_bc）对空脚本仍会走到 assert（未验证）

## 二、性能（剩余方向）

- [ ] fib 剩余差距（~150%）：实参 Push 拷贝与指令条数（TJS 为密集指令编码）；
      可选：参数直传被调帧窗口、调用点内联缓存
- [ ] string_concat 剩余（~180%）：payload+String 单块分配、Value 内联 SSO

## 三、语言特性缺口（对照 README）

- [ ] `[]` Array 声明、`%[]` Dictionary 声明
- [ ] `with` 语句
- [ ] 后缀单目 `!`
- [ ] 标准库：Exception / Array / Dictionary / Date / Math / RegExp
      （String 已部分实现：replace/split 待 RegExp 和 Array）

## 四、工程化

- [ ] vcpkg 无 dumpbin 环境无法自动部署 DLL：补手动拷贝说明或改静态 triplet
- [ ] CI 可跑 Debug 配置（CRT 断言已重定向 stderr，压力模式已可开）

## 已完成（摘要）

- 2026-10-22 语法批次：`^ | &` 及复合赋值、后缀 `expr if cond`、下标 `s[i]`、`typeof`（新增 TypeOf 指令）、`(int)x` 转型、前缀 `+`/`~`、octet 字面量、函数提升、`catch()`；连带修复 zigzagDecode 负立即数差 1、Constant 拷贝语义、octet 词法
- 2026-10-22 二轮热点：GGlobal 零拷贝、池水位线收缩、dispatch 整数特化、Ret 整数直写、this 快通道
- 2026-10-22 崩溃修复：generateFuncMeta 空 Opt 段错误、未捕获异常静默 terminate、异常后 VM 状态残留
- 2026-10-21 性能四项：run0 扁平字节码 55/55 唯一路径、GC 根体系（MarkRootProvider + 6 处标记缺陷）、Value SharedPayload、thisData 缓存
- 2026-10-21 设施：bench/ 对拍脚本、CMake 化基准与 conformance 报告、CMakeUserPresets、main.cpp 补 initNativeMethod
- 2026-10 上旬：Ret 惰性 Err、整数快路径、Debug 构建修复、for(var)/增减运算符、Atom 跨 Runtime 缓存、FastRegisterPool `_sp`
