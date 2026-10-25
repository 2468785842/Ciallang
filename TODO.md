# 后续任务清单

> 已完成项只保留一句话摘要，细节见各提交信息与 [doc/bytecode/ciallang-bytecode.md](doc/bytecode/ciallang-bytecode.md)、[AGENTS.md](AGENTS.md)。
> 当前性能：与原版 TJS2（Release）wall-time 比值（Ciallang/TJS min，2026-10-25，回调机制批次后）——
> 类实例化 125% / 整数循环 124% / fib(25) 121% / 字符串拼接 184%
>（本机连续构建后波动加大 ±10%，多轮区间 110~159%，无系统性回归）。
> TJS2 语料对拍（2026-10-25）：TJS_ONLY 0 / MATCH 3（class/function/with 全一致）/
> DIFF 3（test_misc、test_variant 为异常消息格式差异，Ciallang 行为正确；
> test_string 双方干净通过，Ciallang 侧无输出、参考实现多 25 行引擎转储）/
> CIAL_ONLY 1（参考实现自身崩溃）。

## 一、正确性（已知未修）

- [ ] `&` 引用存入全局变量后读取会触发 getter（GGlobal 对 Property 的触发与
      property 声明槽位无法区分；语料中引用均为局部变量，暂记已知限制）
- [ ] GC 期 finalize 语义：当前 ~DataObject 禁止 sweep 中重入解释器；
      若需 TJS2 完整语义需"回收队列 + 安全区 drain"
- [ ] `main0.cpp`（cial_bc）对空脚本仍会走到 assert（未验证）
- [ ] 未捕获的原生 C++ 异常顶层报错走 unwind 路径，无源码行号（Rlc 未接线）

## 二、性能（剩余方向）

- [ ] fib 剩余差距（132%）：实参 Push 拷贝与指令条数（TJS 为密集指令编码）；
      可选：参数直传被调帧窗口、调用点内联缓存
- [ ] string_concat 剩余差距（179%）：payload+String 单块分配、Value 内联 SSO
- [ ] 类实例化/loop_int 小幅差距（115%/122%）：GProp 通用对象分支仍有
      GlobalObject/ClassObject 两次 dynamic_cast（可扩 typeId()）；
      词法器 token 判别等新增热路径复核
- [ ] 方法访问 NativeFunction 每次分配（GProp 绑定拷贝）：正确修法是调用约定
      携带 objthis（TJS2 同款），属调用点内联缓存的前置；现有基准不敏感
      （stdlib 密集脚本受益）
- [ ] 成员名 Atom 往返：常量池 Atom → String Value → GProp 再 intern 回 Atom；
      方案 LoadAtom 指令或 GProp 操作数直引常量池（指令集变更）

## 三、语言特性缺口（对照 README）

- [ ] 标准库：Date / Math 完整 API；RegExp 剩余（`new RegExp(...)` 构造器、静态属性
      `RegExp.last`、匹配位置改 UTF-8 码点语义——当前为字节偏移）
- [ ] Array 标准库剩余：save/load/pack 系/Assignment 联动
- [ ] Dictionary 标准库剩余：save/load（BinarySerializer）
- [ ] `(const)[...]` / `(const)%[...]` 的只读共享实例语义（当前按运行时新建处理，行为兼容）
- [ ] 十六进制浮点字面量 `0x1.07CCp14`、二进制浮点 `0b1.1`（test.tjs 后段依赖）
- [ ] const 只读性检查（当前 const 与 var 等价，仅语法接入）
- [ ] with 块内 try/catch 穿越 WithPush 的栈截断（当前仅帧级恢复；语料未覆盖）

## 四、工程化

- [ ] vcpkg 无 dumpbin 环境无法自动部署 DLL：补手动拷贝说明或改静态 triplet
- [ ] CI 可跑 Debug 配置（CRT 断言已重定向 stderr，压力模式已可开）

## 已完成（摘要）

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
