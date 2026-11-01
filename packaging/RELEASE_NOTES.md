这是 Ciallang CLI 预览版，面向 TJS2 脚本兼容与宿主嵌入探索，尚未承诺稳定的 C++ SDK/ABI。

下载对应系统和架构的 ZIP/TAR.GZ，按 SHA256SUMS 或附件 .sha256 校验，解压后执行
bin/cial --version 和 examples/hello.tjs（Windows 使用 bin/cial.exe）。

每个包包含构建提交、编译器、系统/架构与功能开关信息，以及项目与第三方许可。
此版默认关闭可选 JIT。Linux 包依赖构建平台对应的系统基础运行库；macOS 部署下限见包内构建信息。

当前边界：SDK 已有源码安装入口，不包含在 CLI 包内，也未承诺稳定 ABI；
TJS2 对象桥与真实 krkr2 游戏替换验收、.d.tjs 声明模式、模块系统和 DAP 调试器仍未完成。
本批 GCC 全局整数循环对照存在约 10% 性能回退，待继续定位；测量与原始样本见 doc/performance.md。
语言兼容性、参考侧不稳定项和后续路线请参阅仓库 doc/compatibility.md、doc/faq.md 与 doc/roadmap.md。

本草稿由各平台构建、CTest、打包及解压验收全部通过后生成；公开发布前请核对资产和说明。
