# 作为库使用

## 安装后的 SDK

先配置并构建库，再安装 SDK 组件（CLI 压缩包仍只包含 Runtime）：

```sh
cmake -S . -B build/sdk -DCMAKE_BUILD_TYPE=Release -DCIAL_BUILD_TESTS=OFF -DCIAL_BUILD_CLI=OFF
cmake --build build/sdk --parallel 4
cmake --install build/sdk --component SDK --prefix /path/to/ciallang
```

外部宿主准备相同工具链/运行库的 fmt、frozen、utf8proc、Oniguruma 后：

```cmake
find_package(Ciallang 0.1.0 EXACT CONFIG REQUIRED)
target_link_libraries(host PRIVATE Ciallang::Ciallang)
```

配置时通过 `CMAKE_PREFIX_PATH` 指向安装前缀与依赖。consumer 不依赖 Python、argparse 或 Catch2。
静态库若要链接进共享库，构建 SDK 时加 `-DCMAKE_POSITION_INDEPENDENT_CODE=ON`；
源码子项目继承父工程的 PIC 设置。CLI 构建不额外强制 PIC。
示例位于 `share/ciallang/examples/host`；`python tests/sdk_install_test.py --build build/sdk`
会安装、移动前缀并独立构建运行示例。

公共接口按两层承诺分层（设计依据见 `.agents/notes` 的 SDK note 与 `cmake/SDK.cmake`）：

| 层 | 头文件 | 承诺 |
|---|---|---|
| Tier 1 公共头 | `cial/Engine.hpp`、`cial/Storage.hpp`、`Binding.hpp`、生成的 `cial/Version.hpp` 与 `cial/Capability.hpp` | 明确范围内的 C++ 源码兼容；破坏性修改提升 `CIAL_SDK_API_VERSION` |
| 签名闭包 | Tier 1 签名引用的类型所在头：`common/Result.hpp`、`common/ResultMessage.hpp`、`types/Value.hpp`、`vm/VM.hpp`（Handle）、`runtime/Runtime.hpp`、`runtime/Context.hpp` | 与 Tier 1 同承诺，不随 Tier 1 独立变化 |
| Tier 2 其余传递头 | 其余随 SDK 分发的 VM/IR/前端头 | 可编译但不承诺稳定，宿主不应直接包含 |

附带的传递头随 SDK 分发仅为编译可用，不代表 VM/IR 内部布局稳定，也不承诺跨编译器、
CRT 或动态库 ABI。`tjsbc`、`decompile`、`cli`、`format`、`dbg` 为工具层源码，
不在 SDK 头安装范围内。`Binding.hpp` 的模板绑定与 `Engine` 的按值成员决定了内部头
无法从安装中移除，这是两层模型而非精选头清单的原因。

能力查询：链接宿主用生成的 `cial/Capability.hpp` 提供 `cial::capabilities::` 下的
constexpr 查询（`sdkApiVersion()`、`capabilitySchemaVersion()`、`hasJit()` 及模块/
调试预留位与已开启的 `hasDeclarationMetadata()`）；不链接 SDK 的工具用 `cial --capabilities` 取单行 JSON，
按 `schemaVersion` 判断兼容并忽略未知字段。预留能力位在对应能力落地前保持关闭，
宿主与工具不得报告为已支持；`CIAL_DEBUG` 反映包含方构建配置，不属于库能力
（`cmake/Capability.hpp.in`、`tools/generate_version.py`）。

```cpp
#include "cial/Engine.hpp"

int main() {
    cial::Engine engine;
    cial::Common::Result diagnostics;
    auto value = engine.execute("40 + 2", diagnostics, "host://startup", true);
    if(diagnostics.isFailed()) return 1;
    return value->asInteger().unwrap() == 42 ? 0 : 2;
}
```

Engine 拥有 Runtime/Context/VM，Handle 必须先于 Engine 销毁。运行期脚本异常以
`vm::ScriptExceptionCarrier` 传播，value 保留原抛出值；宿主若保存或跨分配使用该值，
应立即构造 `VM::Handle<Value>` 保根。编译失败从 Result 判断，合法 void 不能当作失败。
Engine 不自动打印编译错误，也不默认安装 CLI 的 print/Debug/Exception 自举；按需注册。

第五参 executionContext 接受对象：顶层声明落入该对象，读取按 this→global 回落，
声明函数保持接收者。省略、void 或 null 使用默认全局。这是脚本执行上下文，
不是模块隔离或完整 TJS2 Dispatch 桥。

