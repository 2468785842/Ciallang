# 声明文件 `.d.tjs`

`.d.tjs` 描述 Ciallang 宿主与标准库对脚本可见的接口（函数、参数、返回值、类、属性、
继承、文档及可选/变长参数），不包含实现，也不等同于 C++ 头文件。扩展名固定为
`.d.tjs`（[路线图](roadmap.md) §3.2）。核心不变量：

- **声明文件永远不被当作脚本执行**。CLI 执行模式、`Scripts.execStorage/evalStorage`、
  SDK `Engine::execute` 与未来模块加载器都以源码身份（名字以 `.d.tjs` 结尾，大小写
  不敏感）识别并拒绝，诊断码 `decl.not-executable`。
- 声明语法基于 TJS2 自身文法（`doc/grammar/tjs.y` 的 `variable_type`、`func_def`、
  `property_def`、`class_def`），不照搬 TypeScript：每个定义去掉函数体/访问器体，
  以 `;` 收尾；类型注解即 TJS2 原有的 `: type` 记法。
- 声明解析与生成共享 [API 元数据模型](api-metadata.md)所属的符号体系：
  `cial --api-decl`（注册事实 → 声明文本）与声明解析器（声明文本 → 符号模型）互为
  逆操作，往返保持稳定。LSP 消费声明属路线图批次 C，不在本篇范围。

## 1. 语法

```tjs
/**
 * 标准库数组。文档注释取紧邻声明上方的块注释（/** 开头），
 * 多行合并为一条 doc。
 */
class Array extends Object {
    function join(*);                          // 变长：未命名收拢参数（注册点无签名信息）
    function push(value: any): Integer;        // 参数/返回类型注解
    function indexOf(start, value: any): Integer; // 无注解参数 = 类型未知
    function fill(value: any, start: Integer = 0): void; // 字面量默认值 = 可选参数
    property count { getter: Integer; setter(value: Integer); } // 显式访问器
    property length: Integer;                  // 访问方式未指定（简写）
    var buffer: Array;                         // 成员变量（const 同形，不带初始化器）
    static function from(iterable: any): Array; // 类静态函数
    function Array();                          // 构造器：与类同名的成员函数（TJS2 惯例）
}

namespace Nested {                             // 宿主 Module（命名空间对象）
    function helper(): void;
    class Widget extends Base { function draw(): void; } // 嵌套：身份为点分路径 Nested.Widget
}

function print(value: any): void;              // 顶层全局函数
var enabled: Boolean;                          // 顶层全局变量
```

文法（`EBNF` 摘要，词法与 TJS2 脚本一致，含预处理行与注释）：

```
decl_file    := { decl_item }
decl_item    := doc_comment? ( namespace_decl | class_decl | function_decl
                             | property_decl | variable_decl | ";" )
class_decl   := "class" SYMBOL [ "extends" SYMBOL { "," SYMBOL } ] "{" { decl_item } "}"
namespace_decl := "namespace" SYMBOL "{" { decl_item } "}"
function_decl  := [ "static" ] "function" SYMBOL [ "(" params ")" ] [ variable_type ] ";"
params       := param { "," param } [ "," ( collapse | "*" ) ] | collapse | "*"
param        := SYMBOL [ variable_type ] [ "=" literal ] [ "*" ]   // 带 "*" 为命名收拢
collapse     := SYMBOL [ variable_type ] "*"
property_decl := "property" SYMBOL ( variable_type ";" | "{" { handler } "}" )
handler      := "getter" [ "(" ")" ] [ variable_type ] ";"
              | "setter" "(" SYMBOL [ variable_type ] ")" ";"
variable_decl := ( "var" | "const" ) SYMBOL [ variable_type ] ";"
variable_type := [ ":" ( SYMBOL | "void" | "int" | "real" | "string" | "octet" ) ]
literal      := 数字/字符串/true/false/null/void 字面量，可带一元负号
```

