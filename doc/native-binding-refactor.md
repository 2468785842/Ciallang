# 原生库注册重构报告

## 结果

宿主现在可以只在外部项目包含 `Binding.hpp`，注册普通 C++ 类的构造、方法、静态函数、
getter/setter 和模块函数，无需修改 TypeId、TypeTraits 或 VM 类型白名单。
使用示例与 API 约束见 [embedding.md](embedding.md#普通-c-类的声明式绑定)。

## 实现范围

| 层 | 改动与源码 |
|---|---|
| 类型身份 | `src/types/NativeTypeKey.hpp`：开放类型键；保留 TypeId 作为 VM 内部判型与内置兼容编号 |
| 外部 Object | `src/types/Object.hpp`：NativeObjectType<T>、nativeTypeKey/nativePayload 协议 |
| 普通 C++ 类型 | `src/types/NativeObject.hpp`：共享描述符与独立分配的 shared_ptr payload，支持大对象和显式 trace |
| 声明式 API | `src/Binding.hpp`：Module、ClassBuilder、自动转换、实参数量/范围/接收者检查、wrap、静态函数 |
| 注册机制 | `src/runtime/Context.hpp`、`src/stdlib/NativeRegister.hpp`：开放注册表、NativeAccessor、nativeType<T>()、Context 隔离 |
| 执行与 GC | `src/vm/VMState.hpp/.cpp`、`src/runtime/Context.cpp`：移除白名单，缓存随注册版本失效；注册及活动访问器保根 |
| 标准库 | `src/stdlib/*Lib.cpp` 使用 nativeType<T>()；各 Object 自声明 nativeTypeId，注册头移除具体对象类型清单与直接包含 |

已有 registerClass/registerMethod/registerProperty 入口保留，低层回调仍遵循原有协议。
registerClass 继续创建并挂载 Object，不改变已有内置类构造行为。普通 C++ 类使用
class_<T>().constructor<Args...>()，与既有 Object 子类注册路径分开表达。

方法提取继续绑定接收者；方法重新注册使后续查询更新，已提取的方法保留原回调。
命名空间可以重新打开，module/builder 句柄保根自身对象。共享 C++ payload 最后一个
所有者释放时析构；脚本引用必须通过 trace 或显式 Handle 保根。

## 验证

- macOS Debug / Release 各 238 项 CTest 全部通过，包含 CLI、LSP、字节码工具检查。
- LSP 使用 `TMPDIR=/private/tmp`，规避当前测试对 macOS `/var` 别名的 URI 比较差异。
- 新增绑定专项测试 5 个用例、38 条断言全部通过。
- 新增 `tests/native_registration_test.cpp` 与 `tests/native_binding_test.cpp`，覆盖外部类型
  隔离、Context 隔离、注册替换、提取方法保活、可写/只读属性、转换失败、共享对象、
  大 payload、trace、析构以及 getter 替换自身后触发 GC。
- 独立宿主在 `/tmp/cial-binding-host` 通过 add_subdirectory + Ciallang::Ciallang 配置、
  构建和运行；只使用公共 Binding.hpp，注册普通 Counter，脚本构造/属性写入/方法调用返回 42。
- 五项 Release 同机两轮对照见 [performance.md](performance.md#2026-10-30-原生绑定层重构对照)。
  原生方法近似持平，整数循环与字符串耗时回退约 2.4%–3.4%，尚未定位原因。

### ASan 验证限制

已核实：ASan 支持 macOS ARM64，本次现象不应记为“ARM 芯片尚不支持”。本机
macOS 26.5.1 / Apple clang 17.0.0（clang-1700.3.19.1）的初始化栈，与
[LLVM issue #200447](https://github.com/llvm/llvm-project/issues/200447) 中的运行库死锁
报告吻合。平台支持与泄漏检查的区别、最小验证步骤见
[building.md](building.md#asanmacos-arm64)。

额外配置了 `/tmp/cial-binding-asan`，编译完成后测试发现阶段卡在运行库初始化。
`sample` 的栈为 `libSystem_initializer → __malloc_init → __asan::AsanInitInternal →
InitializeShadowMemory → get_dyld_hdr → _Block_copy → malloc → AsanInitFromRtl →
StaticSpinMutex::LockSlow`，尚未进入 main。用 `/usr/bin/c++ -fsanitize=address` 编译
`int main() { return 0; }` 的最小程序，同样在 5 秒内无法完成启动。禁用 detect_leaks 后
仍复现；本机运行库还明确报告 detect_leaks 不受支持。本轮 ASan 未能完成，不能视为
内存安全检查通过。相关临时文件：`/tmp/cial-asan-minimal.cpp`、
`/tmp/cial-asan-sample.txt`。

## 明确边界

- 未提供重载选择、默认参数、继承指针调整或裸指针自动借用。
- 默认模板键用于同一链接映像；跨 DLL 仍使用低层显式 token 接口，不支持活跃模块卸载。
- 普通 C++ 析构只负责资源回收，不运行脚本；trace 必须 noexcept，只标记脚本对象。
- callback 闭包里的脚本引用不会被自动扫描，需要宿主显式保根。
- 当前验证平台为 macOS arm64，Windows/Linux 尚未在本机验证。

## CLion 检查与整理（2026-10-30）

使用 CLion MCP `get_file_problems(errorsOnly=false)` 检查本次重构涉及的文件，
修改后复查以下 11 个核心文件，WARN/ERROR 均为零（拼写及其他 INFO 级建议单独保留）：
`Binding.hpp`、`types/NativeObject.hpp`、`types/NativeTypeKey.hpp`、
`runtime/Context.hpp`、`stdlib/NativeRegister.hpp`、`types/Object.hpp`、
`types/Class.hpp`、`types/RegExp.hpp`、`types/NativeFunDetail.hpp`、
`tests/native_binding_test.cpp`、`tests/native_registration_test.cpp`（源码路径相对 `src/`，
测试路径相对仓库根目录）。

处理内容：未使用 include/变量、nodiscard、const 参数与遍历、按值参数移动、原地构造
Value。模板工厂 `makeFunction` 避免在依赖成员表达式上使用编辑器认为冗余但编译器需要
的 template 消歧；`makeTypedFunction` 合并模块函数与静态函数重复适配。
低层回调 traits 增加 `const VM *` 兼容签名（`src/Binding.hpp`、
`src/types/NativeFunDetail.hpp`、`src/vm/VM.hpp`）。测试的大 payload 字段用
maybe_unused 明确其布局验证用途；echo 改为返回带实例状态的字符串，避免将实例绑定
测试错误改成静态方法（`tests/native_binding_test.cpp`）。

额外优化将原生成员查询合并为一次类型表定位，验证与性能数据见
[performance.md](performance.md#同日-clion-检查后的整理与查询合并)。
Debug/Release 各 238 项 CTest 全过。

未机械采用所有编辑器建议：VM 中受 TypeId/opcode 保护的 static_cast 维持项目热路径
约定；ScriptExceptionCarrier 是内部脚本异常传播协议，不改成普通运行期错误；
标准库对外既有回调签名保留兼容性。也保留直接使用 std::time / std::uint32_t 的
标准头，避免依赖间接包含。相关位置：`src/vm/VMState.cpp`、`src/vm/VMState.hpp`、
`src/stdlib/MathLib.cpp`、`src/stdlib/DateLib.cpp`。
