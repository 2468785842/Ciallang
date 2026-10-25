# 兼容性矩阵与路线图

> Ciallang 对原版 TJS2（TJSCompiler）的三维兼容状态总览。
> 语法细节见 [syntax.md](syntax.md)；字节码规范与转译设计见 [bytecode/](bytecode/) 目录。
> 状态：✅ 可用 / ◐ 部分 / ❌ 未实现 / 📐 设计定稿未实现。

---

## 1. 源码兼容（.tjs 脚本直接运行）

| 领域 | 项 | 状态 |
|---|---|---|
| 预处理 | @set / @if / @endif | ✅ |
| 语句 | var/if/while/do-while/for/switch/try-catch/return/break/continue/function/property/class/debugger | ✅ |
| 语句 | with | ✅（withStack 上下文栈，异常随帧截断）|
| 语句 | for(const …)、var 类型注解 | ◐ for(const) ✅；const 声明 ✅（只读性不强制）；类型注解 ❌ |
| 表达式 | 赋值全家（含 `<->`、`\|\|=`、`&&=`、`>>>=`）、?:、逻辑/位/比较/严格相等/移位/乘除整除 | ✅ |
| 一元前缀 | `!` `new` `invalidate` `-` `~` `++` `--` `delete` `typeof` `#` `$` `+` `&` `*`（及前后缀 `isvalid`） | ✅ |
| 一元后缀 | `()` `.` `++` `--` | ✅ |
| 一元后缀 | `[]`（下标读写：数组/字典/字符串） | ✅ |
| 一元后缀 | `!`（eval 即时求值） | ❌ |
| 容器成员 | Array：push/pop/unshift/shift/insert/remove/erase/reverse/join + count/length（可写）；Dictionary：assign/clear/erase + count | ✅（sort/split/save/load/pack 系 ❌） |
| 中缀 | instanceof / incontextof | ✅（instanceof 基础类型按类型名匹配）/ ✅；`in` ❌ |
| 字面量 | 全部标量（含 i64 大整数字面量精确解析）+ 函数表达式 + @"…" 模板串 | ✅ |
| 字面量 | `[...]` Array、`%[...]` Dictionary、`<%%>` Octet、`(const)` 常量形 | ◐ Array/Dictionary/Octet ✅；`(const)` 按运行时新建（只读共享语义 ❌） |
| 后缀条件 | `expr if cond`（假条件整体不赋值，tjs.y expr 层级绑定） | ✅ |
| 函数 | 默认参数（TestVoid 守卫，实参优先） | ✅ |
| 函数 | 收拢 `*args`、省略调用 `...`、展开调用 `*expr` | ❌ |
| 类 | 多继承/slot 链/super/构造函数/property（29 个语义测试） | ✅ |
| 类 | static / finalize 触发语义 | ◐ |
| 成员 | `delete obj.member`（实例自有成员） | ✅ |
| 成员 | `str.length` 只读属性 | ✅ |
| 异常 | Exception 类（message 成员）+ 原生错误转脚本可捕获异常 | ✅ |

## 2. 字节码兼容（TJSCompiler 编译产物 `"TJS2" "100\0"` 文件）

**路线已定**：加载 → **转译为 Ciallang TAC** → 现有 VM 执行（📐 全套设计见
[bytecode/mapping.md](bytecode/mapping.md)，2026-10-20 决策记录：否决"独立 TJS 解释器"方案）。

| 项 | 状态 |
|---|---|
| 容器/常量池/context 解析（Reader） | 📐 M1 |
| 顶层/函数 context 转译（算术/比较/跳转/调用/flag 物化） | 📐 M1 |
| 属性访问全族（GPD/GPI/SPD/…/S 变体） | 📐 M2 |
| 类（ctClass 加载期解释）/ NEW / try-catch | 📐 M2 |
| 新增 TAC（Typeof/Asc/Chr/BNot/DelProp/GetP/SetP/比较写 zf…）+ run0 编码补全 | 📐 M3 |
| octet 常量、collapse/展开/省略调用、EVAL/EEXP、superClassGetter | 📐 M4 |
| 对拍工具链：TJSCompiler `--dump` 导出选项 + `cial --tjsbc` + 语料入库 | 📐（已批准修改 TJSCompiler，仅 main.cpp） |

## 3. 标准库兼容