宿主继承 `TextStorageReader::read(name, mode, from)` 并返回 `{sourceName, UTF8文本}`，
调用 `engine.setStorageReader(shared_ptr)` 即可接入 Scripts。name/mode 原样传递，from 为调用块
源码身份；宿主决定 XP3、编码及路径规则。读取失败抛标准异常，脚本可 catch。
共享指针不构成 GC 根，读取器里的脚本引用仍需 Handle。默认文件回落支持 UTF-8/UTF-16 BOM，
但不解释非空 mode。二进制流与 Array/Dictionary 文件 API 尚未接此接口。

## 源码子项目

`Ciallang::Ciallang` 是库目标别名，通过目标传递 C++20、头文件路径和基础依赖。作为子目录引入时默认关闭 CLI 和测试，不修改父项目的全局编译选项（`CMakeLists.txt`）。

```cmake
cmake_minimum_required(VERSION 3.25)
project(MyHost LANGUAGES CXX)
add_subdirectory(external/Ciallang)
add_executable(host main.cpp)
target_link_libraries(host PRIVATE Ciallang::Ciallang)
```

配置源码工程需 Python 3.10+，用于从 `vcpkg.json` 生成构建目录中的版本定义；运行脚本本身不依赖 Python。

依赖必须由父项目先准备好。如果父项目使用 vcpkg manifest，应在自己的 `vcpkg.json` 中声明 fmt、frozen、utf8proc、oniguruma；父项目的 `project()` 已完成后，子项目不能追溯安装依赖。需要 CLI/测试/JIT 时也由父项目同步声明相应依赖，或独立构建 Ciallang。开关表见 [building.md](building.md)。

```cpp
#include "vm/VM.hpp"
#include "stdlib/StandardLibrary.hpp"

int main() {
    cial::Runtime runtime;
    cial::Context context{ runtime };
    cial::stdlib::registerStandardLibrary(context);
    cial::vm::VMState state{ context };
    cial::VM vm{ &state };
    return *vm.evalExpr<cial::Integer>(cial::String{ "40 + 2" }) == 42 ? 0 : 1;
}
```

Runtime 的生命周期必须覆盖 Context、VMState 和所有 Handle。Handle/RootGuard 拷贝会独立保根，移动转移根；`Handle<Value>` 同样保护对象值。Value 句柄的箭头访问提供只读值视图，避免直接改写值后与已登记的根失配。`get()` 返回的裸指针仍受句柄生命周期约束（`src/runtime/Runtime.hpp`、`src/vm/VM.hpp`）。

## 统一宿主注册

内建库由 `stdlib::registerStandardLibrary(context)` 显式安装；也可只调用所需库的
`registerStringLib`、`registerArrayLib` 等入口。方法、属性和类对象的注册细节位于各自
`src/stdlib/*Lib.cpp`，结构化存取方法由 `registerSerializeLib` 安装。
汇总入口见 `src/stdlib/StandardLibrary.hpp/.cpp`；Context 只提供注册机制。

IRGenerator 构造函数接受 `ChunkOptimizer` 函数指针，默认使用
`optimizeDefaultChunk`，传入 `nullptr` 可生成未优化 TAC。自定义策略会传递到函数、
构造器和属性访问器；策略收到的 `OptReg` 是必须保留的外部结果寄存器。
默认 pass 组装与性能统计位于 `src/gen/Optimizer.cpp`，注入接口见
`src/gen/IRGenerator.hpp`。

```cpp
using namespace cial;
context.registerClass<Array>("HostArray"_str, &runtime);
context.registerMethod<Array>("answer"_str,
    [](VM *, const Value &, size_t, Value *) { return Value{ Integer{42} }; });
context.registerProperty<Array>("capacityHint"_str,
    [](const Value &) { return Value{ Integer{128} }; });
context.registerGlobal("hostVersion"_str, Value{ Integer{1} });
```

此例还需包含 `types/Array.hpp`。`registerClass<T>` 创建并立即把对象注册为全局构造入口，返回指针由全局对象保根；`T` 应为实现构造调用协议的 Object 子类。方法和只读属性按 NativeTypeKey 注册，因此同一 Context 中同一类型的实例共享注册项。声明式绑定的可写属性由 NativeAccessor 分派，其他属性由对象的 `setProp` 实现；脚本 class/property 语义不变（`src/runtime/Context.hpp`、`src/stdlib/NativeRegister.hpp`）。

库不自动添加 CLI 的 print/println、Debug、Exception 自举；宿主按需注册（`src/stdlib/Print.hpp`、`ExceptionLib.hpp`）。`VM::eval` / `evalExpr` 的编译错误按既有 API 报到 stderr 并返回 void，运行期错误抛异常；需要结构化编译诊断的宿主应使用 Parser/IRGenerator 的 `Common::Result`（`src/vm/VM.hpp`）。`ResultMessage` 携带纯文本 `message`、稳定错误码 `code`、源码身份 `source`、权威字节半开区间 `span()`（`hasSpan()` 为 false 表示无位置错误，不得以 (0,0) 代替）与终端摘录 `details`；REPL/eval 等虚拟源可用 `SourceFile::setSourceName` 设置逻辑名，`SourceFile::load` 对非法 UTF-8/NUL/体内 BOM 会以带位置的诊断使 Result 失败（`src/common/ResultMessage.hpp`、`SourceFile.hpp`）。范围与错误码约定见[诊断重构计划](diagnostics-plan.md)。

