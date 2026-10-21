# 项与关键字常量（Factor / Keywords）

> 原文：TJS2 リファレンス「項」章（[镜像 factor.html](https://a-rabin.github.io/krkr2doc-en/tjs2doc/contents/factor.html)，页面标题 Keywords）。

项（factor）是表达式求值中**优先级最高**的成分。除通常的数值、标识符等 token 外，还包括以下关键字。

---

## true / false

`true`、`false` 分别是值为 **1 和 0 的整数**。

TJS2 没有布尔类型，“0 为假、非 0 为真”是自动的数值↔逻辑转换。因此**不要**把 true/false 与其他表达式直接比较：不要写 `if(true != getBoolean())`，应写 `if(!getBoolean())`。

---

## void

字面含义“无”。用于表达“没有东西”的场合（如返回类型等），**不要与 null 混淆**。

新创建的值自动为 void。void 被当作字符串时是 `""`，被当作整数时是 `0`。
void 与 void 的比较建议用 `===` / `!==`。

---

## null

null 表示“**不表示任何东西的对象**”（空对象引用），不要与 void 混淆。

* 访问 null 对象的成员会**报错**。
* 用 `incontextof null` 把上下文改为 null 后，函数将在**调用点的上下文**（this）上执行：

```tjs
var objmethod = obj.method;
objmethod();  // 在 obj 的上下文上执行
objmethod = objmethod incontextof null;  // 上下文改为 null
objmethod();  // 在 this（即调用位置上下文）上执行
```

---

## Infinity / NaN

* `Infinity` 表示“无穷”；`-Infinity` 为负无穷。
* `NaN` 表示“非数”（Not a Number）；无效计算的结果会变成 NaN。

---

## super

在类的**方法/属性内**表示该类的超类。在类的方法/属性之外引用、或在**多重继承**的类中使用会报错。

---

## global

表示**全局对象**。块外声明的变量、非类方法的函数等都是全局对象的成员，可通过 `global` 访问：

```tjs
var v;
function test(v)
{
    global.v = v;  // 把参数 v 赋给全局对象中的 v
}
```

---

## this

通常表示方法/属性**所属类的对象本身**。准确地说，this 是调用时**显式或隐式指定的上下文（context）**。

---

## 式中数组

用 `[]` 可以在表达式中现场创建 **Array 类**的对象（如 `[1, 2, 3]`）。细节见原文 Array 类一章（未收录至本目录）。

## 式中字典

用 `%[]` 可以在表达式中现场创建 **Dictionary 类**的对象（如 `%["a" => 1, "b" => 2]`）。细节见原文 Dictionary 类一章（未收录至本目录）。
