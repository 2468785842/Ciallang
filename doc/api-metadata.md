# API 元数据与导出

宿主与标准库在注册期自动记录符号与可确定签名，`cial --api-json` 导出为版本化 JSON；
这是[路线图](roadmap.md) §3.2 的第一步：先建立版本化 API 元数据模型，`.d.tjs` 声明
生成器、LSP 与文档生成共享该模型。数据模型与推导在 `src/types/ApiMeta.hpp`，
聚合目录 `ApiCatalog` 挂在 `NativeRegister`（`src/stdlib/NativeRegister.hpp`），
生命周期同 `Context`，纯 C++ 字符串所有权，不涉及 GC 与脚本可见行为。

## 登记链

| 注册入口 | 登记内容 | 位置 |
|---|---|---|
| `context.nativeType<T>().method(name, fn)` / `.property(name, getter)` | 方法签名（自动推导）与属性 | `Context.hpp` 的 `NativeTypeBuilder` |
| `binding::ClassBuilder::method/property/staticFunction` | 类成员签名（自动推导）；`method` 可附 `doc` 尾参 | `src/Binding.hpp` |
| `binding::Module::function/module/class_` | 全局函数与命名空间/类节点（含点分 owner 路径） | `src/Binding.hpp` |
| `context.registerClass<T>(name)` | 类型注册名 | `Context.hpp` |
| `registryGlobalFunc`（print/println 等） | 类型擦除的 `NativeFunction` 无签名可推导，由 `Session.cpp` 按注册事实显式补记 | `src/cli/Session.cpp` |
| 语言内建类型（String/Octet）与无 `registerClass` 的类型（RandomGenerator） | 仅注册名，成员走 `nativeType<T>()` 链 | 各 `*Lib.cpp` 注册函数开头 |
| `ScriptsNS` 的 `execStorage`/`evalStorage`（绑定闭包不走 `registerMethod`） | 注册点用 `api::makeMetaOf` 补记 | `src/stdlib/ScriptsLib.cpp` |

新注册入口默认即被记录；不走上述链路的自定义成员（如 `Namespace` 成员直接 `set`）需要
注册点用 `context.recordApiMember/recordApiGlobal` 补记，否则导出缺条目。

## 类型推导规则

`api::typeName<T>`（`ApiMeta.hpp`）按注册点 C++ 类型输出名称，其余一律空串（导出为
`null`），不编造类型：

| C++ 类型 | API 名称 |
|---|---|
| `void` | `void` |
| `Value` | `any` |
| `bool` | `Boolean` |
| 整型（含 `Integer`） | `Integer` |
| `Real` / 浮点 | `Real` |
| `String` / `std::string` / `const String *` | `String` |
| `Octet` / `const Octet *` | `Octet` |
| `const Object *` | `Object` |
| 其它（宿主 payload 类、`std::shared_ptr<T>` 等） | 空（未知） |

- `(VM*, Value, size_t, Value*)` 形态（stdlib 变长方法惯例）登记为 `variadic: true`，
  不输出参数表。
- `makeTypedFunction` 的 callable 只收业务参数（`NativeFunTraits` 已剥 `VM*/this/argc`
  前缀），签名推导同样只看业务参数。
- 参数名、默认值、条件变长无法从 C++ 类型推导；由注册点显式补充。
  `ApiMeta.params` 与声明模型共享 `ApiParam`（`src/common/ApiParam.hpp`），包含
  name/type/optional/collapse/defaultText。未提供 params 时保留推导，显式空数组表示无参数。
  原生 stdlib 的说明在注册点维护，LSP 不再维护第二份成员表。

## 补充签名与文档

原生接口以 C++ 注册事实为准，注解不能修改登记名、成员类别或归属；脚本库与外部宿主
可直接编写 `.d.tjs`。类型写在声明语法中，文档注释描述用途和行为。

```cpp
context.nativeType<Array>()
    .method("join"_str, &stdlib::arrayJoin,
            api::signature({ { .name = "delim", .type = "String", .optional = true,
                               .defaultText = "\",\"" } },
                           "String", "用分隔符连接数组元素。"));
```