## 外部原生类型注册

外部 `Object` 子类可继承 `NativeObjectType<T>`，无需给 `TypeId` 增加枚举或特化
`TypeTraits`。实例身份由 `Object::nativeTypeKey()` 提供，注册表和 VM 原生成员缓存
统一使用 `NativeTypeKey`；VM 内部 Function/Property 等判型继续使用 `TypeId`
（`src/types/NativeTypeKey.hpp`、`src/types/Object.hpp`、`src/stdlib/NativeRegister.hpp`、
`src/vm/VMState.cpp`）。

```cpp
#include "runtime/Context.hpp"

class HostCounter final : public cial::NativeObjectType<HostCounter> {
public:
    cial::Integer value{41};
    void call(cial::vm::VMState &, cial::u32, size_t) override {
        throw std::runtime_error("HostCounter is not callable");
    }
};

// 在宿主已有的 context 上安装；注册名是实例名，不是 new 构造器。
context.registerClass<HostCounter>("counter"_str);
context.registerMethod<HostCounter>("answer"_str,
    [](cial::VM *, const cial::Value &self, size_t, cial::Value *) {
        auto *counter = static_cast<HostCounter *>(self.asObject().unwrap());
        return cial::Value{counter->value + 1};
    });
context.registerProperty<HostCounter>("value"_str,
    [](const cial::Value &self) {
        return cial::Value{static_cast<HostCounter *>(self.asObject().unwrap())->value};
    });
// 脚本：counter.answer() == 42；counter.value == 41
```

原生成员查询顺序为只读属性、方法、对象自身成员；直接调用与提取绑定方法使用相同
类型键。方法替换使当前 Context 的缓存失效，已经提取的方法保留原回调和接收者。
注册表按 Context 隔离，GC 继续通过 Context 标记注册的方法对象
（`src/runtime/Context.cpp`、`src/vm/VMState.cpp`；回归见 `tests/native_registration_test.cpp`）。

默认模板类型键仅保证同一链接映像中的身份一致。跨动态库时，由宿主提供生命周期
覆盖所有对象与注册项的非空唯一 token：对象覆写 `nativeTypeKey()` 返回
`NativeTypeKey::fromToken(token)`，注册使用
`context.registerMethod(key, name, callback)` / `registerProperty(key, name, getter)`。
不支持含活跃对象/回调的模块卸载。低层回调仍遵循现有 NativeFunction 协议，不自动
转换参数或验证显式重新绑定的接收者；需要时由回调检查类型键
（`src/types/NativeTypeKey.hpp`、`src/runtime/Context.hpp`）。

## 普通 C++ 类的声明式绑定

包含公共头 `Binding.hpp` 即可使用 `context.module()` 与 `class_<T>()`。类无需继承
解释器类型，也无需修改枚举或 traits；构造得到的 `NativeObject` 使用独立分配的
C++ payload，因此不受 GC 的 512B 节点大小限制（`src/Binding.hpp`、
`src/types/NativeObject.hpp`）。

```cpp
#include "Binding.hpp"

struct Counter {
    explicit Counter(int value) : value(value) {}
    int value;
    int get() const noexcept { return value; }
    void set(int n) { value = n; }
    int add(int n) { return value += n; }
};

context.module("Host"_str).class_<Counter>("Counter"_str)
    .constructor<int>()
    .method("add"_str, &Counter::add)
    .property("value"_str, &Counter::get, &Counter::set)
    .property("readOnly"_str, &Counter::get);
// 脚本：var c = new Host.Counter(20); c.value = 40; c.add(2); // 42
```

`binding::Module{context}` 表示全局安装作用域；`context.module(name)` 创建或重新打开
命名空间，同名非命名空间值会报错。模块 `.function(name, fn)` 注册普通 C++ 函数或
lambda；类 `.staticFunction(name, fn)` 注册静态函数。成员注册可链式调用，旧的
`registerMethod` / `registerProperty` / `registerClass` API 保留。相同类型的成员在当前
Context 内共享；重新注册同名方法更新后续读取，已提取方法保留原函数。builder 和
module 句柄通过 RootGuard 保活各自对象，生命周期必须位于 Runtime/Context 内
（`src/Binding.hpp`、`src/runtime/Context.hpp`）。

