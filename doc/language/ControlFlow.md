# 控制流：if / while / for / switch / with

> 原文：TJS2 リファレンス各章（[if](https://a-rabin.github.io/krkr2doc-en/tjs2doc/contents/if.html) / [while](https://a-rabin.github.io/krkr2doc-en/tjs2doc/contents/while.html) / [for](https://a-rabin.github.io/krkr2doc-en/tjs2doc/contents/for.html) / [switch](https://a-rabin.github.io/krkr2doc-en/tjs2doc/contents/switch.html) / [with](https://a-rabin.github.io/krkr2doc-en/tjs2doc/contents/with.html)）。

---

# if 语句

```tjs
if(expression)
    语句/块
else
    语句/块
```

expression 求值为真执行第一段，假执行第二段；`else` 不需要可省略。

## if 与 else 的配对

`else` 的规则是：**与前面尚未配对 else 的最近的 if 配对**。

TJS2 是自由格式语言，**缩进不影响配对**——即使缩进误导，配对仍按上述规则：

```tjs
if(expr)      // ★
    if(expr)  // ●
        statement;
else          // 实际与 ● 配对（尽管缩进看起来像 ★）
    statement;
```

要明确区分语句块，必须用 `{ }` 括住。

---

# while 语句

```tjs
while(expression)
    语句/块
```

执行流程：

1. 先求值 expression，为假则语句/块**一次都不执行**；
2. 执行语句/块；
3. 再求值 expression，为真回到 (2)，为假结束 while。

即：条件检查在**每执行一遍语句/块后**进行一次。注意 (3) 的时点上，块的情况会**先退出块**（块内局部变量被清除）再判定。

## break / continue

* `break`：中途终止 while。
* `continue`：跳回条件判定 (3)。

```tjs
var i = 0;
while(i < 5)
{
    if(func1()) continue;  // 回到 i<5 判定，真则从块首重新执行
    func2();
    i++;
}
```

## do～while

与 while 相同，但**先执行一次语句/块、不先检查条件**：

```tjs
do
    语句/块
while(expression);
```

即使初始时条件为假，语句/块也至少执行一次；之后每次执行完毕检查条件。

---

# for 语句

```tjs
for(第1段; 第2段; 第3段)
    语句/块
```

* **第 1 段**：进入循环前执行的表达式；可在此**声明变量**，该变量在整个循环期间有效（循环结束即离开作用域）。可省略。
* **第 2 段**：条件表达式，为假则结束循环。省略则视为恒真。
* **第 3 段**：语句/块执行完后执行的表达式。可省略。

执行流程：

1. 执行第 1 段；
2. 求值第 2 段，假则结束（语句/块一次不执行），真则到 (3)；
3. 执行语句/块；
4. 执行第 3 段；
5. 求值第 2 段，假则退出循环；
6. 回到 (3)。

* `break`：当场跳出循环。
* `continue`：**跳到 (4)**——即 continue 也会执行第 3 段（如 `i++`）。

```tjs
var i;
for(i=0; i<10; i++)
{
    if(obj[i]==0) continue;  // continue 后 i++ 仍执行
    if(obj[i]==1) break;
    func(obj[i]);
}
```

---

# switch 语句

```tjs
switch(base_expression)
{
case condition_expression1:
case condition_expression2:
    ...
default:
    ...
}
```

* 先求值 `base_expression`；随后**从上到下逐个求值**各 `case` 的条件表达式，与 base_expression 的结果**相同**处开始执行。
* **case 后可以写任意表达式**（不限于常量），字符串也可以。
* 匹配点之前的语句被跳过；一旦匹配，**其后的 case / default 全部被忽略**，顺序执行直到 `break;` 或块尾——忘记写 break 会穿透到下一个 case（常用技巧，也易错）。
* 都不匹配时：有 `default:` 则从 default 开始执行；无 default 则整个块不执行。

```tjs
switch(a)
{
case 0:
case 1:               // 无 break，case 0 穿透到这里
    inform("a は 0 か 1 です");
    break;
case b+1:             // 表达式作 case
    inform("a==b+1");
    break;
case "あいうえお":     // 字符串作 case
    type=1;
    break;
}
```

---

# with 语句

指定 `.` 运算符**左侧被省略**时引用的对象：

```tjs
with(expression)
    语句/块
```

```tjs
with(obj) .member = 1;   // 等价 obj.member = 1;

var object = new Foo();
with(object)
{
    .setPos(0, 0);
    .setSize(100, 100);
    .name = "名前無し";
}
```

## 求值时机：只求值一次

`expression` 在 with 开始时**只求值一次**，之后一直引用该结果（不再重复求值）：

```tjs
this.dic = %[];
with(this.dic)
{
    .member = 1;
    this.dic = 0;   // dic 被换成别的东西
    .member = 2;    // 仍然成功——用的是最初求值的结果
}
```

可视为编译成如下等价代码：

```tjs
this.dic = %[];
{
    var 匿名特殊局部变量 = this.dic;
    匿名特殊局部变量.member = 1;
    this.dic = 0;
    匿名特殊局部变量.member = 2;
}
```

即：with 把表达式结果存入一个**只能通过省略左侧的 `.` 访问的匿名局部变量**，其作用域就是 with 影响的范围。由于只求值一次，反复引用对象的场景可能更快。

## with 之外的省略 `.`

with 语句之外、`.` 左侧被省略时，引用的是**全局对象**：

```tjs
.foo = 1;      // 等价 global.foo = 1;
```
