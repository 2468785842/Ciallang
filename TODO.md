# 后续任务清单

> 已完成项只保留一句话摘要，细节见各提交信息与 [doc/bytecode/ciallang-bytecode.md](doc/bytecode/ciallang-bytecode.md)、[AGENTS.md](AGENTS.md)。
> 当前性能：与原版 TJS2（Release）wall-time 比值（Ciallang/TJS min，2026-10-25，原生类构造批次后）——
> 类实例化 112% / 整数循环 127% / fib(25) 114% / 字符串拼接 155%
>（本机波动 ±10%；string_concat 自 194% 显著回落，payload 单块分配见效）。
> TJS2 语料对拍（2026-10-25）：TJS_ONLY 0 / MATCH 3（class/function/with 全一致）/
> DIFF 3（test_misc、test_variant 为异常消息格式差异，Ciallang 行为正确；
> test_string 双方干净通过，Ciallang 侧无输出、参考实现多 25 行引擎转储）/
> CIAL_ONLY 1（参考实现自身崩溃）。

## 一、正确性（已知未修）

- [ ] `&` 引用存入全局变量后读取会触发 getter（GGlobal 对 Property 的触发与
      property 声明槽位无法区分；语料中引用均为局部变量，暂记已知限制）
- [ ] GC 期 finalize 语义：当前 ~DataObject 禁止 sweep 中重入解释器；
      若需 TJS2 完整语义需"回收队列 + 安全区 drain"

## 二、性能（剩余方向）

- [ ] fib 剩余差距（130%，本机噪声主导）：值拷贝仍在（CallV 压栈同 N×Push 的
      pool 写）——进一步需实参直写被调帧窗口（跳过中转）；调用点内联缓存
- [ ] string_concat 剩余差距（179%）：payload+String 单块分配、Value 内联 SSO
- [ ] loop_int 小幅差距（127%）：词法器 token 判别等新增热路径复核；指令条数
- [ ] 方法访问 NativeFunction 每次分配（GProp 绑定拷贝）：正确修法是调用约定
      携带 objthis（TJS2 同款），属调用点内联缓存的前置；现有基准不敏感
      （stdlib 密集脚本受益）
- [ ] 成员名 Atom 往返：常量池 Atom → String Value → GProp 再 intern 回 Atom；
      方案 LoadAtom 指令或 GProp 操作数直引常量池（指令集变更）

## 三、语言特性缺口（对照 README）

- [ ] 标准库剩余：Date.parse 完整文法
      （tjsdate.y：英文月名/相对表达等，当前仅 "YYYY/MM/DD HH:MM[:SS]" 子集）；
      RegExp 剩余（静态属性 `RegExp.last`、匹配位置改 UTF-8 码点语义——当前为字节偏移）
- [ ] Array 标准库剩余：save/load/pack 系/Assignment 联动
- [ ] Dictionary 标准库剩余：save/load（BinarySerializer）

## 四、工程化

- [ ] vcpkg 无 dumpbin 环境无法自动部署 DLL：补手动拷贝说明或改静态 triplet
- [ ] CI 可跑 Debug 配置（CRT 断言已重定向 stderr，压力模式已可开）

## 已完成（摘要）

- 2026-10-25 with-try 查证批次：对拍+源码定论——"with 内 catch 丢 with 上下文"是原版
  设计现状而非缺陷：裸标识符未命中局部时原版降级为 this.member（T_THIS_PROXY，
  tjsInterCodeGen.cpp:2039 注释 "assume the variable is in this"），只有显式 .member
  裸词链查 with 栈（T_WITHDOT_PROXY）；异常穿越后裸词/裸词链行为两引擎一致
  （多场景对拍全同）。with 的正途是 .member 形态（语料 test_with.tjs 全部此形态，
  本项目已 MATCH）。TODO「WithPush 栈截断」条目关闭；行为锁定测试补齐
  （穿越后新 with 块/裸词链恢复均正常）。96 用例双配置全绿

- 2026-10-25 (const) 内联形态批次：查证定论——原版 `(const)[...]`/`(const)%[...]` 无只读
  也无共享实例语义（tjs.y:795 产生式 = TJSCreateArrayObject 新建 + T_CONSTVAL 节点，
  代码生成仅 VM_CONST 常量池装载；实测写入 a[1]=99 成功；原版 (const)%[...] 甚至解析
  报错）。TODO 原「只读共享实例」条目预期错误，更正关闭；行为锁定测试补齐
  （含嵌套/字典形态，字典形态我们比原版更可用）。95 用例双配置全绿