转换层支持 `Value`、`String`、`std::string`、整数、bool、浮点数、`Real`，以及以引用
传入的已绑定 C++ 对象；成员函数支持 const / noexcept。返回值还支持已注册类型的
`std::shared_ptr<T>`，`void` 映射为脚本 void。实参必须与声明数量一致，整数转换检查
范围，错误接收者抛出运行期异常；运行期 std::exception 继续使用已有脚本异常协议。
默认参数、重载选择、继承指针调整和自动借用裸指针不在此 API 的支持范围
（`src/Binding.hpp`、`src/types/NativeFunDetail.hpp`、`src/vm/VMState.cpp`）。

构造入口由包装对象拥有 C++ payload；共享已有对象使用：

```cpp
auto counter = std::make_shared<Counter>(1);
context.registerGlobal("shared"_str, cial::binding::wrap(context, counter));
```

payload 最后一个共享所有者释放时析构。析构函数只做 C++ 资源回收，不调用脚本。
若 payload 内保存脚本 Value，则必须用 `.trace()` 显式枚举；trace 必须 noexcept，
只标记引用，不分配对象或执行脚本：

```cpp
// 假设 HostState 有 cial::Value callback 字段。
context.module("Host"_str).class_<HostState>("State"_str)
    .constructor<>()
    .trace([](HostState &state, cial::binding::Tracer trace) noexcept {
        trace(state.callback);
    });
```

外部 shared_ptr 本身不构成脚本 GC 根；需要在包装对象不可达后仍保留其中的脚本引用时，
宿主使用已有 `VM::Handle<Value>`。绑定回调闭包捕获的脚本值同样需要宿主显式保根，
不能依靠 C++ lambda 捕获自动进入 GC。见 `src/types/NativeObject.hpp`、
`src/types/Function.hpp`、`src/vm/VM.hpp`。

标准库通过 `context.nativeType<T>().method(...).property(...)` 安装已有原生回调，继续
保留函数指针快路径和原有构造协议。各内置 Object 类型在自身头文件声明
`nativeTypeId`，`NativeRegister.hpp` 不再包含 Array/Dictionary/Date/Math 的类型清单。
`NativeAccessor` 支持 GC 可见的 getter/setter；活动访问器在回调替换自身并触发 GC 时
也被保根（`src/runtime/Context.hpp/.cpp`、`src/stdlib/*Lib.cpp`、
`src/stdlib/NativeRegister.hpp`、`src/vm/VMState.cpp`）。

回归测试见 `tests/native_binding_test.cpp` 和 `tests/native_registration_test.cpp`，覆盖
普通 C++ 类构造、属性写入、只读错误、静态函数、转换错误、类型隔离、共享对象、
payload trace、接收者保活和活动 getter 自替换回收。

源码编译默认保留诊断正文：`SourceFile::snapshot()` 按装载版本惰性创建只读正文与行索引，嵌套函数的 Chunk 共享同一份，故障元数据在异常展开后继续持有；不借用原 SourceFile，也不持有脚本对象或增加 GC 根。重新装载或改名不改变已编译代码的快照。正文及行索引成本随不同源码版本累积，最后一个 Chunk、故障元数据及 SourceFile 缓存释放后回收；编译期间原缓冲与快照各有一份正文和行表。宿主可在传给 `VM::execute`/IRGenerator 的 SourceFile 上调用 `setRetainSource(false)`，只对该源码的后续编译关闭保留（不改变独立 eval/嵌套存储读取的策略）；身份和行列仍保留，渲染器不会重新读取磁盘（`src/common/SourceFile.*`、`src/vm/Chunk.hpp`、`src/vm/VMState.hpp`）。

运行期栈由 `VMState::fault().frames` 提供（`src/vm/VMState.hpp`）：按故障帧到外层调用者排列，包含函数名、源码身份、0 基行列、`hasLocation`、`kind` 和只读正文快照。字段是独立值，不借用 Chunk/FuncMeta/Atom；可在异常展开和 GC 后复制/保留。`valid` 仅表示首帧有位置，`frames` 非空但 `valid == false` 表示仍可查看无源码的栈。最多 `maxFaultFrames`（64）帧，`truncated`/`tailFramesOmitted` 表示降级，`NativeBoundary` 表示未捕获 C++ 调用链。宿主哨兵转交 `ScriptExceptionCarrier` 时保留首次故障，脚本 catch 和新一轮顶层执行清理元数据；脚本重新 throw 时建立新栈。捕获只读取帧、原始 Atom 名与行映射，不访问异常对象成员或执行脚本。

getter 的同步哨兵按 VMState 惰性缓存，嵌套访问复用同一份已编码的空 Chunk，各帧拥有独立寄存器窗口；仅含 void 常量，不持有脚本对象。getter 返回或抛错后窗口均回收，不向调用者遗留返回槽（`src/types/Property.hpp`、`src/vm/VMState.cpp`，`tests/diagnostics_test.cpp` 的池顶回归）。
