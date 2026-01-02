# 类（Class）

“类”是创建“对象”时使用的**模型**。

* 类中的变量称为 **成员变量**
* 类中的函数称为 **成员函数 / 方法**
* 类中的属性称为 **成员属性**

---

## 类的声明

类的声明形式如下：

```tjs
class classname
{
    // 定义成员变量，成员函数和成员属性

    function classname() // 构造函数
    {
    }

    function finalize() // 相当于析构函数
    {
    }
}
```

说明：

* 使用一个标识符作为 **类名**
* 在类内部定义变量、函数和属性，作为类的成员
* **必须**定义一个与类名同名的成员函数，称为 **构造函数**
* 使用 `new` 运算符创建对象时，构造函数会被自动调用
* 可以定义 `finalize` 方法，在对象销毁时调用（可省略）

---

## 类定义示例

```tjs
class test
{
    function test()
    {
        // 构造函数
        // 在这里填写对象创建时需要处理的内容
        variable = 0;
    }

    function finalize()
    {
        // finalize 方法
        // 在这里填写对象销毁时需要处理的内容
    }

    function method1()
    {
        // 方法
        System.inform(variable);
    }

    var variable; // 成员变量

    property prop // 成员属性
    {
        getter() { return variable; }
    }
}
```

---

## instanceof 与 Class 判断

对类使用 `instanceof` 运算符和 `"Class"` 操作数时，会得到 `true`。

例如：

```tjs
test instanceof "Class" // true
```

---

# 对象的创建

已定义类的对象使用 `new` 运算符创建。

```tjs
class Test
{
    var variable1 = getValue(); // 成员变量初始化

    function Test(arg1, arg2)
    {
        // arg1 和 arg2 接收 new 运算符传入的参数
    }
}

var newobject = new Test(1, 2);
```

---

## 对象创建时的处理顺序

1. 创建一个空对象
2. 注册方法和属性
3. 创建成员变量（需要初始化的变量在此阶段初始化）
4. 执行构造函数

---

### ⚠ 注意

即使构造函数不需要参数，也 **不能省略括号**：

```tjs
new Test(); // 正确
new Test;   // 错误（不像 JavaScript）
```

---

## 在类方法中创建自身对象的注意事项

以下写法是 **错误的**：

```tjs
class Test
{
    function Test()
    {
    }

    function func()
    {
        return new Test(); // 错误
    }
}
```

原因：

* 在类内部，`Test` 会被解析为构造函数本身
* 作用域优先级导致歧义

### ✅ 正确写法

类已注册到 `global`，应显式使用：

```tjs
class Test
{
    function Test()
    {
    }

    function func()
    {
        return new global.Test(); // 正确
    }
}
```

---

# 对象的销毁

TJS2 中，对象销毁分为两个阶段：

1. **无效化（invalidate）**
2. **销毁**

---

## finalize 与 invalidate

```tjs
class Test
{
    var variable;

    function Test()
    {
        variable = new AnotherClass();
    }

    function finalize()
    {
        // 对象无效化时调用
        invalidate variable;
    }
}

var object = new Test();
invalidate object;
```

说明：

* 对象无效化时会调用 `finalize`
* 被无效化的对象：

    * 标记为无效
    * 后续访问会抛出异常
* 可用 `isvalid` 运算符判断对象是否有效

---

### ⚠ 注意

* 即使不显式调用 `invalidate`，对象在“没用了”时也可能被自动无效化
* **无效化和销毁发生的时机不确定**
* 强烈建议对象用完后手动 `invalidate`

---

### 补充说明

* `invalidate` 类似于 C++ 的 `delete`
* TJS2 的 `delete` **不是**对象删除运算符
  它用于删除成员或全局变量

---

# 对对象的操作

通过 `.` 或 `[]` 访问成员：

```tjs
var obj = new MyLayer(window, window.primaryLayer);

obj.method1();
obj['method1']();

obj.num = 3;
obj['num'] = 3;

obj.prop1++;
obj['prop1']++;
```

---

# 闭包（Closure）

对象的方法和属性在注册时会绑定 **所属对象（context）**。

即使方法被取出单独使用，也仍然作用于原对象。

```tjs
var obj = new FooBarClass();
obj.method();

var objmethod = obj.method;
objmethod(); // 等同于 obj.method()
```

---

## incontextof 运算符

可强制改变方法的上下文对象：

```tjs
(objmethod incontextof obj2)();
(objmethod incontextof this)();
```

---

# 继承（extends）

使用 `extends` 关键字实现继承。

```tjs
class Class1
{
    function Class1() {}
    function finalize() {}
    function method1() {}
}

class Class2 extends Class1
{
    function Class2()
    {
        super.Class1();
    }

    function finalize()
    {
        super.finalize();
    }
}
```

```tjs
var obj = new Class2();
obj.method1();
```

---

## 继承时的初始化顺序

1. 创建空对象
2. 注册方法和属性（先超类，后子类）
3. 创建成员变量（先超类，后子类）
4. 调用子类构造函数
5. 从子类构造函数中调用超类构造函数

---

# 多重继承

```tjs
class SubClass extends ClassA, ClassB
{
    function SubClass()
    {
        ClassA();
        ClassB();
    }

    function finalize()
    {
        global.ClassA.finalize();
        global.ClassB.finalize();
    }
}
```

说明：

* 多重继承中 **不能使用 `super`**
* 必须显式使用 `global.ClassName`
* 若存在同名成员，**后写的类优先**

---

# override（隐藏）

子类定义了与超类同名的方法或属性，会 **隐藏** 超类成员。

```tjs
class Class2 extends Class1
{
    function method1()
    {
        if (cond)
            return super.method1();
    }
}
```

说明：

* 方法和属性可被隐藏
* **成员变量无法隐藏**
* 子类成员变量会覆盖超类同名成员变量

---