| 库 | 原版能力（概要） | 状态 |
|---|---|---|
| （宿主）print | 控制台输出 | ✅ print/println |
| String 内建成员 | length/charAt/indexOf/toUpperCase/…/sprintf/replace/split/escape/trim | ◐ replace（g 全局）/split ✅；length 为码点计数（其余方法字节语义） |
| Array | 数字下标数组；load/save(BinarySerializer)/sort/join/split/pack… | ◐ 字面量/下标/push/pop/unshift/shift/insert/remove/erase/reverse/join/split/sort(字母+比较器回调)/count ✅；save/load/pack ❌ |
| Dictionary | 哈希字典；load/save/assign/clear | ◐ 字面量/下标/assign/clear/erase/count ✅；save/load ❌ |
| Exception | 异常类（message 成员），脚本异常载体 | ✅（TJS 源自举，main.cpp 注入；原生错误自动包装为 Exception 实例） |
| Math / Math.RandomGenerator | abs/floor/…；Xorshift / MT19937 | ❌ |
| Date | get/set 系列 + 字符串 parse（tjsdate.y 文法） | ❌ |
| RegExp | compile/test/match/exec/replace/split（oniguruma）；属性 matches/start/index/lastIndex/input/lastMatch/lastParen/leftContext/rightContext | ◐ /pattern/flags 字面量（g/i）+ 全部方法 + 全部实例属性 ✅（std::regex ECMAScript）；缺：`new RegExp(...)` 构造器、静态属性 `RegExp.last`；replace 回调 this 绑定为函数自身（TJS2 以 regexp 作 objthis）；匹配位置为字节偏移（TJS2 为 UTF-16 码元） |
| Octet | 字节流 + pack 系 | ❌（类型有，库无） |

## 4. 运行时行为差异（已知，须保持记录）

| 项 | TJSCompiler | Ciallang | 处理 |
|---|---|---|---|
| 内存管理 | 纯引用计数（tTJSVariant CopyRef） | 标记清除 GC + 引用计数混合 | 行为等价；GC 句柄坑位见 AGENTS.md |
| 性能 | fib(25) 基准 | 约慢 2.3~2.5×（优化前 4~5×） | TODO.md 一节任务 |
| 字符串内部编码 | UTF-16（tjs_char） | UTF-8 | 字节码转译处做转换（代理对）；str.length 为字节长度 |
| 调用栈深度 | 无同值上限 | 1024 | 记录差异 |
| 局部变量名（字节码态） | 序列化不含 Namespace | 同样无 | 一致 |
| 随机序列 | Math.random Xorshift；RandomGenerator MT19937 | 无 | 实现时注意原版注释"序列不保证一致" |
| 原生错误报告 | 未捕获异常打印 VM 转储（含 ip） | 顶层打印 Exception.message 文本 | 语料对拍报告的 DIFF 部分由此产生，语义等价 |
| `#`/`$` 操作数 | 严格类型（字符串/整数） | `#` 对非字符串先字符串化（`#65 == 54`，对齐 kirikiri 语料期望） | 记录差异 |
| 后缀 `i++` 表达式值 | 返回自增前旧值 | 同（旧值，2026-10-24 修正） | 一致 |
| `&` 引用作用域 | 全局/局部变量均可持引用 | 仅局部变量可持引用（全局槽位读取会触发 getter，TODO 一节已知限制） | 记录差异 |

## 5. 路线图（建议顺序）

```
P0  性能与正确性（TODO.md 一节）：run0 扁平字节码补全 → eval 切 run0；
    GC 偶发 SIGSEGV 修复；fib(25) 追平旧版
P1  字节码兼容 M1+M2（mapping.md §10）：Reader/转译器 + TJSCompiler --dump
    对拍工具链；此时"任意 TJS 脚本"可经旧编译器编译后在 Ciallang 运行，
    覆盖 Array/Dictionary/with 等前端缺口的大头
P2  标准库：Array/Dictionary（含内联字面量）→ Exception → Math → RegExp(选型：
    oniguruma 同源或 std::regex/pcre2) → Date
P3  源码前端补齐（语法矩阵剩余 ❌ 项），使源码路径与字节码路径能力对齐
P4  M3/M4：转译性能（比较写 zf、Inc/Dec 等新 TAC）与边缘语义（EVAL/collapse）
```

> P1 与 P2 的先后可互换：字节码路线先用"旧编译器编译"绕过前端缺口；
> 源码路线独立推进。两条路线在语义层（Value/类系统/标准库）共享成果。
