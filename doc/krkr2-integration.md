# krkr2 接入与替换验收

目标为 [2468785842/krkr2](https://github.com/2468785842/krkr2)，核对基线为
`main` 的 `dca7264572a63e753c1b07ca7053513d1751ce70`。这是跨平台模拟器分支：
`cpp/core/tjs2/tjsTypes.h` 定义 `tjs_char = char16_t`，顶层 CMake 使用 C++17、PIC，
包含桌面和 Android 路径。Ciallang 目标自行要求 C++20，不修改父工程默认标准。

## 当前基础

- null 在源码、容器、字节码和结构化存取中区别于 void，GC 能标记空对象值。
- 字符串基础操作及正则位置按 UTF-16 码元计数，内部 UTF-8/WTF-8 保留代理对切片。
  RegExp 使用与宿主相同的 Oniguruma Perl 方言；index 不追仿参考实现的字节混算缺陷。
- Engine 提供逻辑源码身份、结构化编译诊断和带根结果；嵌套调用隔离异常。
  Scripts 第三参 context 接收顶层声明并保留声明函数的接收者。
- TextStorageReader 将存储名、mode、调用源身份交还宿主，已有虚拟存储测试。
- SDK 有 install/export、版本头与移动前缀后的独立宿主验收。CI 已登记三平台 Release
  安装验收，远端是否通过以实际运行结果为准。

这些是接入基础；双向 TJS2 对象桥和真实 KAG/游戏启动验收尚未完成。

## 目标仓库中的接入点

| 路径/入口 | 当前职责 | 适配要求 |
|---|---|---|
| `cpp/core/tjs2/CMakeLists.txt` | 静态 tjs2、解析器和头文件 | 增加可切换后端并保留回退；依赖由父 manifest 声明 |
| `cpp/core/base/ScriptMgnIntf.cpp` / `TVPInitScriptEngine` | new tTJS、自举、取得 global、注册 Window/Layer/Timer | 全局桥必须接收已有 iTJSDispatch2 原生类与方法 |
| 同文件 `TVPExecuteScript` / `TVPExecuteExpression` | context、逻辑名及行偏移 | 转换诊断和异常，保留 context/name/lineofs |
| 同文件 `TVPExecuteStorage` | 二进制探测、placed path、文本流 | 接 TVP 解析及 XP3；虚拟路径不能交给 std::filesystem |
| `cpp/core/base/TextStream.h` / `StorageIntf.h` | 文本流与路径解析 | 实现 TextStorageReader，遵循流的 Destruct 协议 |
| `cpp/core/tjs2/tjsInterface.h` / `tjsVariant.h` | Dispatch vtable、Variant、Object/ObjThis | 双向转换、错误码、flags、对象身份与引用持有 |
| `cpp/core/plugin/`、`cpp/plugins/` | 插件依赖现有 API | 验证原生实例、枚举、ClassInstanceInfo 与生命周期 |

## 继续实施的依赖顺序

1. 对象桥与生命周期：宿主对象进入脚本时 AddRef，包装对象回收时 Release；脚本对象交给
   宿主时持有 Handle。缓存保持身份且不形成永久根，关闭时先解绑回调与桥，再销毁运行时。
   回传闭包分别保留 Object 和 ObjThis。
   两后端共存时协调 Oniguruma 的进程生命周期：宿主 `tjsRegExp.cpp` 的
   `TJSReleaseRegex()` 会调用 `onig_end()`，不能在另一后端仍持有表达式时提前释放全局表。
2. 分派协议：MEMBERENSURE/MUSTEXIST/IGNOREPROP、隐藏/静态成员、数字成员、
   FuncCall/CreateNew、一次 Operation、枚举、NativeInstanceSupport、ClassInstanceInfo、
   Invalidate/IsValid 与异常转换。多个 TAC 的读改写不能冒充一次宿主 Operation。
3. TVP 文本/二进制读写、XP3、编码与模式串。Scripts 读取器只是入口；Array/Dictionary
   序列化仍走文件系统，字节码直载与行偏移也需补齐。
4. 字节码 proxy/typeof/动态类缺口；在宿主增加后端开关，先验证无窗口启动，再运行 KAG。
5. Window/Layer/Timer 回调、插件、存档、重启与退出；覆盖强制 GC、循环引用、无效对象、
   异常和宿主关闭。桌面通过后验收 Android ARM64/x86_64。

完成标准是目标宿主在相同资源与脚本下通过验收，且能切回原 TJS2 差分。
单元测试、fixture 匹配与 SDK 链接分别证明各自范围，不单独构成“核心可替换”。
未完成项见 [TODO](../TODO.md#krkr2-接入)，CLI 公开发布另见[发布指南](releasing.md)。