- 2026-10-25 const 语义批次：原版 const 仅在编译期要求初始化（const E; 报错），
  赋值/自增/复合赋值/for 递增全部放行——**官方语法定稿依据**：tjs.y:319-321 原作者注释
  "const: note that current version does not actually disallow re-assigning new value"
  （非未定义行为，是官方明确声明的设计现状；社区共识同）。据此实现 VarDeclParser 的
  const 必须初始化检查（逗号链逐个查），只读强制不实现。94 用例双配置全绿

- 2026-10-25 RandomGenerator 批次：Math.RandomGenerator（MT19937 类，对齐 tjsMT19937ar-cok
  与 tjsRandomGenerator.cpp）——randomize 三形态（0 参=time 单值播种/整数=(低32,高32)
  init_by_array/Dictionary=状态恢复）、random（rand_double 52 位尾数法）、random32/63/64、
  serialize（Dictionary{state hex 624×8, left, next} 跨引擎格式）。自写 MT（std::mt19937
  内部布局不可移植），固定种子序列与原版逐位一致（对拍 6 值全等；曾因 tempering 未分步
  全链错位）；state 2.5KB 超 GC 节点 512B，堆分离持有。93 用例双配置全绿

- 2026-10-25 默认构造批次：new 无构造器类——原版为未定义行为，按参考实现实测行为
  对齐（较报错更宽容）：仅成员字段初始化（递归含
  基类 + initDefaultVal 默认值 + property 成员），基类构造器不调用（对拍证据：C 无
  构造器 extends B 时 B.b 保持字段默认值 1 而非构造器里的 2）；newClass 前半段
  提取为 initClassFields 复用。92 用例双配置全绿

- 2026-10-25 CallV 变长实参调用批次：新指令 CallV（寄存器列表内嵌，语义 = N×Push+Call，
  消逐参派发轮次；fib 每次调用 3 派发→1）——X-macro/手写子类（Operand 三槽装不下列表，
  Chunk 增 emitRaw 原生发射）/dump/encode/IRGen ProcCall 全链路；dispatch 先读全寄存器号
  再压栈（push 扩池可能迁移寄存器窗口）。基准 fib 130%（本机噪声 ±10% 掩盖单指令收益，
  指令条数结构性下降：fib(10) 顶层调用 5→4 条、递归点 3→1 指令）

- 2026-10-25 Date 批次：Date 标准库全量（对齐 tjsDate.cpp）——构造（now/分量 mktime/字符串
  解析子集）、get 系 8 个（localtime 字段，month 0-based 对齐原版）、set 系 7 个
  （读改 mktime，DateField 枚举助手）、getTime(ms)/setTime(ms)、getTimezoneOffset；
  DateNS 走 TypeId::Date（call 即构造，同 RegExp/Array 模板）。91 用例双配置全绿

- 2026-10-25 Rlc 行号接线批次：未捕获异常的顶层报错与原生错误包装（Exception.message）带
  源码行号——Chunk 挂 Rlc 行号表（IRGen 15 个语句级 generate 入口记录，语句粒度）、
  toBytecode 时 TAC 下标换算为 bc 偏移（CompileResult 暴露 tacStartPos）、unwind 终态与
  run0 catch 两处查询拼接（AST 行号 0-based，输出 +1）。连带修复：GProp 对 void 取属性
  Debug 下 assert 崩溃（改报脚本异常，与 Release 一致）、Rlc::find 尾部误告警、
  上批构造器测试 raw string 含 )" 提前终止导致的编译错误（跑的一直是旧测试二进制）；
  main0 空脚本项核实关闭（内嵌脚本硬编码非空、无脚本入参，路径不存在）

- 2026-10-25 原生类构造批次：new 编译为透明调用（IRGenerator 对 New 直接求值 rhs）——
  原生类构造只需把全局类对象的 call() 从抛错升级为构造。RegExp(pattern, flags?) /
  new RegExp(...)（全局 "RegExp" 挂空模式占位实例）+ new Array() / new Dictionary()
  （TJS2 构造器为空、参数忽略）。此机制为后续 new Date()/new RandomGenerator() 的模板

- 2026-10-25 性能小组合批：TypeId 扩 Global/Class 专用值（不进方法表，仅分派标识），
  GlobalObject/ClassObject 覆写 typeId()——GProp/DProp 对象分支的 Global/Class 两次
  dynamic_cast 全部消除；SharedPayload::makeSingle 单块分配（payload 头+T 就地构造，
  Value(String/Octet)/Constant 字面量负载/IRGen octet 字面量四处切换，拼接路径每次
  省一次堆分配）——string_concat 基准 194%→155%。90 用例双配置全绿，对拍一致

