# 后续任务清单

> 已完成项只保留一句话摘要，细节见各提交信息与 [doc/bytecode/ciallang-bytecode.md](doc/bytecode/ciallang-bytecode.md)、[AGENTS.md](AGENTS.md)。
> 2026-10-28 macOS arm64 Release 同机交替对照（1 轮预热 + 7 轮取 wall-time 中位数）：
> 本批改动后/改动前：类实例化 100.4% / 整数循环 51.0% / fib 93.7% /
> 原生方法 72.7% / 字符串拼接 70.5%。与本机 TJS2 的比例分别为
> 104.0% / 68.8% / 147.7% / 185.5% / 98.0%；不同机器的历史比例不可直接比较。
> 具体数据、方法与验证范围见 [doc/performance.md](doc/performance.md)。
> TJS2 语料对拍（2026-10-25）：TJS_ONLY 0 / MATCH 3（class/function/with 全一致）/
> DIFF 3（test_misc、test_variant 为异常消息格式差异，Ciallang 行为正确；
> test_string 双方干净通过，Ciallang 侧无输出、参考实现多 25 行引擎转储）/
> CIAL_ONLY 1（参考实现自身崩溃）。
> **字节码兼容 M1–M4 完成（2026-10-27）**：`cial --tjsbc` 全链路——tjs2 语料 12 文件
> 字节码路径全量通过（test_class/test_function/test_misc/test_variant/test_with +
> s1–s7；test.tjs 需宿主 Scripts 对象双方同位置失败、test_string 上游 --dump 崩溃）；
> 128 操作码转译覆盖。见 doc/bytecode/mapping.md。

## 一、正确性

- [x] GC finalize：不可达对象先入保活队列，由所属 VM 的安全点 drain，覆盖环、复活、回调内 GC、异常和一次性终结。跳过编译器合成的空 finalize；修正其寄存器计数；空闲节点重建 Header，Runtime 销毁释放 C++ 资源但不运行脚本。见 `src/gc/gc.*`、`src/types/Class.*`、`src/runtime/Runtime.hpp`、`tests/interpreter_test.cpp`。

## 二、性能

- [x] Value 内联 SSO：14 字节内无需字符串负载分配；String 与 Value 均保持 16B，长串共享负载不变。增加字符串+整数快路径，修正 NewRegExp 借用临时 Value 的字符串寿命。见 `src/types/Value.*`、`src/types/String.*`、`src/vm/Constant.cpp`、`tests/value_storage_test.cpp`。
- [x] 优化器收益：profile 定位循环的比较/Test 与全局整数槽访问；增加 CompareTest 融合，保留结果寄存器并重映射跳转/try/RLC。配套整型槽直接读写减少 Value 临时对象。本批整数循环耗时约为原来的 51%，不单独归因于某个 pass。见 `src/gen/Optimizer.*`、`src/vm/VMState.cpp`、`tests/optimizer_test.cpp`。
- [x] 原生方法调用分离 objthis：GMethodC/CallMethodV 直接携带接收者，避免每次调用分配 NativeFunction；提取方法为一等值时继续创建独立绑定。覆盖嵌套调用、宿主替换方法和分配数量测试。见 `src/gen/IRGenerator.cpp`、`src/types/Function.*`、`tests/native_function_test.cpp`。
- [x] 访问缓存：64 项原生成员调用点缓存按注册版本失效；64 项全局槽缓存按插入/删除版本失效，函数替换读取最新值。本批 fib 耗时约为原来的 93.7%，仍有性能差距。见 `src/vm/VMState.*`、`src/types/Class.cpp`、`src/stdlib/NativeRegister.hpp`。

## 三、语言特性缺口

本轮未新增语法；已知语义差异继续在 `doc/compatibility.md` 跟踪，不以本清单已完成代表完全兼容。

## 四、工程化

- [x] Windows DLL 部署：Windows 预设改用 `x64-windows-static` 与静态 CRT，移除手动复制 DLL 的需求，见 `CMakePresets.json`。本机未执行 Windows 构建，待 CI 验证。
- [x] Debug/Release CI：新增 Linux/macOS/Windows 六配置，构建 CLI 和测试并运行 CTest，见 `.github/workflows/ci.yml`。工作流尚未推送运行。
- [x] Release 缓存防护：配置阶段拒绝缺少优化标志的 Release 缓存；空标志配置失败已验证，见 `CMakeLists.txt`。
- [x] Parser 无条件 `[scan]` 打印：核实当前 `src/parser/Parser.cpp` 已不存在，关闭过时条目。
- [x] 历史测试文件回归主线（2026-10-29）：statement/type_system/ieee/integration 四个被注释禁用的文件修复为现代 API 并启用——AST 结构直查（Parser 改经 `Context::pp()` 构造）、Value `as*`/Ret 运算面、`types/Real.hpp` IEEE 位工具（原 `lexer/IEEETypes.hpp` 已并入该头，此前零覆盖）、VM 门面多特性组合端到端；测试 120→143 用例（Windows Debug 全绿）。同步 doc/compatibility.md 与实现状态对齐。

本机最终验证：macOS Debug、Release、UBSan 均 **120/120 通过**（上一批次时点）；Windows Debug **149/149 通过**（2026-10-29，4 个历史测试文件回归主线 + 常量传播/回边检测后）。12 个字节码样本全部成功；从 `/tmp` 配置时 TJSCompiler 相对路径解析通过。ASan 运行库在 macOS 初始化阶段自旋，尚未进入测试，不能计为通过。

