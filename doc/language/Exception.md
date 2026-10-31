# 异常处理（try / catch / throw）

> 原文：TJS2 リファレンス「try」章（[镜像 try.html](https://a-rabin.github.io/krkr2doc-en/tjs2doc/contents/try.html)）。

异常（exception）是不属于正常处理的情况，多数场合可与“错误”互换。异常可能发生在程序的**任何位置**（如 `3%0` 这类显然的，或函数调用深处不显然的）。

---

## try / catch

```tjs
try
{
    func1();  // 可能发生异常
}
catch
{
    // try 块内发生异常时执行这里
    inform("The image cannot be loaded.");
}
```

* try 块内不发生异常时 catch 块不执行。
* 异常发生时，块内**剩余处理被中断**，跳到 catch 块。
* **catch 块内发生的异常不会被该 catch 捕获**；若外层没有别的 try，异常继续沿调用栈向上抛。
* 异常抛出到 catch 之间又发生新异常时，行为未定义。
* 异常在整个脚本中都没被捕获时，会被传给宿主应用程序处理（通常作为错误终止执行）。建议在易出错的位置写 try…catch。

---

## 异常对象

异常信息以 **Exception 类的对象**（异常对象）抛出，可在 catch 中接收：

```tjs
try
{
    loadImages("nothing.jpeg");
}
catch(e)  // 在变量 e 中接收异常对象
{
    // e.message 是随异常抛出的消息字符串
    inform("Unable to load image. \n"+e.message);
}
```

catch 后可写接收变量；该变量的作用域**从 catch 后的块开始、到该块结束**，无需事先声明。

---

## throw 语句

手动抛出异常：

```tjs
function pow2(n)
{
    // 计算 2 的 n 次方，n 必须是非负整数
    if(n<0) throw new Exception("Negative numbers cannot be specified.");
    return 1<<n;
}
```

* `throw` 可以抛出**任意值**——数字、字符串、对象或函数引用都可以。
* 但**建议抛 Exception 类（或其派生类）的对象**，这样 catch 一侧可以假定拿到的是 Exception 对象。

---

## 重抛异常

捕获后可以再次 `throw`，交给外层的 try 处理：

```tjs
function tryloadimage()
{
    try
    {
        primaryLayer.loadImages("test1.bmp");
    }
    catch(e)
    {
        inform("The image failed to load.");
        throw e;  // 显示消息后重抛
    }
}

function test()
{
    try
    {
        tryloadimage();
    }
    catch
    {
        return false;  // 在这里捕获重抛的异常
    }
    return true;
}
```
