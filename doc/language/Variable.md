# 变量（var）

> 原文：TJS2 リファレンス「変数」章（[镜像 variable.html](https://a-rabin.github.io/krkr2doc-en/tjs2doc/contents/variable.html)）。

TJS2 的变量用 `var` 关键字声明，**不声明不能使用**。

---

## 声明与初始化

```tjs
var 变量名;
var 变量名 = 初值;         // 声明并初始化
var a, b, c;               // 逗号分隔一次声明多个
var a=0, b=1, c=3;         // 多个 + 各自初始化
```

声明时**无需指定类型**：变量变为被赋入值的类型；运算过程和上下文中会自动进行类型转换。参考文法也接受 `var a:int` 等类型注解，但会忽略它们，不形成静态类型约束。

---

## 变量名

所有**标识符**都可作为变量名（规则见 [Token.md](Token.md)）：

```tjs
var ampan;       // OK
var 123a;        // NG：数字开头
var function;    // NG：保留字
var function1;   // OK：包含保留字但不是保留字
var 変数その一;  // OK：全角字符可用
```

---

## 重复声明

同名变量声明两次以上**不报错**，但：

* 第二次以后的声明**带初值** → 该初值被赋给变量；
* **不带初值** → 变量变为 `void`。

```tjs
var a=0;
var a;    // 第二次：不报错，a 变为 void
var a=-1; // 第三次：不报错，a 被赋 -1
```

---

## 作用域

在 `{ }` 块内声明的变量是**块内局部变量**，离开块即销毁。

* 与 C 不同，**声明位置没有限制**（可在块中任意位置声明）。
* 块内声明与块外同名变量时，块内**只能访问新声明的变量**（遮蔽），离开块后块外变量不受影响。

初值表达式在建立新绑定前求值，因此 `var x=3; { var x=x+4; }` 中内层初值为 7，
退出块后外层仍为 3。同一层重复声明则复用该层绑定；无初值时重置为 `void`。
函数参数与函数体属于相邻两层，函数体或更深的块可以遮蔽参数；`for` 的初始化子句
也有独立作用域，循环结束后不会覆盖外层同名变量。

上述行为已经与本机适配版 TJSCompiler（基础提交 `e369611`）执行对照，并核对
`../TJSCompiler/src/syntax/tjsNamespace.cpp` 的 `Add`/`Find`（仅当前层声明、逆序查找）及
`../TJSCompiler/src/syntax/lexer/tjsInterCodeGen.cpp` 的
`InitLocalVariable`/`EnterBlock`/`ExitBlock`/`EnterForCode`。
Ciallang 通过活跃绑定栈记录每层边界，避免用指令位置猜测空块、参数或 catch 的作用域；
实现见 `src/gen/IRGenerator.hpp/.cpp`，回归见 `tests/scope_test.cpp`。

```tjs
while(t--)
{
    var ratio=t/all;  // 声明 ratio
    setRatio(ratio);
}
// 控制流到这里时 ratio 已被销毁
var test;             // 这里也可以声明

var ratio=0;
if(flag)
{
    var ratio;                       // 遮蔽块外的 ratio
    ratio=buf.position/buf.totalTime; // 只访问块内的 ratio
    setRatio(ratio);
}
// 块内 ratio 销毁，块外 ratio 保持原样
ratio=-1;
```

---

## 全局作用域

在所有作用域之外（**全局作用域**）用 `var` 声明的变量，会成为**全局对象的成员**（见 [StatementAndBlock.md](StatementAndBlock.md)）。
