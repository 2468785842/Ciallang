# TJS2 语法参考（附 Ciallang 实现状态）

> 权威来源：**官方《TJS2 リファレンス》**
> <https://hydrozoa.felisworks.com/doc/tjs2doc/contents/index.html>（语言规格/内建类/预处理器）
> 与原版 bison 语法 `doc/grammar/tjs.y`（产生式逐字权威，本文行号即指此文件）。
> 本文整理完整语法并标注 **Ciallang 当前实现状态**：✅ 已实现 / ◐ 部分 / ❌ 未实现（与 README/TODO 对齐）。

---

## 1. 词法

| 元素 | TJS2 规则 | 状态 |
|---|---|---|
| 标识符 | 以字母/`_`/全角文字开头，后续同集合+数字；大小写敏感 | ✅ |
| 行注释 `// ...` | | ✅ |
| 块注释 `/* ... */` | **可嵌套**（kirikiri 特性） | ✅（嵌套行为以 Parser 为准，测试覆盖） |
| 整数字面量 | 十进制、`0x` 十六进制 | ✅ |
| 实数字面量 | `1.5`、`1e3`、`1.5e-3` | ✅ |
| 字符串 `"..."` / `'...'` | 单双等价；`\` 转义（`\n \t \\ \"` 等） | ✅ |
| 模板/逐字字符串 `@"..."` | 逐字转义；**内嵌表达式** `&expr;`（求值嵌入）与 `$expr;`（表达式结果再作为内嵌表达式求值） | ◐（模板串已实现；内嵌表达式语义对齐中） |
| 正则字面量 `/re/flags` | 与除法 `/` 的歧义由语法状态消解（`SetStartOfRegExp`，tjs.y/词法配合） | ❌（依赖 RegExp） |
| 保留字 | `var const if else while do for continue break switch case default function return class extends property setter getter try catch throw debugger new invalidate isvalid delete typeof void null true false global this super int real string incontextof instanceof in` 等 | 词法层 ✅ |

## 2. 预处理（条件编译）

原版指令（`tjsLex.cpp:1023-1070`，`tjsCompileControl.h` + `tjspp.y` 表达式求值）：

```
@set(NAME = 表达式)      设置预处理变量
@if(表达式)              条件开始
@endif                   条件结束
```

- 表达式可用 `== != < > <= >= && || ! + - * / \ % ( )` 与已定义变量；
- 主脚本运行前由宿主注入变量（如 kirikiriz）。
- **Ciallang 状态**：✅ `@set` / `@if` / `@endif`（实现于 `src/parser/PreProcessor.cpp`；
  表达式能力对齐程度见测试 `tests/preprocess_test.cpp`）。

## 3. 类型与值

| 类型 | 字面量/构造 | 状态 |
|---|---|---|
| Void | `void` | ✅ |
| Integer | 8 字节整数 | ✅ |
| Real | double | ✅ |
| String | `"..."`（16 位字符语义） | ✅（内部 UTF-8） |
| Object | `null`（空闭包）、对象/函数/类/数组/字典 | ✅（Array/Dictionary 类缺失，见标准库） |
| Octet | `<% 00 01 ff %>` 十六进制字节流 | ❌ 字面量（类型已实现） |
| `true` / `false` | 布尔（编译为 Integer 1/0 语义） | ✅ |
| `Infinity` / `-Infinity` | | ✅ |

## 4. 表达式与优先级（从低到高）

依据 tjs.y:527-775 产生式阶梯（左结合除非注明）：

