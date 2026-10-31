# TJS2 字节码到 Ciallang 的转译

TJS2 文件经 Reader 解析为常量池和 context，再由 Translator 生成 Ciallang TAC，
交给同一套 Chunk、Value、GC 与 VM 执行。本文描述当前实现和已知差异，不再保留实施前提案。

依据：`src/tjsbc/TjsBytecodeReader.*`、`TjsTranslator.*`；格式见
[tjs2-bytecode.md](tjs2-bytecode.md)，目标指令见 [ciallang-bytecode.md](ciallang-bytecode.md)。

## 1. 总体架构

```text
TJS2 100\0 文件
  → TjsBytecodeReader：常量池、context、索引形式的 i16 码字
  → TjsTranslator：元数据骨架、解码与 TAC 发射、跳转和 context 引用回填
  → Chunk::toHeap：保留常量图及元数据
  → VMState::run：惰性编码后执行
```

同一 context 内解码与发射在一次扫描中完成，保留码字位置到 TAC 下标的映射，
扫描结束后回填跳转。跨 context 引用则在全部 context 就绪后装配。
不要将这两个阶段与“先计算每条指令会展开多少 TAC”的旧设计混淆。

## 2. 寄存器空间映射

令 `K = max(maxVariableCount + variableReserveCount, funcDeclArgCount + 2)`，
普通 TJS 寄存器 `r` 映射为 Ciallang `%r+K`。窗口还分配标志物化、临时值、
实参备份和丢弃返回值的专用槽，具体布局以 Translator 的 prologue 为准。

| TJS 槽位 | 初始化或用途 |
|---|---|
| `ra[-2]` | 装入 this；部分 direct 成员指令专门转为 this→global 查找，差异见 §8 |
| `ra[-1]` | 装入 this |
| `ra[0]` | 装入 void；不能用作调用结果的丢弃槽 |
| `ra[-3-i]` | 第 i 个实参；先批量备份，再从 `%i` 搬入，避免槽位置换互相覆盖 |
| 临时区 | 普通 TJS 正向寄存器及转译专用 scratch |

`funcDeclCollapseBase >= 0` 时在 prologue 发射 `CollapseArgs`，把对应位置之后的
实际实参收拢成 Array。超额实参另由调用帧保留，以供 `CallOmit` / `CallExpand` 使用。
缺省形参补 void，默认参数的求值逻辑保留在原字节码中。

## 3. 标志与控制流

不相邻的 flag 生产者和消费者通过专用寄存器 `%F` 物化；相邻比较/TT/TF 与 JF/JNF
则前瞻融合为直接写 `_zf` 的指令和跳转，减少中间值。

| TJS | 转译规则 |
|---|---|
| CEQ / CDEQ | `EQ` / `AbsEQ` 写 `%F`；融合路径用 `CmpEQ` / `CmpDEQ` |
| CLT / CGT | 自然序 `<` / `>`；融合路径用 `CmpLT` / `CmpGT` |
| TT / TF | 布尔化 / 取反写 `%F`；融合路径直接 `Test` 并按需反转跳转方向 |
| SETF / SETNF / NF | 复制或逻辑取反物化值 |
| JF / JNF | 真跳 / 假跳，对应 `Test %F` 后的 `JmpE` / `JmpNE` |
| JMP | `Jmp` |

参考实现的 `GreaterThan()` 实际返回 `<`，`LittlerThan()` 实际返回 `>`；
必须结合 `tjsVariant.cpp` 的实现理解 CLT/CGT，不能只按函数名换序。
跳转目标按“指令首码字偏移 + 相对码字偏移”计算，并检查范围与指令边界。

## 4. 主要操作码映射

| TJS | Ciallang |
|---|---|
| CONST / CP / CL / CCL | `Load` 或 `LoadImm` / `DLocal` / 装入 void / 展开清空 |
| 算术与位运算基型 | 对应三操作数指令，结果写回原目标寄存器 |
| INC / DEC | `Inc` / `Dec`；成员变体使用读、加减、写模板 |
| GPD / SPD | `GPropC` / `DPropC`，成员名 Atom 直引 |
| GPI / SPI | `GProp` / `DProp`，成员名由寄存器提供 |
| GPDS / SPDS / GPIS / SPIS | 对应 `GPropRawC` / `DPropRawC` / `GPropRaw` / `DPropRaw`，绕过属性处理器 |
| GETP / SETP | `DerefGet` / `DerefSet` |
| DELD / DELI | `DelPropC` / `DelProp` |
| INT / REAL / STR / OCTET | `ToInt` / `ToReal` / `ToString` / `ToOctet` |
| BNOT / NUM / CHS / LNOT | `BNot` / `ToNumber` / `ChgSign` / `LNot` |
| ASC / CHR | `Asc`（`#`）/ `Chr`（`$`） |
| TYPEOF | `TypeOf`；成员形态的缺失成员行为见 §8 |
| INV / CHKINV | `Inv` / `ChkInv` |
| CHKINS | `ChkIns target, className`，结果原地写回 target |
| EVAL / EEXP | `EvalStr`；非字符串或空字符串的现有处理由 VM 决定 |
| CALL / CALLD / CALLI / NEW | 普通实参 Push 后 `Call`；成员调用先读函数，类值由 `ClassObject::call` 构造实例 |
| argcount=-1 / -2 | `CallOmit` / `CallExpand`，展开项为普通值、Array 展开或匿名实参展开 |
| SRV / RET | SRV 记录返回寄存器，RET 发射 `Ret`；没有返回值时使用 void 槽 |
| ENTRY / EXTRY / THROW | 静态 try 表 / 无独立执行指令 / `Throw` |
| CHGTHIS | `ChgThis %function, %thisValue`，操作数顺序与源指令一致 |
| GLOBAL | `Global` |
| ADDCI / REGMEMBER | 只在支持的类构造模式中加载期解释；普通函数 context 遇到时拒绝 |
| NOP / DEBUGGER | 转译扫描中不发射执行指令 |

