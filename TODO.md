# 待办

只记录尚未完成的工作；完成后删除条目。使用限制见 [FAQ](doc/faq.md)，历史测量见[性能记录](doc/performance.md)。

## 性能与优化

- [ ] 将对象图 GC 标记从 C++ 递归改为显式工作队列，消除深图对宿主线程栈的依赖；当前 Windows 深数组压力测试使用显式栈预算，见 [FAQ](doc/faq.md#windows-ci-的深数组压力测试为什么配置较大栈)。

- [ ] 定位语义/SDK 对齐批次的 GCC 全局整数循环回退（同机 Release 约 +10%，生成 TAC 一致）；正式发布前复核，见[测量](doc/performance.md#2026-11-01-语义对齐与-sdk-基础批次)。

- [ ] 复核结构化异常栈/getter 隔离批次的 Windows Release 类压力回退（2 万次实例化，同机两次约 +8%，尚未归因），见[测量](doc/performance.md#2026-11-01-结构化异常栈与-getter-异常隔离)。

- [ ] 评估 stdlib 内部临时值拷贝消除；继续采用同机 Release 对照及异常/GC 验证。已有调用布局、Value 生命周期与 asInteger 快路径结论见[性能记录](doc/performance.md)。

## 字节码反编译器

设计与批次见[反编译器方案](doc/bytecode/decompiler-plan.md)（2026-11-01 采纳 TAC 层直译路径）。

- [ ] M5：switch 链（JmpE + `_zf` 直写比较族）、try/catch 嵌套兜底（任意 CFG 降级
  状态机）、with、eval 名字依赖；按方案 §7 四环验证逐批验收。M3 遗留小项：
  顶层 `for` 初始化变量（寄存器携带无名字，如实拒绝）、`while` 条件内联存储歧义拒绝。
- [ ] M4→M5 移交的 bc 路径验收扩展：super 构造直调 context 的实参搬运 prologue
  （读未定义寄存器形态）、`CallMethodV`/`testDeref` 形态逐个落地后按方案 §7 四环
  对拍（语料见 `tests/test_files/tjsbc/debug/manifest.json` pendingUnits）。
- [ ] 顶层 getter+调用混合链的临时值跨脏区间需物化而顶层无局部作用域
  （`o.a + o.b + o.m()` 形态），物化策略待定（合成局部名有 §3 动态名查找风险）。

## 语言兼容缺口（待评估）

按[兼容定位](README.md)逐条评估，不为对齐而修复；确认要修再排批次。

- [ ] `throw` 后接二目表达式解析失败（`throw "W" + x;` 报 "except a return value of
  expression"；`throw "W";` 合法），TJS2 语法 throw 取表达式。
- [ ] 嵌套函数自由变量解析失败（函数体内 `function inner(){ return a + 2; }` 引用
  外层局部 `a` 运行期报 "in global object Not found property"），TJS2 嵌套作用域链
  未实现；反编译侧已可恢复该形态，执行语义待补。

M4 已完成（2026-11-04）：壳组装（2026-11-03 落地）+ 收尾项全通——全局/`*p` 目标
`+=`/`++` 逆融合（`sameObjectIdentity` 补匿名拷贝源头解析，读写两侧各持 `g_local`
拷贝的模板可融合；`isRegisterOperand2` 漏列二元算术族致 op2 使用漏计的守卫 bug
修复）；property/class/function 安装名登记 `_declaredGlobals`（顶层 property 后
同名赋值不再带多余 `var`）；IRGen 类成员按源码位置排序注册（原 AST 三向量分组
注册使方法前移，成员枚举序对齐 TJS2）；实参位置匿名函数字面量恢复（`FuncLit`
节点渲染期经 DeclHost 装配，恒匿名渲染）；函数体内零拷贝嵌套声明修复
（`scanDeclarations` 非安装指令误折叠吞掉后续语句）；声明文本统一相对缩进；
bc 行表语句区间按 origin 条目分段（修 `test_decompile_outputdebug_source_lines`
M4 结构变化后的期望）。回归：`cli_test.py` M4 复合赋值/解引用/字面量 round-trip
新增用例，CTest 431 全绿。

M0 已完成（2026-11-01）：`src/decompile/` 的 `{文本, 行表}` 管道、CLI `--decompile`（严格模式
默认 / `--view` 反汇编视图 / `--map` 行表 JSON / `--source` 偏移换算）、TJS2 路径来源表
（`TacOrigin`，码字坐标 + 可选源偏移）随转译器透传；两条入口反汇编级端到端，CTest 237 全绿。

M1 已完成（2026-11-01）：参考编译器新增 `--dump-debug` 导出 outputdebug 源位置表；
`--source` 行号换算以真实产物端到端验证（多行函数/类/继承/UTF-8 中文注释四类样本，
覆盖 TJS2 仅在控制流点登记位置、条目间继承行号的粒度），语料与固定清单（参考二进制
与样本哈希、导出选项）见 `tests/test_files/tjsbc/debug/manifest.json`，断言入
`tests/cli_test.py`；全量 CTest 通过。

M2 已完成（2026-11-02）：`src/decompile/ExprBuilder.*` 单块直线表达式恢复——寄存器版本链
+ 使用点、后优化形态 tile（BinaryImm/CompareTestImm 逆映射、`~x` 降糖逆映射、字面量 SetIndex
折叠、折叠常量容器重建、Push+Call 归并、具名局部零拷贝声明还原）、干净区间内联守卫（同树
内联保持求值序，函数体多使用物化为临时变量，顶层纯读复制/非纯拒绝）、TJS2 优先级括号。
严格模式对直线顶层程序产出可重编译文本（环① round-trip 对拍 9 类形态通过，`tests/cli_test.py`），
结构/声明按批次号拒绝；视图模式以 `// >>` 附加恢复结果。顺带修复 `FuncMeta::localVars.startPC`
为优化前 pc 的错位（`Chunk::pcAnchors` 随压缩重映射，调试器局部作用域同样受益）。

M3 已完成（2026-11-03）：`ExprBuilder` 语句结构化——CFG 块 + 条件区（TestVoid? Test?
CompareTest(+Imm)? JmpNE，优化器融合保证相邻）、if/else 区域归约、while/do-while/
while(true)（锁存判别：锁存目标到条件区含存储即 do-while，条件内联存储歧义如实拒绝）、
break/continue（for 与 while+尾部递增 TAC 不可区分，按 while 形态恢复，行为等价）、
三元跨块折叠（两分支单条纯叶定义 + d_local 拷入同一 dst；命名/匿名 dst 分别合并/绑版本）、
跳转求值时机屏障（跨分支内联守卫）、`Chunk::pcAnchors` 扩展重映射 endPC、`localNameAt`
按 startPC ≤ pc < endPC 作用域查名（修 stale 名外溢）。函数体循环 round-trip 对拍
（while+continue / do-while+break / for+三元 / else-if / while(true)+break+return，
逐输入执行比对返回值）与顶层 while/do-while CLI 对拍通过；switch → M5、声明 → M4、
with → M3 遗留，按批次号拒绝。

## SDK 与开发工具路线图

- [ ] 完成诊断重构批次 5 剩余的 JIT 等价位置验证（继续扩展原生互调与异常恢复组合）。故障位置、源码快照、结构化异常栈与栈降级已落地，验收见[诊断重构计划](doc/diagnostics-plan.md)实施状态一节。

设计边界、依赖与验收条件见[路线图](doc/roadmap.md)；以下均为未完成项。

- [ ] 批次 1：收敛公共 SDK 兼容承诺与能力查询，再拆分核心与开发工具仓库并建立独立发行。install/export、Engine 入口、逻辑源码身份、安装后示例与三平台远端安装验收已完成。
- [ ] 批次 2：实现 API 元数据与 `.d.tjs` 声明模式，连接标准库/宿主导出与 LSP，逐步统一核心前端分析；执行器和脚本扫描须正确区分声明文件。
- [ ] 批次 3：实现模块作用域、显式导出、宿主解析、Context 缓存、循环诊断及异常/GC 恢复，保留 `Scripts` 兼容入口；稳定后再讨论 `import/export` 语法。
- [ ] 批次 4：提供核心脚本调试接口、DAP 适配器及 VSCode 端到端调试，首先覆盖行断点、单步、调用栈、变量和异常暂停。

## krkr2 接入

目标为 [2468785842/krkr2](https://github.com/2468785842/krkr2)，接入位置与验收门槛见[接入说明](doc/krkr2-integration.md)。

- [ ] 实现双向 `iTJSDispatch2` / VariantClosure 桥：对象身份、AddRef/Release 与 GC 根、flags、Operation、枚举、NativeInstanceSupport、ClassInstanceInfo、无效化和异常转换。
- [ ] 补齐字节码 proxy 的 direct/indirect 回落、缺失成员 typeof、一次 Operation 派发与动态类体/父类表达式。
- [ ] 对接 TVP 文本/二进制读写流；Scripts 文本读取器已有入口，Array/Dictionary 保存与加载仍直接使用文件系统。接入 XP3、模式串、字节码探测与源码身份映射。
- [ ] 在目标仓库增加可切换后端并验证 startup/Initialize、KAG、Window/Layer/Timer 回调、插件、存档和退出回收；桌面通过后验证 Android ARM64/x86_64。
- [ ] 补齐其余 Unicode 边界（sprintf 的字符/宽度/精度、大小写及文本存取），建立目标 TJS2 与真实游戏语料的持续差分验收。
