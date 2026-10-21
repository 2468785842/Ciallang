# 函数

> 原文：TJS2 リファレンス「関数」章（[镜像 function.html](https://a-rabin.github.io/krkr2doc-en/tjs2doc/contents/function.html)）。

TJS2 的函数与“子程序”几乎同义，**没有返回值的也叫函数**。类/对象成员的函数称为**方法**。

对函数使用 `instanceof "Function"` 结果为真（如 `func instanceof "Function"`）。

---

## 声明

```tjs
function 函数名(参数列表)
{
    函数内容
}
```

```tjs
function test(a) { System.inform(a); }

function func(a, b, c, d)
{
    test(a); test(b); test(c); test(d);
    return a+b-c*d;
}
```

不需要接收参数时，**参数列表整体可以省略**：

```tjs
function test
{
    // 不接收参数的函数
}
```

---

## 调用

```tjs
func();
func(1+2, 1-2);   // 参数为表达式，逗号分隔
func(func2());    // 嵌套调用
func(,1);         // 空位视为 void → 相当于 func(void, 1)
func(,);          // 相当于 func(void, void)
```

---

## 参数列表

参数按声明的顺序、从实参前面开始依次绑定到形参变量。这些参数是**局部变量**，函数退出后不可访问。

**实参个数与声明不一致不报错**（用 TJS2 编写的函数；C++ 等宿主原生函数可能报错）：

* 实参**多于**声明 → 多余的被忽略；
* 实参**少于**声明 → 不足的形参得到 `void`。

```tjs
function test(a, b)
{
    // test(1)      → a=1, b=void
    // test(1,2,3)  → a=1, b=2（3 被忽略）
}
```

### 默认参数

形参可用 `=` 指定默认值。**实参为 void 或实参不足**时自动使用默认值：

```tjs
function test(a = -1, b = 1)
{
    // test()           → a=-1, b=1
    // test(5)          → a=5,  b=1
    // test(void, void) → a=-1, b=1
    // test(,4)         → a=-1, b=4
}
```

---

## 式中函数（匿名函数）

`function` 用在表达式内可创建无名函数，此时函数名省略：

```tjs
var func  = function(i) { return i*5; };
var func2 = function { System.inform("hoge"); };

var v = func();
func2();
```

---

## 参数的省略传递：`...`

调用时把 `...` 作为**唯一的实参**，则把**当前函数收到的实参**按原样（原内容、原个数）转发给被调函数——即使参数变量已被修改、或收到的实参不足，也能传递正确的原始内容：

```tjs
function test()
{
    test2(...);  // test 收到的实参原样转发给 test2
}
```

---

## 参数的数组化接收：`形参名*`

形参名后加 `*`，该形参成为**收到的实参组成的数组**：

```tjs
function func(args*)
{
    // func(1,2,3,4) → args = [1,2,3,4]
}
```

也可**从中间某个形参开始**收为数组；实参不足时该数组为空数组：

```tjs
function func(x, args*)
{
    // func(1,2,3,4) → x=1, args=[2,3,4]
    // func(1)       → x=1, args=[]（空数组）
}
```

形参也可以**不写名字只写 `*`**（用于下述无名 `*` 传递）。

---

## 数组展开传参：`表达式*`

调用时在数组表达式后加 `*`，把数组**展开为逐个实参**传入：

```tjs
var args = [1, 2, 3, 4];
func(args*);        // 等价 func(1, 2, 3, 4);
func(0, args*, 5);  // 等价 func(0, 1, 2, 3, 4, 5);
```

实参位置只写 `*`（不写表达式），则把**本函数声明中无名 `*` 所代表的实参**原样转发：

```tjs
function func(func_array, *)
{
    for(var i = 0; i < func_array.count; i++)
        func_array[i](i, *);  // 无名 * 的实参跟在 i 后面转发
}
```

---

## `...` 与 `*` 的等价关系

无名 `*` 与“参数的省略 `...`”效果相同，且**比命名数组更高效**：

```tjs
function func(ar*) { func2(ar*); }   // 两行等价，
function func(*)   { func2(*);   }   // 下面的写法效率更高

function func()  { func2(...); }     // 这两个声明也等价
function func(*) { func2(*);    }
```