规则与约束（解析实现在 `src/parser/DeclParser.cpp`，经 `syntax::Parser` 的
`parseDeclarations` 入口）：

- **类型规范化**：关键字 `int`/`real`/`string`/`octet` 解析为模型名
  `Integer`/`Real`/`String`/`Octet`（与 `--api-json` 一致）；`void` 保持 `void`；
  标识符按原文保留（可为内置类名或宿主类型）。省略注解 = 类型未知。
- **可选与变长**：带字面量默认值的参数为可选；`*`/`name*` 收拢参数表达变长，
  必须是参数表最后一项（与 tjs.y 相同）。默认值只接受字面量——声明文件不描述
  计算；其余表达式按 `decl.invalid-default` 诊断。
- **无初始化器**：`var`/`const` 带初始化器按 `decl.initializer` 诊断；声明只描述
  接口，不描述值。
- **static 仅类成员函数可用**；命名空间或顶层使用按 `decl.static-outside-class` 诊断。
- **namespace 不得出现在类体**（`decl.invalid-nesting`）；类体内允许嵌套类
  （身份为点分路径，与 tjs.y 的 class_def 嵌套一致）。
- 除上述声明与空语句外，其余语句/表达式按 `decl.not-a-declaration` 诊断；
  文法错误使用 `decl.expected-token` 等通用码。所有诊断码以 `decl.` 为前缀，
  与脚本编译诊断（`parse.*`/`lex.*`/`gen.*`）可区分。
- 词法/预处理层错误（`lex.*` 等）原样透传，与脚本一致。

## 2. 数据模型与转换

模型定义在 `src/decl/DeclModel.hpp`（纯 C++ 所有权，不涉及 GC）：

| 结构 | 字段 |
|---|---|
| `decl::Param` | `name`（空 = 未命名收拢）、`type`（空 = 未知）、`optional`、`collapse`、`defaultText` |
| `decl::Symbol` | `kind`、`name`、`doc`、`params`、`ret`、`hasGetter`/`hasSetter`、`constant`（Variable）、`extends`、`members`、`span` |
| `decl::Module` | 顶层 `symbols` |

`SymbolKind`：`Namespace` / `Class` / `Function` / `StaticFunction` / `Property` /
`Variable`。属性的读写性由 `hasGetter`/`hasSetter` 表达：两者皆假 = 访问方式未指定
（简写形式）。

入口（`src/decl/DeclModel.hpp` 声明，`src/decl/DeclModel.cpp` 实现）：

| 函数 | 作用 |
|---|---|
| `decl::parseDeclarations(SourceFile &, Result &, PreProcessor &)` | 声明文本 → 模型；诊断写入 Result |
| `decl::generateDeclarations(const Module &)` | 模型 → 声明文本（2 空格缩进） |
| `decl::declarationsFromCatalog(const ApiCatalog &)` | 注册事实（[api-metadata](api-metadata.md)）→ 模型 |

`declarationsFromCatalog` 规则：`typeMembers` + `typeNames` 生成 `class` 符号
（按注册名排序保证确定性）；`globals` 按 `owner` 点分路径归位为嵌套
`namespace`/`class`（globals 按登记顺序，先父后子）；`variadic` 条目转换为单个
未命名收拢参数；无法推导的类型保持未知；登记属性转换为简写形式（登记点不含
读写性事实——实测 getter-only 登记在语言层未必只读，如 `Array.count`，见
`src/stdlib/NativeRegister.hpp` 的 `NativePropGetter` 与 VM 的属性特判）。

## 3. 生成器

`cial --api-decl`（无输入文件，与 `--api-json` 同类退出模式）以标准会话注册链
导出标准库与内建宿主接口的声明文本。输出规则：

