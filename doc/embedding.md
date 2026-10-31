# 作为库使用

`Ciallang::Ciallang` 是库目标别名，通过目标传递 C++20、头文件路径和基础依赖。作为子目录引入时默认关闭 CLI 和测试，不修改父项目的全局编译选项（`CMakeLists.txt`）。

```cmake
cmake_minimum_required(VERSION 3.25)
project(MyHost LANGUAGES CXX)
add_subdirectory(external/Ciallang)
add_executable(host main.cpp)
target_link_libraries(host PRIVATE Ciallang::Ciallang)
```

配置源码工程需 Python 3.10+，用于从 `vcpkg.json` 生成构建目录中的版本定义；运行脚本本身不依赖 Python。

依赖必须由父项目先准备好。如果父项目使用 vcpkg manifest，应在自己的 `vcpkg.json` 中声明 fmt、frozen、utf8proc；父项目的 `project()` 已完成后，子项目不能追溯安装依赖。需要 CLI/测试/JIT 时也由父项目同步声明相应依赖，或独立构建 Ciallang。开关表见 [building.md](building.md)。

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

库不自动添加 CLI 的 print/println、Debug、Exception 自举；宿主按需注册（`src/stdlib/Print.hpp`、`ExceptionLib.hpp`）。`VM::eval` / `evalExpr` 的编译错误按既有 API 报到 stderr 并返回 void，运行期错误抛异常；需要结构化编译诊断的宿主应使用 Parser/IRGenerator 的 `Common::Result`（`src/vm/VM.hpp`）。

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
