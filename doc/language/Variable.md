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

声明时**不指定类型**：变量变为被赋入值的类型；运算过程和上下文中会自动进行类型转换。

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