## 已完成（摘要）

> 细节见各提交信息；字节码批次见 [doc/bytecode/mapping.md](doc/bytecode/mapping.md)，性能见 doc/performance.md。

- 2026-10-29 常量传播与测试主线回归批次：整数常量 pass 升级为块间状态分析（唯一前驱块继承前驱出口常量状态，回边/多前驱/try handler 汇聚保守放弃）；CFG 增加支配树回边检测与自然循环识别（getLoops，循环体变换待后续）；CFG/常量传播/死代码消除/循环优化（回边检测）在 README 核心特性清单勾选；statement/type_system/ieee/integration 四个历史测试文件修复为现代 API 回归主线（120→149 用例）；Windows 首次双引擎基准与文档同步见 doc/performance.md
- 2026-10-28 macOS 构建验证：修正 DateParser `::tzset()` 命名空间与系统头引用，fuzz 测试 CRT 断言重定向限定 MSVC，双配置 110/110 通过
- 2026-10-28 性能批次：成员名 Atom 直引指令 GPropC/DPropC/GPropRawC/DPropRawC/DelPropC（86→92 条）+ CallV 实参直写 argWindowDirect 帧布局；fib 内部计时 -25%
- 2026-10-27 源码前端补齐：`...`/`*` 语法（收拢参数/展开调用/省略透传），test_function 源码路径全量通过；连带修复 parseScope 深前视门控
- 2026-10-27 字节码 M3：Cmp 族直写 _zf + 转译器前瞻融合 + Inc/Dec 单指令 + BNOT/NUM/OCTET，128 操作码转译覆盖闭环；连带修复 Reader OCTET 段 pos 推进与 dst=0 丢弃语义
- 2026-10-27 字节码 M4：CallOmit/CallExpand/CollapseArgs 三条新 TAC + 超额实参保留区 + 整数成员名全族 + CLT/CGT 序修正 + EVAL/EEXP→EvalStr
- 2026-10-27 字节码 M2：ctClass 加载期解释 + superClassGetter→extends + NEW→Call + GETP/SETP + DELD/TYPEOFD + proxy 回落链；修复 JF/JNF 物化方向颠倒等 M1 遗留
- 2026-10-27 字节码 M1.5：octet 常量 + 二目组/INC·DEC 的 PD/PI 读-算-写模板 + scratch 扩容
- 2026-10-26 字节码 M1 与转译器：TjsTranslator 恒等平移 + 标志物化 %F/%T/%N + 属性族两套操作数序 + CALL/SRV/RET 契约 + INTER_OBJECT 两阶段回填；`cial --tjsbc` 入口接通，s1/s4/s5 对拍一致
- 2026-10-26 后缀 `!` 即时求值（EvalStr 指令，源码语言特性收官，README 语法表清零）；指令计数勘误
- 2026-10-26 Array/Dictionary 结构化存取 + Octet pack 系（SerializeLib：二进制序列化 tTJSBinarySerializer 对齐 + (const) 文本解析器 + pack/unpack 全类型）
- 2026-10-26 RegExp 收尾：`RegExp.last` 静态属性 + 匹配位置改 UTF-8 码点语义
- 2026-10-26 Date.parse 完整文法（tjsdate.y 全 11 形态，DateParser 手写递归下降，对拍逐位一致）
- 2026-10-25 查证定论批次：with-try（原版设计现状，行为锁定测试补齐）、(const) 内联形态（原版无只读共享语义）、const 语义（tjs.y:319-321 官方注释定稿依据）
- 2026-10-25 Math.RandomGenerator（MT19937 与原版逐位一致）+ Date 标准库全量 + 默认构造批次 + CallV 变长实参调用指令
- 2026-10-25 脚本回调机制（VM::callValue 底座 + Array.sort + RegExp.replace 回调）；Rlc 行号接线；原生类构造（new 透明调用）；性能小组合批（TypeId 专用值消 dynamic_cast、makeSingle 单块分配）；Math + 数字字面量五处修复
- 2026-10-24 热路径优化（GProp 消双重 intern、wkAtoms 构造期驻留、String+String 快路径）；RegExp 完整 API + String SSO 21 字符边界修复；RegExp 字面量与 String.replace/split
- 2026-10-24 with 语句批次（WithPush/WithPop/GetWith + 裸词链）；Array/Dictionary 容器批次（字面量/下标/方法表）；property 引用批次（`&`/`*`、成员后缀 `++`）
- 2026-10-24 兼容性批次：前缀 `++ -- # $ delete`、const 声明、instanceof、Exception 内建类（TJS 源自举）、原生异常转脚本可捕获、默认参数 TestVoid 守卫等 17 项
- 2026-10-22 语法批次（`^ | &` 及复合赋值、后缀 if、typeof、(int) 转型、octet 字面量、函数提升、catch()）+ 二轮热点 + 崩溃修复（generateFuncMeta、未捕获异常静默 terminate、异常后状态残留）
- 2026-10-21 run0 扁平字节码唯一路径 + GC 根体系（MarkRootProvider + 6 处标记缺陷）+ Value SharedPayload；bench/ 对拍与 CMake 化基准
- 2026-10 上旬：Ret 惰性 Err、整数快路径、for(var)/增减运算符、Atom 跨 Runtime 缓存、FastRegisterPool `_sp`