调用结果目标为 TJS 寄存器 0 时，转译到独立丢弃槽，避免污染 `ra[0]` 的 void 语义。
包含解码分支不等于全部语义已经对齐，尤其不能据此宣称“128 条都完全兼容”。

## 5. 常量池与 context 引用

| TJS 条目 | 目标表示 |
|---|---|
| VOID / null OBJECT | 常量 0 的 void；null 与 void 的区别未完整保留 |
| BYTE / SHORT / INTEGER / LONG | 归一为 i64，在 CONST 发射点使用 `LoadImm` |
| REAL | Real 常量，保留 double 位模式 |
| STRING | UTF-16LE 转 UTF-8 后驻留为 Atom，处理代理对 |
| OCTET | 共享 Octet 负载常量 |
| INTER_OBJECT | 每处保留独立占位槽，全部 context 完成后回填 FuncMeta/ClassMeta/PropMeta |

空占位槽不能经常量去重合并，否则回填会相互覆盖。移入 GC 堆时使用 `Chunk::toHeap()`
保根；Translator 还持有转译产物的根，见 `TjsTranslator.hpp/.cpp`。

## 6. 属性模板与异常表

以 direct 二目成员运算为例：

```text
GPropC    %object, atom, %temp
Add       %temp, %operand, %temp
DPropC    %object, atom, %temp
DLocal    %temp, %result          ; 只有源结果目标非 0 才写回
```

间接成员名使用寄存器版本；闭包属性 P 变体使用 `DerefGet/DerefSet`。
这与参考引擎一次 `Operation()` 分派不同，不能视为完整的原子属性操作。
SPDE/SPIE 的创建成员语义依赖目标对象的 setProp，HIDDENMEMBER 等标志未有独立表示。

ENTRY 登记异常接收寄存器、try 起点和 catch 目标；EXTRY 不生成独立执行指令。
回填后的 TAC try 区间在 `Chunk::toBytecode()` 中转换为字节码偏移，unwind 使用该表。

## 7. 类与属性元数据

普通函数、表达式函数、getter 和 setter 生成函数元数据；ctProperty 汇总 getter/setter。
ctClass 的受支持构造模式在加载期解释，填充成员、构造入口、默认字段初始化和父类名。
`superClassGetter` 只支持可识别的父类引用模式，不支持任意动态父类表达式。
无法识别的类构造模式报错，当前没有通用运行期 REGMEMBER/ADDCI 回退后端。

## 8. 已知差异与待修复项

| 项目 | 当前边界 |
|---|---|
| proxy `ra[-2]` | 部分 direct 访问译为 GThis/GRefRaw/DThis；间接 GPI/CALLI 等只对映射的 this 值访问，没有完整的 this→global 回落 |
| SPDS 写 proxy | 写 this 槽；this 为空时未完整回落 global |
| TYPEOFD/TYPEOFI | 目前由属性读取后 TypeOf 实现；成员缺失会抛错，未返回参考实现的 `"undefined"` |
| 属性读改写 | 多条 TAC 不提供一次 Operation 派发的完整语义 |
| 动态类体与父类表达式 | 只接受加载期识别的模式；不是通用类体解释器 |
| null / void | 常量池 null OBJECT 合并到 void，不能宣称完全等价 |
| 输入校验 | 已检查截断指令、非法调用计数、类引用和跳转边界；不代表所有寄存器或元数据都已全面验证 |

## 9. 对拍与工具链

入库 `.tjsbc` 位于 `tests/test_files/tjsbc/`，共 7 个，均有对应源码：s6_class、s7_ext、
test_class、test_function、test_misc、test_variant、test_with。源码先在同目录寻找，
再到 `tests/test_files/tjs2/` 找同名文件；CI 不需要参考引擎。

```sh
python bench/bytecode_conformance.py --cial build/Linux/Debug/cial \
  --report bench/results/bytecode-conformance.json
python bench/bytecode_conformance.py --cial build/Linux/Debug/cial \
  --case s7_ext --case test_class
```

工具要求源码路径和字节码路径都以 0 退出，并逐字节比较 stdout；不剥离引号、时间戳、空行或诊断。
缺源码为 `TOOL_ERROR`；非 `MATCH` 返回 1，参数错误返回 2。每个子进程默认 15 秒超时，
可由 `--timeout` 设置。JSON 保留输入/执行器 SHA-256、返回码、首个差异位置及无损输出。
`test_misc` fixture 的异常消息格式与行号差异仍按 `OUTPUT_DIFF` 报告，不能把该命令默认当作全绿。

参考导出可选：

```sh
python bench/bytecode_conformance.py --cial build/Linux/Debug/cial \
  --reference /path/to/TJSCompiler --case s7_ext \
  --report bench/results/bytecode-fresh-export.json
```

该模式先用两个随机标记脚本检查路径参数，再验证 `--dump <script> <output>` 产生的
`TJS2 100\0` 文件能被 Ciallang 执行。探测失败即停止；通过后只在临时目录导出，不覆盖 fixture。
TJSCompiler `d8044c3` 的 CLI 已支持这些参数，不应继续将其描述为“尚未提交的本机适配”。
这项检查验证参考导出到转译执行的链路，不替代三方语义或全部脚本覆盖。

回归见 `tests/tjsbc_reader_test.cpp`、`tests/tjsbc_translator_test.cpp`、
`tests/cli_test.py` 与 `bench/test_bytecode_conformance.py`。
设置 `TJS_REFERENCE` 可启用工具测试的参考引擎真实子进程部分。
