# 声明文件 `.d.tjs`

`.d.tjs` 描述 Ciallang 宿主与标准库对脚本可见的接口（函数、参数、返回值、类、属性、
继承、文档及可选/变长参数），不包含实现，也不等同于 C++ 头文件。扩展名固定为
`.d.tjs`（[路线图](roadmap.md) §3.2）。核心不变量：

- **声明文件永远不被当作脚本执行**。CLI 执行模式、`Scripts.execStorage/evalStorage`、
  SDK `Engine::execute` 与未来模块加载器都以源码身份（名字以 `.d.tjs` 结尾，大小写
  不敏感）识别并拒绝，诊断码 `decl.not-executable`。
- 声明语法基于 TJS2 自身文法（`doc/grammar/tjs.y` 的 `variable_type`、`func_def`、
  `property_def`、`class_def`），不照搬 TypeScript：每个定义去掉函数体/访问器体，
  以 `;` 收尾；类型注解沿用 `: type`，声明模式扩展支持限定名、联合类型和数组元素类型；脚本执行文法不变。
- 声明解析与生成共享 [API 元数据模型](api-metadata.md)所属的符号体系：
  `cial --api-decl`（注册事实 → 声明文本）与声明解析器（声明文本 → 符号模型）互为
  逆操作，往返保持稳定。[LSP](lsp.md) 通过核心声明分析读取相同模型。

## 1. 语法

```tjs
/** 动态数组；元素可以是不同的脚本值。 */
class Array {
    /**
     * 向末尾追加元素。
     * @param values 要追加的脚本值，允许混合类型。
     * @returns 修改后的数组长度。
     * @example var values = []; values.push(1, "hello");
     */
    function push(values: any*): Integer;
    function join(delim: String = ","): String;
    function sort(order: String | Function = '+', stable: Boolean = false): void;
    property count { getter: Integer; setter(value: Integer); }
}

/** 工程宿主 API 按命名空间组织，避免污染全局。 */
namespace Assets {
    class Image { property width { getter: Integer; } }
    /**
     * 加载一组图像。
     * @param filenames 资源路径数组。
     * @returns 图像资源数组。
     * @throws 文件读取或解码异常。
     */
    function loadImages(filenames: String[]): Assets.Image[];
}
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
variable_type := [ ":" type_expr ]
type_expr     := named_type { "|" named_type }
named_type    := ( SYMBOL | "void" | "int" | "real" | "string" | "octet" ) { "." SYMBOL } { "[" "]" }
literal      := 数字/字符串/true/false/null/void 字面量，可带一元负号
```

规则与约束（解析实现在 `src/parser/DeclParser.cpp`，经 `syntax::Parser` 的
`parseDeclarations` 入口）：

- **类型规范化**：关键字 `int`/`real`/`string`/`octet` 解析为模型名
  `Integer`/`Real`/`String`/`Octet`（与 `--api-json` 一致）；`void` 保持 `void`；
  标识符按原文保留（可为内置类名或宿主类型）；支持 `Assets.Image`、`String | RegExp`、`String[]`。联合类型规范为带空格的 ` | `。省略注解 = 类型未知。
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
未命名收拢参数；无法推导的类型保持未知。显式参数与访问方式保留原描述；未知访问方式
使用属性简写。不能仅凭 getter 登记推断只读，如 `Array.count`，见
`src/stdlib/NativeRegister.hpp` 的 `NativePropGetter` 与 VM 的属性特判。

## 3. 生成器

`cial --api-decl`（无输入文件，与 `--api-json` 同类退出模式）以标准会话注册链
导出标准库与内建宿主接口的声明文本。输出规则：

- 类按注册名排序；成员保持登记顺序；文档注释以 `/** */` 输出。
- 参数名不可从 C++ 类型推导；注册点可显式提供，否则生成 `arg0`、`arg1`……占位名。
- 显式签名保留参数名、可选字面量和具名收拢参数；仅知变长时输出 `function name(*);`。
  未知类型省略注解。
- 属性在访问方式明确时输出 getter/setter，否则使用简写，避免误称只读/可写。
- 函数文档保留注册点行为说明，补齐可由签名确定的 `@param` 类型/默认值、`@returns`；
  文档格式使用 `/** */`，允许 `@throws`、`@example`，LSP 以 Markdown 悬停展示。
- 输出仅由模型决定，不含版本戳或头部注释；对同一注册事实输出逐字节稳定，
  且 `generate(parse(generate(catalog)))` 与 `generate(catalog)` 一致（往返幂等，
  由 `tests/declaration_test.cpp` 验证）。

宿主应用可在注册后调用 `decl::declarationsFromCatalog(context.apiCatalog())` +
`decl::generateDeclarations` 随 SDK/应用发布自己的 `.d.tjs`。