- 2026-10-25 Math+数字字面量批次：Math 标准库（16 方法+8 常量+Xorshift random，对齐 tjsMath.cpp；
  MathNS 承载对象走 TypeId::Math/NativeRegister 分派，max/min 含 NaN 传播与 ±0 位级语义、
  round=floor(x+0.5)）+ 数字字面量五处修复：extractNumber 循环条件改以 validDigits 为权威
  （旧十进制字符集致 0x1.07CCp14 在 'C' 处中断）/退出时统一回退终止符（旧吞 ')'）/p 指数符号
  分支直接读完指数（旧的跳空白+break 留下空指数从未工作）/八进制 peek 回退（010 旧得 0）/
  parseNonDecimalReal 弃 IEEE 位组装改朴素求值（旧的 0x1p-2 得 4）/科学计数法符号可选
  （1e3 旧词法报错）且指数非零强制 Real（1e-12 旧截断 0）。90 用例双配置全绿

- 2026-10-25 脚本回调机制批次：VM::callValue（原生层同步调用脚本函数，TJS2 FuncCall 对应物；
  tmp 宿主帧承接返回值 + 全区哨兵 throwHandler 截住 unwind + ScriptExceptionCarrier 专用载体
  转回脚本异常协议）——两个关键陷阱均已踩平并记注释：① 压栈顺序必须 tmp 帧→实参→func 帧
  （CallFrame 以 allocFrame-arity 回退对齐参数寄存器，实参先压会把 tmp 槽夹进参数区整体错位）；
  ② 哨兵必须显式 toBytecode（bc 层 handlers 惰性编译由 FETCH_NEW_FRAME 触发，tmp 帧永不被
  派发执行）。基于此底座：Array.sort（排序字母 + - 0 9 a z / 比较器回调 / 稳定开关，对齐
  tjsArray.cpp）+ RegExp.replace 函数回调（对齐 replace_regex，回调收匹配数组）；main.cpp
  Debug CRT 断言重定向 stderr（断言弹窗在无交互环境表现为"挂起"）。88 用例双配置全绿

- 2026-10-25 热路径优化批次：GProp 消同指令双重 intern + 对象分支直取 typeId()（一次虚调用
  替代 ValueToTypeId 的 3 连 dynamic_cast；TypeId 枚举提取 types/TypeId.hpp 破循环依赖，
  Object::typeId() 虚函数，Array/Dictionary/RegExp 覆写）+ Runtime::wkAtoms 构造期驻留
  高频属性名（Array::isCountAtom 每次 intern×2、RegExp 属性协议每次 intern×9 → 零哈希比较）+
  Value::add 补 String+String 快路径（跳过通用分支的两次 Value 拷贝与 toString no-op）。
  基准：string_concat 185%→179%；其余持平（现有基准不覆盖容器属性访问路径）。
  已评估未做：Chunk 编码后释放 TAC（Class.cpp 两处 Debug assert 与 Debugger dump 依赖，
  收益为内存非速度）；NativeFunction 绑定拷贝与成员 Atom 直达记入性能节剩余项

- 2026-10-24 RegExp 完整 API 批次：`/pattern/flags` 字面量 flags 接入（g=全局/i=忽略大小写/l=忽略，
  lexer 捕获、token 编码 "pattern\nflags"、NewRegExp 处解码）+ RegExp 方法
  test/match/exec/compile/replace/split（对齐 tjsRegExp.cpp：exec 更新 index/lastIndex/lastMatch/
  lastParen/leftContext/rightContext/input 与 matches 数组、g 推进 start、match 无副作用）+
  实例属性协议（matches/start 可写/其余只读，走 RegExp::getProp/setProp）+ TypeId::RegExp 接入
  NativeRegister/GProp + String.replace 改 g 全局语义 + String.split/Array.split（正则/分隔字符集
  双模式，TJS2 split 字符集语义）+ VM::create 沉睡 bug 修复（RootGuard 无法隐式转 T*）；
  连带修复 String SSO 边界 bug：concat/append/reserveExact 的短串阈值（< 21）与构造/析构/move 的
  长串判据（> 21）不一致——恰好 21 字符的结果丢尾字符+泄漏缓冲（glm-5.3-flash 所写热路径 helpers
  未吃透 String SSO 不变量，已统一为 <= / > 并在 String.hpp 记注不变量；补 20/21/22 边界回归测试）

