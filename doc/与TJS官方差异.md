### function

* 闭包(closure):TJS不支持,Ciallang支持
```javascript
  function returnFunction() {
    var outside = "outside";
    function inner() {
      Debug.message(outside);
    }
    return inner;
  }

  var fn = returnFunction();
  fn()
```

上面这段代码在官方TJS解释器无法运行, 因为调用returnFunction()之后returnFunction作用域结束outside被删除,所以inner中的outside引用已经为非法内存地址