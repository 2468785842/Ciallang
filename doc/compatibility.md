# 兼容性与已知差异

> Ciallang 对原版 TJS2（TJSCompiler）的三维兼容状态总览。
> 语法细节见 [syntax.md](syntax.md)；字节码规范与转译设计见 [bytecode/](bytecode/) 目录。
> 状态：✅ 已有实现及对应验证 / ◐ 部分 / ❌ 未实现；不代表所有输入都与参考引擎一致。

---

## 1. 源码兼容（.tjs 脚本直接运行）

| 领域 | 项 | 状态 |
|---|---|---|
| 预处理 | @set / @if / @endif | ✅ |
| 语句 | var/if/while/do-while/for/switch/try-catch/return/break/continue/function/property/class/debugger | ✅ |
| 语句 | with | ✅（withStack 上下文栈，异常随帧截断）|
| 语句 | for(const …)、var 类型注解 | ✅ for(const)；const 声明（只读性不强制）；类型注解 `: symbol/void/int/real/string/octet`（var/函数参数/返回值/getter/setter 全位解析并丢弃——tjs.y "Ignore types"，2026-10-29 对齐） |
| 表达式 | 赋值全家（含 `<->`、`\|\|=`、`&&=`、`>>>=`）、?:、逻辑/位/比较/严格相等/移位/乘除整除 | ✅ |
| 一元前缀 | `!` `new` `invalidate` `-` `~` `++` `--` `delete` `typeof` `#` `$` `+` `&` `*`（及前后缀 `isvalid`） | ✅ |
| 一元后缀 | `()` `.` `++` `--` | ✅ |
| 一元后缀 | `[]`（下标读写：数组/字典/字符串） | ✅ |
| 一元后缀 | `!`（eval 即时求值，EvalStr 指令；嵌套 eval 可用） | ✅ |
| 容器成员 | Array/Dictionary 方法面（sort/split/save/load/pack 系含） | ✅（逐方法状态见 §3 标准库兼容） |
| 中缀 | instanceof / incontextof / `in` | instanceof ✅（基础类型按类型名匹配）/ incontextof ✅；`in` ✅ 保留字行为对齐（原版 codegen 无 T_IN 分支——节点与操作数均不求值、结果 void；官方リファレンス亦无此运算符，2026-10-29 实测定论，见 `tests/compat_reserved_test.cpp`） |
| 字面量 | 全部标量（含 i64 大整数字面量精确解析）+ 函数表达式 + @"…" 模板串 | ✅ |
| 字面量 | `[...]` Array、`%[...]` Dictionary、`<%%>` Octet、`(const)` 常量形 | ✅ 普通字面量与 `(const)` 编译期容器、常量元素限制及跨调用共享，见 [语法 §8](syntax.md#8-内联字面量array--dictionary--常量折叠形) |
| 后缀条件 | `expr if cond`（假条件整体不赋值，tjs.y expr 层级绑定） | ✅ |
| 函数 | 默认参数（TestVoid 守卫，实参优先） | ✅ |
| 函数 | 收拢 `args*`、省略调用 `...`、展开调用 `expr*` | ✅（2026-10-27 源码路径补齐，与字节码路径能力对齐） |
| 类 | 多继承/slot 链/super/构造函数/property（29 个语义测试） | ✅ |
| 类 | static / finalize 触发语义 | finalize ✅（GC 保活队列 + 所属 VM 安全点 drain，显式 invalidate 保持同步，`src/gc/gc.hpp`、`src/types/Class.cpp`、`src/vm/VMState.cpp`；Runtime 销毁不执行脚本）。static ✅ 定论（2026-10-30）：原版 tjs.y 保留 static/private/public/protected 但产生式零使用，类内 `static var` 在原版即语法错误；本项目同为编译错误（tjsLex.cpp:926 仅注册保留字），行为锁定 tests/compat_reserved_test.cpp |
| 成员 | `delete obj.member`（实例自有成员） | ✅ |
| 成员 | `str.length` 只读属性 | ✅ |
| 异常 | Exception 类（message 成员）+ 原生错误转脚本可捕获异常 | ✅ |

## 2. 字节码兼容（TJSCompiler 编译产物 `"TJS2" "100\0"` 文件）

执行路径：加载 → **转译为 Ciallang TAC** → 现有 VM 执行，见
[bytecode/mapping.md](bytecode/mapping.md)。
Reader 与转译执行已接入；当前有 7 个带源码的入库 fixture。已有解码分支不等于所有语义都已对齐，已知问题见 [转译边界](bytecode/mapping.md#8-已知差异与待修复项)。

| 项 | 状态 |
|---|---|
| 容器/常量池/context 解析（Reader） | ✅ 有截断、索引及布局回归；不代表所有元数据组合均已验证 |
| 顶层/函数 context 转译（算术/比较/跳转/调用/flag 物化） | ◐ 已实现；`CHKINS` 按 target/name 原地写回，其他边界见转译文档 |
| 属性访问（GPD/GPI/SPD/…/S 变体 + P 变体/GETP/SETP/DELD/TYPEOFD） | ◐ 缺失成员的 typeof、proxy 回落、Operation 原子语义等尚有差异 |
| 类 / NEW / try-catch / property / extends | ◐ fixture 覆盖常见模式；类体与父类表达式只支持加载期可识别模式 |
| collapse/展开/省略调用、EVAL/EEXP、比较与转换 | ✅ 已有指令及 fixture；具体映射与边界见转译文档 |
| test_variant（octet 语料） | ✅ 已有 fixture 和对应源码 |
| test_string | ❌ 尚无入库字节码 fixture；旧记录中参考导出曾崩溃，未据此推定新版本仍不可用 |
| 对拍工具链 | ✅ `bench/bytecode_conformance.py` + `cial --tjsbc`；参考 `d8044c3` 已提供 `--dump`。当前 7 个 fixture 均有源码，`test_misc` 异常消息格式与行号仍会产生 `OUTPUT_DIFF`，默认全量比较并非全绿 |

## 3. 标准库兼容

| 库 | 原版能力（概要） | 状态 |
|---|---|---|
| （宿主）print | 控制台输出 | ✅ print/println |
| String 内建成员 | length/charAt/indexOf/toUpperCase/…/sprintf/replace/split/escape/trim | ✅；replace 委托 regexpReplaceIn（g 全局替换、非 g 仅首处，2026-10-30 实测复核）；length 为码点计数（其余方法字节语义） |
| Array | 数字下标数组；load/save(BinarySerializer)/sort/join/split/pack… | ✅ 字面量/下标/push/pop/unshift/shift/insert/remove/erase/reverse/join/split/sort/count/assign ✅；save（文本行）/load（文本行）/saveStruct（文本+二进制）/loadStruct（仅二进制，原版同）/pack/assignStruct ✅（2026-10-26）；文件文本 UTF-8 写出、读取自动识别 UTF-8/UTF-16LE/BE BOM |
| Dictionary | 哈希字典；load/save/assign/clear | ✅ 字面量/下标/assign/clear/erase/count/assignStruct/saveStruct（文本+二进制）/loadStruct（二进制入自身；非二进制回落文本解析且解析值作返回值不写入自身——原版同款怪癖）✅（2026-10-26）；load/save 原版即 TODO 空实现，按特性同步 |
| Exception | 异常类（message 成员），脚本异常载体 | ✅（TJS 源自举，main.cpp 注入；原生错误自动包装为 Exception 实例） |
| Math / Math.RandomGenerator | abs/floor/…；Xorshift / MT19937 | ✅ 18 方法与原版逐一对应（abs/acos/asin/atan/atan2/ceil/exp/floor/log/pow/max/min/random/round/sin/cos/sqrt/tan，tjsMath.cpp:116-407 核对，2026-10-30）+ 常量 + RandomGenerator（MT19937 序列与原版逐位一致、serialize 格式互认） |
| Date | get/set 系列 + 字符串 parse（tjsdate.y 文法） | ✅ 全部方法+构造+parse 完整文法（tjsdate.y 全 11 形态/月名/星期/AMPM/时区名与 ±hhmm/括号忽略段/两位年/小数秒，2026-10-26 对拍逐位一致）；注意：进程首个字符串构造因 CRT `_timezone` 惰性初始化偏移 +2×\|tz\|（原版同款怪癖，已同步保留，见 DateParser.cpp build() 注释） |
| RegExp | compile/test/match/exec/replace/split（oniguruma）；属性 matches/start/index/lastIndex/input/lastMatch/lastParen/leftContext/rightContext | ✅ /pattern/flags 字面量（g/i）+ 全部方法 + 全部实例属性 + 静态属性 `RegExp.last`（仅 test/exec 更新且失败也更新）；`new RegExp(pattern, flags?)` ✅；replace 回调保留已有 this 绑定（含全局函数声明）；未绑定函数表达式/原生回调以 regexp 作 objthis（原版 `replace_regex` 同语义；实现 `src/stdlib/RegExpLib.cpp`、`src/vm/VM.hpp`，回归 `tests/native_function_test.cpp`）。位置语义：UTF-8 码点（TJS2 为 UTF-16 码元，BMP 内一致；2026-10-26 从字节偏移改定）。已知偏差：`index` 属性成功匹配的非 ASCII 值——原版 tjsRegExp.cpp:669 少除 sizeof(tjs_char)（UTF-16 单位+字节混加，start>0 时可与 lastIndex 相等），本项目为纯码点 |
| Octet | 字节流 + pack 系 | ◐ 类型+字面量+unpack ✅（2026-10-26，tjsOctPack 全类型；Octet 仅有的方法即 unpack）；偏差：原版 BinToHex 负 len 钳位 bug 使 unpack h\*/H\* 恒返回空串，本项目返回正确 hex（同 tjsOctPack.cpp HexToBin 大写仅收 A-E 的漏写按特性同步） |
| Scripts | execStorage/evalStorage/compileStorage/exec/eval/dump/getTraceString/setCallMissing/getClassNames/textEncoding（tTJSNC_Scripts，ScriptMgnIntf.cpp:1295-1570） | ◐ execStorage ✅（语句脚本执行，全局上下文，嵌套双帧哨兵协议，`src/stdlib/ScriptsLib.cpp`）；evalStorage ✅（表达式求值返回值）；**compileStorage ❌ 不实现**——Ciallang 无库内字节码导出能力（`--tjsbc` 是 CLI 加载方向，非导出），按偏差记录；exec/eval/dump 等其余成员未实现。路径解析：绝对路径直用，相对路径相对当前执行脚本所在目录（krkr2 TVPSearchPlacedPath 的 CLI 简化，无 xp3 归档）；缺失文件抛可捕获 Exception（消息含路径）。回归 `tests/scripts_test.cpp` |

### 标准库边界处理（2026-10-31）

- `String.indexOf` 对负数起点返回 `-1`；`substring/substr` 的超大正长度裁剪到剩余串长，避免有符号加法溢出。参考实现 `tjsInterCodeExec.cpp` 的 `indexOf` 分支未检查负数起点，本项目显式拒绝此越界输入（`src/stdlib/StringLib.cpp`）。
- `Math.RandomGenerator` 恢复只接受初始化状态 `{left=1,next=0}`，或 `1 <= next <= 624` 且 `left + next = 625` 的取数状态；拒绝整数窄化、非法游标及损坏的状态串，失败时保留原有序列。正常 `serialize()` 检查点及跨块序列不变（`src/stdlib/RandomLib.cpp`）。
- 浮点 `unpack` 的 `d/f` 字段仅按实际读取字节推进一次游标；显式长度超过输入时按既有规则补零形成最后一个字段，后续字段不越界读取（`src/stdlib/SerializeLib.cpp`）。
- 上述行为回归见 `tests/native_function_test.cpp` 的 `[stdlib][regression]` 用例。

### Scripts 双引擎对拍（2026-10-30）

`tests/test_files/tjs2/test.tjs`（execStorage 加载 6 个兄弟语料 + 解析器/实数语料断言）
已在本仓库 `cial.exe` 与参考适配版 `TJSCompiler.exe` 双侧通过（均退出码 0）。语义消息
序列 17/17 对齐；差异仅为输出协议/诊断格式类：

1. 参考引擎 Debug.message 带时间戳前缀、被捕获异常打印 VM 转储、退出前转储脚本块
   （其 main 的 Dump 流程）——输出协议差异，非行为差异（§4"原生错误报告"同类）。
2. test_misc.tjs:149 octet 除法异常的 `e.message`：参考引擎为空串，本项目为
   "unsupported operand types for \`/\` (line 149)"（原生错误包装 + 行号装饰）——
   异常消息文案差异，控制流（catch 触发、后续消息序）一致。
3. `success: class ClassA::finalize` 双方均不打印（SubClass 实例未 invalidate，GC 时机
   不触发 finalize）——双方一致。

参考引擎 `d8044c3` 已提交 `src/syntax/library/ScriptsInterface.{h,cpp}` 及 `src/main.cpp` 的 Scripts 注册；上面的测量是历史记录，不是本轮重新对拍。

## 4. 运行时行为差异（已知，须保持记录）

| 项 | TJSCompiler | Ciallang | 处理 |
|---|---|---|---|
| 内存管理 | 纯引用计数（tTJSVariant CopyRef） | 标记清除 GC + 引用计数混合 | 对象存活依赖显式根；见 [FAQ](faq.md#宿主持有-value-或-shared_ptr-就能保住脚本对象吗) |
| 性能 | fib(25) 基准 | 2026-10-28 同机对照：fib 约 1.5×、原生方法约 1.9×；整数循环/字符串拼接已与旧版持平或反超（详见 doc/performance.md） | [性能待办](../TODO.md#性能与优化) |
| 字符串内部编码 | UTF-16（tjs_char） | UTF-8 | 字节码转译处做转换（代理对）；str.length 为码点计数 |
| 调用栈深度 | 无同值上限 | 1024 | 记录差异 |
| 局部变量名（字节码态） | 序列化不含 Namespace | 同样无 | 一致 |
| 原生错误报告 | 未捕获异常打印 VM 转储（含 ip） | 顶层打印 Exception.message 文本 | 语料对拍报告的 DIFF 部分由此产生，语义等价 |
| `const` 只读性 | 仅编译期要求初始化；赋值/自增/for 递增放行——tjs.y 原作者注释明确"current version does not actually disallow re-assigning"（非 UB，官方设计现状） | 一致（2026-10-25 对齐，tjs.y 佐证） | 无差异 |
| `#`/`$` 操作数 | 严格类型（字符串/整数） | `#` 对非字符串先字符串化（`#65 == 54`，对齐 kirikiri 语料期望） | 记录差异 |
| 后缀 `i++` 表达式值 | 返回自增前旧值 | 同（旧值，2026-10-24 修正） | 一致 |
| `isvalid` 非对象值 | 恒 true（VM_CHKINV：`Type() != tvtObject` 即 true，tjsInterCodeExec.cpp:1177） | 一致（2026-10-30 修正——曾对非对象抛"Invalid type get as Object"）；对象按 IsValid 判定，invalidate 后 false | 无差异 |
| `instanceof` 内置对象 | `TJSDefaultIsInstanceOf` 沿原生类链按名匹配：Array/Dictionary/RegExp 实例与函数对象对各自类名恒真 | 一致（2026-10-30 修正——基类 instanceOf 曾恒 false）；保留 Atom + 各类型覆写（`src/runtime/Runtime.hpp`、`src/types/*.hpp`） | 无差异 |
| 无 return 函数返回值 | void（编译器补 ret 读 void 常量） | 一致（2026-10-30 修正潜伏缺陷——补丁 ret 的 void Load 曾发射到 parseAst 后的暂存 chunk，返回寄存器池陈旧值；`src/gen/IRGenerator.cpp` patchVoidRet） | 无差异 |
| 回调异常值传播 | 宿主回调（sort 比较器/replace 回调/即时求值）抛出的脚本异常，外层 catch(e) 收到抛出值本身（try handler 的 exValueReg 回写） | 一致（2026-10-30 修正——ThrowHandler 以 0 表示"无槽位"与寄存器 0 冲突，异常值曾丢失、catch 读到池内陈旧值；现以 0xFFFF 为无槽位哨兵，寄存器 0 合法；`src/vm/VMState.cpp` unwind、`src/vm/Bytecode.hpp`、回归 tests/try_catch_test.cpp） | 无差异 |
| `+` 的 void 幺元 / `(string)void` | `operator+=`：任一侧为 String 时两侧 `AsString()`（void→NULL 即空串，tjsVariant.cpp:1095-1128、683-686）；void±Integer/Real 恒等；void+void → AsReal 兜底得 Real 0；octet+String 抛错。`ToString()` 对 void 给空串（:411-415）、octet 抛错（:443-445） | 一致（2026-10-30 修正——`(string)void` 与 `"a"+void` 曾报 "Invalid type cast / unsupported operand"；`src/types/Value.cpp` toString Void 分支） | 无差异 |
| `&` 引用作用域 | 全局/局部/对象成员均可存属性引用；普通全局/成员读取触发 getter，局部读取仅复制引用 | 一致；`&name` / `&obj.member` 取引用本体，`*(&name)` 可显式解引用全局槽 | 无差异；回归见 `tests/tjs/property/` |

属性引用作用域已于 2026-10-30 复核：原版 `TJSDefaultPropGet` 在未设 `TJS_IGNOREPROP`
时调用属性默认 getter，`VM_GPDS` / `VM_GPIS` 则以该标志取本体；局部变量走 `VM_CP`
纯复制（`../TJSCompiler/src/syntax/tjsObject.cpp`、`../TJSCompiler/src/syntax/tjsInterCodeExec.cpp`）。
因此“只有局部变量能持引用”不是两引擎差异。全局槽中的引用应通过 `&reference` 取出，
再用 `*` 或作为函数参数传递；普通读取仍按属性调用语义执行。
已用两份不同最小脚本确认本地参考 CLI 适配二进制接收脚本路径，再以含自校验的
全局/局部/对象成员脚本对拍，两引擎均成功。参考二进制报告 TJS2 2.4.28，SHA-256 为
`D404F11D5E173194E02AD92F5F2C13A0161F5A99CC49F6B23838076D2D5D5BCA`；
参考引擎旧入口曾硬编码路径；`d8044c3` 已提供路径参数和导出 CLI，见 [FAQ](faq.md#对拍前怎样确认参考引擎可用)。

## 5. 后续工作

未完成事项集中记录在 [TODO.md](../TODO.md)，性能测量见 [performance.md](performance.md)。
