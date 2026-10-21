# 数据类型

> 原文：TJS2 リファレンス「Datatypes」章（[镜像 types.html](https://a-rabin.github.io/krkr2doc-en/tjs2doc/contents/types.html)）。

TJS2 的变量**没有静态类型**，但内部有类型——类型由引擎自动/动态处理。

内部类型共 6 种：`void`、整数、实数、字符串、对象、字节列（octet）。

---

## void

表示“什么都没有”（与 JavaScript 的含义不同）。**新声明未赋值的变量即为 void**。

void 与 void 的比较建议使用 `===` / `!==`（识别比较）运算符。
void 的 `typeof` 结果为 `"void"`。
void 被当作字符串时为 `""`，被当作整数时为 `0`（见 [Factor.md](Factor.md)）。

---

## 整数（Integer）

TJS2 以 **64 位精度**处理整数。通常按**有符号**整数对待，唯一的例外是 `>>>` 运算符，它把左侧按**无符号**整数处理。

`typeof` 结果为 `"Integer"`。

---

## 实数（Real）

以 64 位精度（double）处理。`typeof` 结果为 `"Real"`。

---

## 字符串（String）

处理**零终止**、无长度限制的字符串。字符编码为宽字符（通常 UNICODE），宽字符是 16bit 还是 32bit 取决于编译环境（Windows 为 16bit，Linux/Solaris 为 32bit）。

`typeof` 结果为 `"String"`。

---

## 对象（Object）

容纳对象的类型。除 `new` 分配的对象外，**函数、类、属性对象也都是对象类型**。

`typeof` 结果为 `"Object"`。

与 TJS(1) 和 JavaScript 不同，TJS2 的对象类型内部持有**两个指针**：一个指向对象本身，一个指向作为该对象执行**上下文（context）**的对象——以此实现所谓的**闭包**。改变这个上下文部分的运算符是 `incontextof`（见 [Class.md](Class.md) 闭包一节）。

---

## 字节列（Octet）

处理 8 位宽数据串，主要用于二进制数据。`typeof` 结果为 `"Octet"`。

---

## typeof 汇总

| 内部类型 | typeof 结果 |
|---|---|
| void | `"void"` |
| 整数 | `"Integer"` |
| 实数 | `"Real"` |
| 字符串 | `"String"` |
| 对象 | `"Object"` |
| 字节列 | `"Octet"` |
| （访问不存在的对象成员） | `"undefined"` |

---

## 关于原文的 C++ 侧章节（未收录）

原参考还有三章描述**宿主 C++ 侧**类型，不属于脚本语法，未在此总结：

* 「TJ2 Basic Datatypes」（basictypes）：`tjs_int` 等原始类型、`tTJSString`、异常基类 `eTJS`、`TJS_W` 宏。
* 「tTJSVariant 型」（variant）：C++ 中“变量”的载体 `tTJSVariant` 及 `tTJSVariantType` 枚举、对象/字符串/字节列的引用计数管理注意事项。
* 「iTJSDispatch2 インターフェース」（interface）：TJS2 “对象”的 C++ 抽象接口（FuncCall/PropGet/PropSet 等）与调用 flag、hint 机制。
