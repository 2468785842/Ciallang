# 属性（property）

> 原文：TJS2 リファレンス「プロパティ」章（[镜像 property.html](https://a-rabin.github.io/krkr2doc-en/tjs2doc/contents/property.html)）。

属性可以像变量一样读写，但实际会引发 **setter / getter**（合称属性处理器）的方法调用。

---

## 声明

```tjs
property 标识符
{
    setter(v)
    {
        // setter 内容（接受唯一一个参数）
    }
    getter()
    {
        // getter 内容
        return 表达式;  // getter 的返回值
    }
}
```

* setter 与 getter **顺序可换**。
* `getter` 后的 `()` **可省略**。
* 该属性注册为所在类或 global 的成员；读该标识符走 getter、写走 setter。
* **只写其一即为只读或只写属性**：例如只写 getter 则为只读，写入报错。

```tjs
var value;

property property1
{
    setter(v)
    {
        value = v;
        inform("value set.");
    }
    getter
    {
        inform("value get.");
        return value;
    }
}

property1 = 5;  // 调用 setter
property1++;    // 先调 getter 取值、再调 setter 写回
```

属性与变量、函数、类一样**可以被子类 override**（见 [Class.md](Class.md)）。

---

## 属性对象与 `&` 运算符

属性本身也是一个**对象**（属性对象）。但它一旦注册到某个对象上，普通访问只会触发属性处理器，**取不到属性对象本身**。

用单目 `&` 运算符可以**不经过属性处理器**、直接访问属性对象：

```tjs
property prop { /* 略 */ }

{
    var p = &prop;          // 取出属性对象存入局部变量
    &object.property1 = p;  // 把 p 注册为 object 的 property1
}
```

---

## 用 `*` 调用属性处理器

取出到局部变量的属性对象，即使不注册到对象上，也可用单目 `*` 运算器直接驱动属性处理器：

```tjs
{
    var p = &prop; // 块内局部变量，普通读取保留属性引用
    *p = 30;      // 调用 setter，写入 30
    func(*p);     // 调用 getter 取值，传给 func
}
```

---

## 存放位置与自动属性调用

属性对象可以存入局部变量、global 或对象成员。局部变量普通读取复制引用；global 或成员槽的普通访问则会调用 getter/setter。需要取引用本体时使用 `&name` / `&obj.member`，需要显式调用处理器时使用 `*`。

当然，对这样注册的属性对象再用 `&` 取出来是可以的。

对属性对象使用 `instanceof "Property"` 结果为真（如 `&prop instanceof "Property"`）。