- 2026-10-24 RegExp 批次：`/pattern/` 正则字面量（lexer 按前 token 判别除法 vs 正则，
  JS lexer 标准做法；TokenType::RegExp + NewRegExp 指令 71→72；std::regex ECMAScript）+
  String.replace（替换第一处）+ 字符串下标 TJS2 越界语义（str[len] 空串、str[>len] 报错）+
  str.length 改 UTF-8 码点计数（"abc漢字"==5）+ toUpperCase/toLowerCase 改返回新串（不改原串，
  对齐 TJS2）+ sprintf 的 %c 接受字符串参数与 %04X 等整数宽度/填充生效

- 2026-10-24 with 语句批次：`with(expr) stmt_or_block` + 裸词链 `.member`（新增
  WithPush/WithPop/GetWith，指令 68→71；withStack 上下文栈挂 Runtime handle 保 GC，
  帧/unwind 级截断；裸词链基座空栈得 global——顶层 `.gvarl = 7` 语义）；
  连带修复 conformance 对 test_with 的 MATCH 假象（此前 cial 解析失败 rc 被忽略）

- 2026-10-24 Array/Dictionary 批次：`[...]`/`%[...]` 字面量（sym: 与 "k" >= v 两种键形式，
  => 词形即 >= 由上下文区分）、通用下标读写 `a[i]`（新增 NewArray/NewDict/GetIndex/SetIndex，
  指令 64→68；数组负下标/越界写自动扩展/越界读报脚本异常；字符串下标保持 charAt 语义）、
  Array/Dictionary 运行时容器（GC 接入）+ 方法表（push/pop/unshift/shift/insert/remove/
  erase/reverse/join/assign/clear/erase + count/length 属性）、全局 Array/Dictionary
  类对象（Dictionary.assign incontextof 惯用法）；连带修复 ChgThis 不认 NativeFunction 段错误

- 2026-10-24 property 引用批次：前缀 `&`/`*`（新增 GPropRaw/GRefRaw/DPropRaw/DerefGet/DerefSet，
  指令 59→64）、成员后缀 `obj.prop++`（getter+setter 链）、后缀 `i++` 旧值语义；
  连带修复 super.<Ctor>() 段错误（_lastSuperClass 未记录时按 extends 解析 + 构造器
  经 getProp 可访问返回类对象）、GLocal/DLocal 改纯读写（局部变量=VM_CP 语义，
  Property 可作一等值传递）、DProp 改 setter 感知（槽位为 Property 调 setter）；
  test_class 语料 MATCH

- 2026-10-24 兼容性批次：前缀 `++ -- # $ delete`、`const` 声明与 `for(const)`、
  `str.length` 原生属性、后缀 if 绑定修正（假条件整体不赋值）、三元优先级修正、
  `#` 对非字符串先字符串化、octet 算术改为报错、一元 `+` 转数值、
  字符串→数值支持负号与小数写法、`void` 字面量发射、instanceof 基础类型匹配、
  大整数字面量不经 double 往返、Exception 内建类（TJS 源自举）、
  原生异常转脚本可捕获异常（包装为 Exception 对象）、
  未初始化 `var a,b,c;` 寄存器别名修复、函数默认参数 TestVoid 守卫（修实参覆盖）+
  构造器 arity 用 FuncMeta::arity 的修复、sLEB128 maxLen 8→10（i64 字面量）

- 2026-10-22 语法批次：`^ | &` 及复合赋值、后缀 `expr if cond`、下标 `s[i]`、`typeof`（新增 TypeOf 指令）、`(int)x` 转型、前缀 `+`/`~`、octet 字面量、函数提升、`catch()`；连带修复 zigzagDecode 负立即数差 1、Constant 拷贝语义、octet 词法
- 2026-10-22 二轮热点：GGlobal 零拷贝、池水位线收缩、dispatch 整数特化、Ret 整数直写、this 快通道
- 2026-10-22 崩溃修复：generateFuncMeta 空 Opt 段错误、未捕获异常静默 terminate、异常后 VM 状态残留
- 2026-10-21 性能四项：run0 扁平字节码 55/55 唯一路径、GC 根体系（MarkRootProvider + 6 处标记缺陷）、Value SharedPayload、thisData 缓存
- 2026-10-21 设施：bench/ 对拍脚本、CMake 化基准与 conformance 报告、CMakeUserPresets、main.cpp 补 initNativeMethod
- 2026-10 上旬：Ret 惰性 Err、整数快路径、Debug 构建修复、for(var)/增减运算符、Atom 跨 Runtime 缓存、FastRegisterPool `_sp`