- 类按注册名排序；成员保持登记顺序；文档注释以 `/** */` 输出。
- 参数名不可从 C++ 类型推导，生成 `arg0`、`arg1`……占位名（模型有名时用原名）。
- 变长条目输出 `function name(*);`；未知类型省略注解。
- 属性一律输出简写 `property name: type;`（读写性未知，不声称只读/可写）。
- 输出仅由模型决定，不含版本戳或头部注释；对同一注册事实输出逐字节稳定，
  且 `generate(parse(generate(catalog)))` 与 `generate(catalog)` 一致（往返幂等，
  由 `tests/declaration_test.cpp` 验证）。

宿主应用可在注册后调用 `decl::declarationsFromCatalog(context.apiCatalog())` +
`decl::generateDeclarations` 随 SDK/应用发布自己的 `.d.tjs`。

## 4. CLI 行为与执行侧拒绝

| 用法 | 行为 |
|---|---|
| `cial --api-decl` | 输出标准库声明文本，退出码 0；不接受输入与执行选项 |
| `cial --check-json foo.d.tjs` | 按**声明模式**分析，输出 JSON 诊断（`decl.*` 码）；无错误退出 0 |
| `cial foo.d.tjs`（执行模式） | 拒绝：`decl.not-executable`，退出码 1 |
| `cial --dump-cfg/--dump-ssa/--decompile foo.d.tjs` | 同上拒绝（声明无可执行代码） |
| `cial --check-json - --source-name x.d.tjs` | stdin + 逻辑名识别为声明（LSP 路径） |
| `Scripts.execStorage("x.d.tjs")` / `evalStorage` | 运行期异常（可被脚本 catch），消息含声明文件说明 |
| SDK `Engine::execute(text, r, "x.d.tjs")` | 仅写 Result 诊断（`decl.not-executable`），不编译 |

拒绝检查集中在两处：`cli::compile`（`src/cli/Compilation.cpp`，覆盖 CLI 各模式与
cial-dbg/cial-disasm 工具）与 `VM::execute`（`src/vm/VM.cpp`，覆盖 `Scripts` 与
`Engine`）。身份判定 `Common::isDeclarationSourceName` 在 `src/common/SourceFile.hpp`。

## 5. 测试与扫描排除

声明解析另设测试，不进入脚本测试扫描；以下位置显式排除 `.d.tjs`：

- `tests/CMakeLists.txt` 的 `tjs/*.tjs` GLOB（`list(FILTER ... EXCLUDE REGEX "\\.d\\.tjs$")`）；
- `tests/run_tjs_tests.py` 的 `rglob("*.tjs")`；
- `cmake/Bench.cmake`、`cmake/Conformance.cmake` 的基准/语料 GLOB；
- `fuzz/run_fuzz.sh` 的种子列表。

声明样例与测试：`tests/declaration_test.cpp`（解析、生成、目录转换、往返与拒绝，
Catch2）与 `tests/cli/test_cli.py`（CLI 行为）。声明语料放 `tests/decl/`（不在
`tjs/` 扫描范围）。

## 6. 现状与限制

- 属性读写性、参数名、默认值、条件变长在注册点不可推导，生成器如实省略或
  占位（`argN`），不编造；注册点可按 [api-metadata](api-metadata.md) 以 `doc`
  尾参/显式 `ApiMeta` 补充。
- 宿主构造器（`ClassBuilder::constructor`）不登记成员元数据，生成的类无构造器
  条目；手写声明可用与类同名的成员函数表达（TJS2 惯例）。
- LSP 消费声明（替换 `tools/lsp/cial_lsp.py` 的 `BUILTIN_MEMBERS`）、编辑器
  `.d.tjs` 关联、声明/实现一致性检查、同名声明的查找优先级与冲突诊断属路线图
  批次 C 及后续设计；本阶段不引入可被脚本扫描器误收集的声明样本。
- `namespace` 是声明模式中的上下文关键字（后随标识符 + `{` 才成立），TJS2 保留字
  表不含它，脚本模式行为不变。