| 级 | 运算符 | 状态 |
|---|---|---|
| 1 | 顺序表达式 `A, B`（取 B 值） | ✅ |
| 2 | **赋值（右结合）** `= += -= *= /= %= \= >>= <<= >>>= &= \|= ^= \|\|= &&=` | ✅ |
| 2′ | **交换** `A <-> B`（两值互换，返回 void） | ✅ |
| 3 | 条件 `A ? B : C`（右结合） | ✅ |
| 4 | 逻辑或 `\|\|` | ✅ |
| 5 | 逻辑与 `&&` | ✅ |
| 6 | 按位或 `\|` | ✅ |
| 7 | 按位异或 `^` | ✅ |
| 8 | 按位与 `&` | ✅ |
| 9 | 相等 `== !=`；**严格相等** `=== !==`（类型与值都相同，如 `"1" === 1` 为假） | ✅ |
| 10 | 关系 `< > <= >=` | ✅ |
| 11 | 移位 `<< >> >>>`（`>>>` 无符号右移） | ✅ |
| 12 | 加减 `+ -`（`+` 对字符串为连接） | ✅ |
| 13 | 乘除类 `% / \ *`（`\` 整除；`*` 与前缀属性访问歧义经专门产生式 `mul_div_expr_and_asterisk` 消解） | ✅ |
| 14 | **一元前缀** `! ~ ++ -- new invalidate isvalid delete typeof # $ + - & *` | ✅（`&` 取属性本体不触发 getter、`*` 解引用调 getter/setter，引用须存局部变量） |
| 14′ | **一元中缀式** `A instanceof B`（B 为类名）、`A in B`（成员存在判断） | ◐：instanceof ✅（基础类型按类型名匹配，Number 覆盖 Integer/Real）；`in` ❌ |
| 15 | `A isvalid`（**后缀**形态，与前缀并存）、`A incontextof B`（**右结合**，替换函数闭包的 this） | ◐：incontextof ✅；后缀 isvalid ✅ |
| 16 | **类型转换** `(int)A (real)A (string)A` 或裸 `int A real A string A` | ✅ |
| 17 | **后缀**：调用 `f(args)`、成员 `.sym`（含 `.` 起始的裸词链）、下标 `a[i]`（通用路径：数组/字典/字符串；可写，越界扩展）、后缀 `++ --`（表达式值为旧值；成员形态走 getter/setter 链）、**即时求值 `A !`**（把 A 的字符串值作为 TJS 表达式编译执行） | ◐：`() . -- ++ []` ✅；`!` ❌ |
| 18 | 初等项：字面量 / `(expr)` / `this` `super` `global` / 函数表达式 / 内联 Array `[...]` / 内联 Dictionary `%[...]`（键形式 `sym :` 与 `"k" >= v`，=> 词形即 >=）/ 正则 `/pattern/` / 常量式 `(const)[...]` `(const)%[...]` | ✅（`(const)` 按运行时新建，只读共享语义未实现） |

前缀一元语义速查（原版）：

- `#s`：字符串取首字符的字符码（ASC，非字符串先字符串化；octet 报错）；`$n`：字符码 → 单字符字符串（CHR，对应新增字节码 Asc/Chr）
- `&expr`：忽略属性访问（属性当普通值取）；`*expr`：强制属性访问（当 property 取）
- `+expr`：数值规范化（字符串转数值、octet 报错，对应 NUM；降糖为 ToReal）
- `delete expr`：删除成员，返回 bool（仅删实例自有成员）
- `typeof expr`：类型名字符串

后缀条件 `expr if cond`（tjs.y `expr → comma_expr "if" expr`）：绑定比逗号更松，
假条件时**整个赋值不发生**（`tmp = a if c` 的 lhs 是整个赋值表达式）——已实现 ✅。
成员只读属性：`str.length`（字节长度）✅。

## 5. 语句（tjs.y:222-243）