标准库声明在构建期落盘：`cial` 目标的 POST_BUILD 步骤运行 `--api-decl-dir`，把按顶层符号
拆分的声明（`Array.d.tjs`、`Math.d.tjs`……顶层自由函数归入 `globals.d.tjs`）写到可执行文件旁的
`declarations/`，并复制手写的 `Exception.d.tjs`（`cmake/GenerateApiDeclarations.cmake`）。
构建目录同时包含 `index.json`（schemaVersion、SDK 版本、各文件 path/SHA-256）与独立 README，
方便整体分发和核对接口包；清单由构建脚本生成，不作为可执行源码。
SDK 组件与 CLI 归档安装时在 `share/ciallang/declarations/` 发布该目录，脚本工具直接读取这些
声明文件获得接口文档与跳转，不需要查看 C++ 注册源码；安装/构建布局的读取顺序见
[LSP](lsp.md)。可选能力库按 CMake 开关门控注册：关闭的库不进注册目录，声明产物自然不含它。

## 4. CLI 行为与执行侧拒绝

| 用法 | 行为 |
|---|---|
| `cial --api-decl` | 输出标准库声明文本，退出码 0；不接受输入与执行选项 |
| `cial --api-decl-dir <dir>` | 与 `--api-decl` 同用：按顶层符号拆分写出 `.d.tjs` 到目录 |
| `cial --check-json foo.d.tjs` | 按**声明模式**分析，输出 JSON 诊断（`decl.*` 码）；无错误退出 0 |
| `cial foo.d.tjs`（执行模式） | 拒绝：`decl.not-executable`，退出码 1 |
| `cial --dump-cfg/--dump-ssa/--decompile foo.d.tjs` | 同上拒绝（声明无可执行代码） |
| `cial --check-json - --source-name x.d.tjs` | stdin + 逻辑名识别为声明（LSP 路径） |
| `cial --decl-json --source-name x.d.tjs -` | 按声明模式输出符号和诊断；不执行输入 |
| `Scripts.execStorage("x.d.tjs")` / `evalStorage` | 运行期异常（可被脚本 catch），消息含声明文件说明 |
| SDK `Engine::execute(text, r, "x.d.tjs")` | 仅写 Result 诊断（`decl.not-executable`），不编译 |

拒绝检查集中在两处：`cli::compile`（`src/cli/Compilation.cpp`，覆盖 CLI 各模式与
cial-dbg/cial-disasm 工具）与 `VM::execute`（`src/vm/VM.cpp`，覆盖 `Scripts` 与
`Engine`）。身份判定 `Common::isDeclarationSourceName` 在 `src/common/SourceFile.hpp`。

`--decl-json` 为工具传输接口：schemaVersion 1，symbols 为递归的声明模型，含
kind/name/doc/params/ret/extends/members、属性访问方式及声明名的 begin/end UTF-8 字节偏移；
check 为现有 schema 2 的 JSON 诊断。输入无效时退出 1，工具不得把残缺符号当作有效接口。
该选项显式选择声明解析，不依赖文件扩展名；支持文件与 stdin，拒绝执行选项。
客户端通过 stdin 发送原始 UTF-8 字节，不使用会自动转换换行的文本管道，以保持偏移与原文一致。

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

- 自动推导未知的事实仍如实省略；注册点可按 [api-metadata](api-metadata.md) 显式补充。
  ClassBuilder 已登记构造器；原生 registerClass 的任意动态调用协议不会被猜测为构造签名。
- LSP 已支持声明补全、悬停、继承与跳转；同名歧义保守放弃推断，具体优先级见 [LSP](lsp.md)。
  声明/实现一致性检查、重载选择和独立冲突诊断尚未实现。
- `namespace` 是声明模式中的上下文关键字（后随标识符 + `{` 才成立），TJS2 保留字
  表不含它，脚本模式行为不变。


## 7. 工程声明包规范

建议目录结构如下，运行代码与接口声明分开维护：

```text
project/
  src/                         # 运行脚本
  types/
    host/Assets.d.tjs           # 宿主命名空间及类型
    host/Widget.d.tjs
  .vscode/settings.json         # 外部 SDK 的 declarationFiles 配置（按需）
SDK/share/ciallang/declarations/
  README.md                    # 使用约定与工程示例
  index.json                   # 版本、文件清单与 SHA-256
  Array.d.tjs                  # 每个主要公共类型单独成文件
  Exception.d.tjs
  globals.d.tjs
```

按 TypeScript/Javadoc 常见的接口契约方式编写：摘要说明行为，参数说明单位、边界和默认值，
返回说明实际结果，异常说明触发条件，并提供能运行的示例。宿主类型用 namespace 归属，
跨命名空间引用使用限定名。属性明确 getter/setter，构造器沿用 TJS 的同名成员函数形式；
LSP 对类引用导航到 class，而不额外列出同名构造器。

优先使用真实具体类型；有限选择使用联合类型，已知数组元素类型使用 `T[]`。`Function`
表示可调用对象，回调的参数和返回协议写在文档中。`any` 仍用于 Array 的异构元素、脚本
求值结果和按原值保存的 Exception.message；`values: any*` 是具名变长实参，星号不是
未知类型占位。不能为减少 any 而把动态返回值假写成 String 或 Integer。

此版本没有实现泛型、类型别名、import/export、重载选择或 Java 式完整静态类型检查。
联合类型不会被猜测为唯一接收者；`String[]` 等数组返回类型可按 Array 查找容器成员。
宿主生成自己的接口包时，应通过 API 元数据登记具名参数和语义说明，保持注册事实为唯一来源；
不要修改构建生成的标准库文件。标准包更新时整体替换，避免混入不同版本。
