# 预处理器（条件编译）

> 原文：TJS2 リファレンス「Preprocessor」章（[镜像 pp.html](https://a-rabin.github.io/krkr2doc-en/tjs2doc/contents/pp.html)）。

TJS2 支持条件编译表达式（类似 C 的 `#if`/`#endif`），在脚本被实际解释**之前**处理，可用于阻止或放行脚本的一部分。

与 C 预处理器指令不同，条件编译表达式是**自由格式**的，可以写在任何位置，不必每行只写一条指令。

---

## @set 指令

求值条件编译表达式并丢弃结果：

```text
@set(condition)
```

condition 中可用的运算符：

```text
,  =  !=  ==  ||  &&  |  ^  &  <  >  <=  >=
+  -  %  *  /  !  +  -  ( )
```

含义同普通表达式（见 [ExpressionAndOperator.md](ExpressionAndOperator.md)），但**只能处理整数**。

可使用任意变量与立即值，变量命名规则与 token 规则相同。预处理器变量与 TJS2 脚本变量**互不干扰**（命名空间独立）。

---

## @if / @endif 指令

```text
@if(condition)  ...  @endif
```

`@if` 求值条件编译表达式：结果为 0（假）则到对应 `@endif` 为止的部分**不被解释**；非 0 则正常解释。`@if`～`@endif` 可以**嵌套**。

```text
@if(flag==0)      // 未赋值的变量视为 0（假）
    @set(flag=1)
    ****
@endif
// ↑ **** 部分只编译一次

@if(version>=0x02040009)
    @if(flag)
        ****
    @endif
@endif
// ↑ 仅当 version ≥ 2.4.9 且 flag 非 0 时编译 ****
```

---

## 预定义变量

| 变量 | 含义 |
|---|---|
| `version` | TJS2 当前版本号，十六进制 `0x0XYYZZZZ`：X=主版本、YY=次版本、ZZZZ=发布号。例 `0x02040009` = 2.4.9 |

宿主程序可通过 `tTJS::SetPPValue` / `GetPPValue`（C++ 接口）注入/读取条件编译变量，未找到的变量取 0。