`NativeTypeBuilder::method/property`、`ClassBuilder::method/property/staticFunction`、
`Module::function/class_` 接受显式 ApiMeta；已有 doc 字符串调用仍可用。
`ClassBuilder::constructor<Args...>` 自动登记构造器，也接受具名参数及说明。
注解只用于工具，不为绑定增加默认实参或改变运行时参数检查。

`accessSpecified/hasGetter/hasSetter` 描述已知属性访问方式；ClassBuilder 按实际绑定自动
登记，不能从原生 getter 存在推断所有属性只读，例如 Array.count/length 可写以调整长度。
`extends` 保留显式继承描述，不从 C++ payload 的继承关系猜测脚本继承。

## 导出格式

`cial --api-json` 输出单行 JSON（`schemaVersion: 1`），不执行用户脚本；`Exception` 与
`Debug` 是 TJS 脚本自举类，不属原生注册，不进入导出：

```json
{
  "schemaVersion": 1,
  "version": "0.1.0-preview.1",
  "classes": [
    {"name": "String", "members": [
      {"kind": "method", "name": "charAt", "variadic": false,
       "ret": "String", "params": ["Integer"]}
    ]}
  ],
  "globals": [
    {"kind": "global", "name": "print", "variadic": false,
     "ret": "void", "params": ["any"], "doc": "..."},
    {"kind": "namespace", "name": "Nested", "owner": ""}
  ]
}
```

- `classes` 按注册名排序（内部为 `unordered_map`），含 `members`；未登记注册名的类型
  不进入导出。
- `kind` 取值 `method` / `property` / `static` / `global` / `namespace` / `class` / `constructor`；
  `owner` 为点分归属路径（`Math.RandomGenerator` 类的成员归属仍由 `classes` 表达）。
- 类型未知输出 `null`；`doc` 存在时才出现该字段。消费方按 `schemaVersion` 判断兼容，
  忽略未知字段。
- schema 1 保留原 params 类型数组；显式签名额外输出 parameters 对象数组及默认值，
  已知访问器输出 hasGetter/hasSetter，继承输出 extends。构造器 kind 为 constructor；
  消费方忽略未知字段和不识别的 kind。

## 测试与消费方

- 元数据登记与推导：`tests/native_registration_test.cpp`（`[api]` 标签）。
- CLI 导出与参数校验：`tests/cli/test_cli.py` 的 `test_api_json`（签名对照 stdlib
  注册代码断言）。
- 声明生成（已实现）：`cial --api-decl` 与 `decl::declarationsFromCatalog` /
  `decl::generateDeclarations`（`src/decl/`），规则见[声明文件](declarations.md)；
  同名同类别成员按注册表语义替换，重注册回填不产生重复条目
  （`Context.hpp` 的 `recordApiMember`）。
- LSP 已消费 `--api-decl` 与核心 `--decl-json` 符号，替代 BUILTIN_MEMBERS；宿主随
  SDK/应用发布自己的声明即可，编辑器不会自动执行宿主应用来探测接口，见 [LSP](lsp.md)。


## 精确类型与工程文档约定

注册点可以显式填写 `String | RegExp`、`String | Function`、`Object | void`、
`Host.Resource` 和 `String[]` 等声明类型表达式，生成器与声明解析器保持字符串往返。
只细化实现能够保证的契约：例如 Dictionary.count 是 Integer，Scripts 的 filename/mode/context
具名登记，保留位置使用 void；动态脚本结果和异构数组元素仍为 any。

方法说明使用 Javadoc 风格的行为摘要和 @param/@returns/@throws/@example。生成器保留
注册点原始说明，并补充签名能够确定的类型/默认值标签，因此声明 doc 可以比 api-json 的
原始 doc 更丰富；其参数、返回与读写性仍与 API 元数据一致。完整工程组织约定见
[声明包规范](declarations.md#7-工程声明包规范)。