| 语句 | 形式 | 状态 |
|---|---|---|
| 空语句 `;` | | ✅ |
| 表达式语句 | | ✅ |
| `var` 声明 | `var a, b = expr;`；可带类型注解 `var a:int`（注解仅文档性） | ◐：核心与 `const` 声明 ✅（const 与 var 等价，只读性不强制）；注解 ❌ |
| `if (e) s [else s]` | | ✅ |
| `while (e) s` | | ✅ |
| `do s while (e);` | | ✅ |
| `for (init; cond; step) s` | 三段均可空；首子句可为 `var`/`const` 声明或表达式（`for_first_clause`） | ✅ |
| `break` / `continue` | | ✅ |
| `switch (e) { case 常量: … default: … }` | case 标签为**编译期常量**（整数/字符串） | ✅ |
| `try { } catch (v) { }` | 捕获值为异常对象（Exception 类实例） | ✅ |
| `throw e;` | 任意值可抛 | ✅ |
| `return [e];` | | ✅ |
| `with (e) s` | 把 e 的成员注入作用域查找链（裸词链 `.member` 基座取栈顶；无 with 时为 global） | ✅ |
| `debugger;` | 断点 | ✅（TAC Debugger） |
| 函数定义 | 见 §6 | ✅ |
| `property` | 见 §7 | ✅ |
| `class` | 见 §7 | ✅ |

## 6. 函数

```
function 名(参数表) { … }          // 声明（顶层/成员）
function (参数表) { … }            // 函数表达式（值）
参数:  名字 [= 默认值表达式]        // 默认值：实参为 void 时求值
       *                        // 匿名收拢：多余实参打包 Array
       名字*                     // 命名收拢
调用:  f(a, b, ...)               // ... 省略：透传当前函数全部实参
       f(a, *arr)                 // *arr 展开：数组元素作为实参
```

- 状态：**声明/表达式/调用/默认值** ✅（15 个 TJS2 语义用例固化于 `tests/class_extends_test.cpp` 等处）；
  **收拢参数 `*` / 省略调用 `...` / 展开调用 `*expr`** ❌（与字节码 argcount=-1/-2 对应，见
  [bytecode/mapping.md](bytecode/mapping.md) §4.5）。
- 函数是"闭包值"：捕获定义处作用域；`incontextof` 只改 this 不改闭包链。

## 7. 类与属性

```
class 类名 extends 父类1, 父类2 … {   // 多继承，后面的覆盖前面
    var 成员变量;                      // 实例字段
    static var 静态字段;               // 静态成员（挂在类对象上）
    function 方法(…) { … }
    static function 静态方法(…) { … }
    function 类名(…) { … }            // 与类同名的构造函数
    property 名 { getter() { … } setter(v) { … } }   // getter/setter 顺序任意，可只有其一
    function finalize() { … }         // 析构（invalidate 后触发）
}
```

- 语义要点（TJS2 特有，Ciallang 以 29 个类测试固化）：
  - 成员解析走 **slot 链**（实例字段 → 本类 → 父类链），`super` 显式跨层；
  - 方法访问按对象绑定 this（字节码侧经闭包 objthis，本项目经 `DataObject::getProp` 克隆绑定）；
  - `invalidate obj` 置失效 + 引用计数归零时触发 `finalize`。
- 状态：class/多继承/property/构造函数/super ✅；`static`/`finalize` 触发时机 ◐（以测试为准）。

## 8. 内联字面量（Array / Dictionary / 常量折叠形）

```
[ e1, e2, e3 ]                // Array
%[ key: v, "key2": v2, … ]    // Dictionary（key 可为裸符号或字符串；尾逗号合法）
(const)[ … ] / (const)%[ … ]  // 常量形：元素须为编译期常量，编译时构建真实 Array/Dictionary 常量
```

状态：全部 ❌（依赖 Array/Dictionary 类型，见 [compatibility.md](compatibility.md)）。

## 9. 标准库概览（详细对照见 compatibility.md）

原版全局注册（`tjs.cpp:100-190`）：`Array` `Dictionary` `Exception` `Math`（含 `Math.random`
Xorshift 与 `Math.RandomGenerator` MT19937）`Date`（bison 日期文法 `tjsdate.y`）`RegExp`
（oniguruma）`String 内建成员`（length/charAt/indexOf/sprintf/replace/split/escape…）`Octet pack 系`。

Ciallang 现状：`print/println` ✅、String 部分 ✅（replace/split 待 RegExp/Array），其余 ❌。
