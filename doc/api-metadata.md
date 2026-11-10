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
- 参数名、默认值、条件变长无法从 C++ 类型推导；需要时由注册点以 `doc` 尾参或显式
  `ApiMeta` 提供。LSP 手写名单（`tools/lsp/cial_lsp.py` 的 `BUILTIN_MEMBERS`）中的
  描述文案将按此路径逐步移入注册点。

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
- `kind` 取值 `method` / `property` / `static` / `global` / `namespace` / `class`；
  `owner` 为点分归属路径（`Math.RandomGenerator` 类的成员归属仍由 `classes` 表达）。
- 类型未知输出 `null`；`doc` 存在时才出现该字段。消费方按 `schemaVersion` 判断兼容，
  忽略未知字段。

## 测试与消费方

- 元数据登记与推导：`tests/native_registration_test.cpp`（`[api]` 标签）。
- CLI 导出与参数校验：`tests/cli/test_cli.py` 的 `test_api_json`（签名对照 stdlib
  注册代码断言）。
- 后续消费方：`.d.tjs` 生成器（路线图批次 B）、LSP 替代 `BUILTIN_MEMBERS`
  （路线图批次 C）；宿主可随 SDK/应用发布该导出产物。
