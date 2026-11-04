# 嵌套函数自由变量（外层局部作用域链）

Status: rejected — 参考 TJS2 同样拒绝该形态，非兼容缺口，不为对齐而实现

## 问题（原提案）

`function outer() { var a = 40; function inner() { return a + 2; } return inner(); }`
运行期报 "in global object Not found property"。原待办假设 TJS2 支持嵌套作用域链、
Ciallang 缺失，考虑实现捕获/环境链。

## 对拍证据（2026-11-04，否决依据）

同一脚本（输出改用 `Debug.message`）在本机参考引擎
`TJSCompiler-release.exe` 上同样失败，且反汇编可见参考编译把自由变量编译为
`this` 成员访问后运行期找不到：

```text
gpd %1, %-2.*0   // *0 = (string)"a"
```

即 TJS2 编译器对嵌套函数体内的标识符只查当前 Namespace
（`tjsInterCodeGen.cpp` T_SYMBOL 分支：`Namespace.Find` 仅本上下文），
查不到按成员/全局回落——`inner` 内的 `a` 不指向 `outer` 的局部。
Ciallang 行为（找不到即报错）与参考一致；双方错误形态仅回落目标表述不同
（参考按 `this` 成员、Ciallang 按全局名），见下方观察。

## 遗留观察（不构成本次行动项）

标识符未命中局部时的回落语义双方不同：TJS2 编译为 `this` 成员访问
（有接收者时可命中 `this.a`），Ciallang 走全局名。仅错误信息与
"接收者恰有同名成员"的边角场景可观察；如需对齐另立评估项。

反编译侧（`ExprBuilder`）已能恢复该形态文本，执行语义随本引擎回落规则。

## 风险或备选方案

若未来需要真正的闭包捕获（Escaping 内层函数保持外层局部存活），
属语言扩展而非 TJS2 对齐，须走独立设计（环境对象、GC 根、调用约定）。
