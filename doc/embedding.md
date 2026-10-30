# 作为库使用

`Ciallang::Ciallang` 是库目标别名，通过目标传递 C++20、头文件路径和基础依赖。作为子目录引入时默认关闭 CLI 和测试，不修改父项目的全局编译选项（`CMakeLists.txt`）。

```cmake
cmake_minimum_required(VERSION 3.25)
project(MyHost LANGUAGES CXX)
add_subdirectory(external/Ciallang)
add_executable(host main.cpp)
target_link_libraries(host PRIVATE Ciallang::Ciallang)
```

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

此例还需包含 `types/Array.hpp`。`registerClass<T>` 创建并立即把对象注册为全局构造入口，返回指针由全局对象保根；`T` 应为实现构造调用协议的 Object 子类。方法和只读属性按 TypeTraits/TypeId 注册，因此同一类型的实例共享注册项。可写属性仍由对象的 `setProp` 实现；脚本 class/property 语义不变（`src/runtime/Context.hpp`、`src/stdlib/NativeRegister.hpp`）。

库不自动添加 CLI 的 print/println、Debug、Exception 自举；宿主按需注册（`src/stdlib/Print.hpp`、`ExceptionLib.hpp`）。`VM::eval` / `evalExpr` 的编译错误按既有 API 报到 stderr 并返回 void，运行期错误抛异常；需要结构化编译诊断的宿主应使用 Parser/IRGenerator 的 `Common::Result`（`src/vm/VM.hpp`）。
