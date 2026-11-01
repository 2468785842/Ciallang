这是 Ciallang 的首个预览版，面向 TJS2 脚本执行、兼容性验证与宿主嵌入探索。

## 本版提供

- CLI、REPL、JSON 编译诊断，以及 TJS2 字节码加载与转译；兼容范围见[兼容性说明](https://github.com/2468785842/Ciallang/blob/v0.1.0-preview.1/doc/compatibility.md)。
- 常用语言与标准库功能；本批区分 null/void，基础字符串操作按 UTF-16 码元计数，RegExp 使用 Oniguruma。
- 源码中的实验性 C++ SDK：CMake install/export、Engine 执行入口、带根结果、执行 context 与宿主文本存储读取器。SDK 从源码另行安装，不包含在 CLI 压缩包内，尚未承诺稳定 ABI。

## 下载与运行

按系统和架构选择附件，使用 `SHA256SUMS` 或对应 `.sha256` 校验后解压：

```sh
bin/cial --version
bin/cial examples/hello.tjs
bin/cial --repl
```

Windows 将程序名替换为 `bin/cial.exe`。程序运行不需要源码、vcpkg 或 Python。
包内 `build-info.json` 记录提交、系统/架构、编译器、运行库及功能开关，`licenses/` 包含第三方许可与依赖资料。
本版关闭可选 JIT。Linux 包使用 Ubuntu 24.04 构建；macOS 部署下限为 15.6，均只面向包名所列架构，系统基础运行库仍由操作系统提供。

## 已知限制

- 尚不能直接替换 krkr2 的 TJS2 核心：双向对象桥、TVP/XP3 接入及真实游戏验收仍未完成，见[接入计划](https://github.com/2468785842/Ciallang/blob/v0.1.0-preview.1/doc/krkr2-integration.md)。
- 字节码 proxy、缺失成员 typeof、动态类及部分 Unicode 边界仍有缺口；`.d.tjs`、模块系统和 DAP 调试器尚未实现。
- 本批 GCC 全局整数循环对照存在约 10% 性能回退，尚待定位；环境、基线与原始样本见[性能记录](https://github.com/2468785842/Ciallang/blob/v0.1.0-preview.1/doc/performance.md)。该数据不代表发布包所用编译器的性能结论。

普通 CI 覆盖三平台 Debug/Release、Windows JIT 与 Release SDK 安装消费测试。
发布流程另在各平台执行构建、全部 CTest、打包、独立目录解压运行及校验和核验，全部通过后才生成 Release 草稿。
