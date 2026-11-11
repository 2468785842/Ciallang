# 原生 API 丰富元数据与 LSP 声明消费

Status: implemented

## 问题与决定

基于 `057ef5e`，声明解析/生成已存在，但 Python LSP 的函数绑定要求有函数体，
会漏掉 `.d.tjs` 的方法，并维护独立 BUILTIN_MEMBERS；接口类型与文档容易漂移。
本批主体实现为 `a7dd06d`，收尾补齐 SDK 声明能力位与安装消费方验收。

原生接口以 C++ 注册点的 ApiMeta 为权威，生成声明；脚本自举类与第三方 SDK 可手写声明。
类型使用声明语法，doc 保存说明，不从文档字符串猜测类型。

## 实际实现

- `common/ApiParam.hpp` 在注册与声明模型间共享具名参数、类型、默认字面量、可选/收拢信息。
  ApiMeta 保留旧字段，丰富 params 优先；登记时同步旧参数类型与 variadic，显式空参数可覆盖变长推导。
- Context/Binding 的 fluent 注册可附 ApiMeta；ClassBuilder 自动登记构造器及实际属性访问器。
  显式注解不能改写登记的名字/归属/类别，也不会为运行时添加默认实参或改变参数检查。
- `--decl-json` 复用核心声明解析器，输出 schemaVersion 1 符号树、UTF-8 名称偏移及嵌套 schema 2 诊断；
  不执行输入。JSON 正确转义控制字符，参数和注释保持传递。
- LSP 删除 BUILTIN_MEMBERS。生成标准声明和手写声明均由核心转换；Array/String 的常用签名具名，
  原生方法说明移到注册点。修正旧描述中的 Array.sort 返回类型、count/length 可写性、
  remove 越界异常与 split 的 reserved 参数。
- 声明提供补全、纯文本悬停、定义导航、继承和返回类型推导；支持命名空间构造/工厂，
  区分函数值和调用返回值，尊重局部构造器遮蔽。导航转换到 UTF-16，生成声明可在服务生命周期内打开。
- 项目声明覆盖对应生成接口；缓冲优先、关闭恢复、修改/删除更新。显式 declarationFiles 可指定工作区外 SDK 文件，
  不运行宿主应用或扩大扫描到整个 SDK 目录。
- 无效声明不进入普通脚本索引回退；多义或预算内未能分析的同名声明不被误当成唯一接口。
  API rename 保守拒绝，脚本局部 rename 保持现状。分析结果按正文缓存，临时工具失败不永久缓存为空接口。
- VSCode 显式关联 .d.tjs，打包两个 Python 模块及脚本 Exception 声明；必要编辑配置有单元测试。
- SDK 的 hasDeclarationMetadata() 开启；安装消费方实际注册具名接口、生成声明、检查文档并执行宿主调用。

## 验证

本机 Debian 13，GCC 14.2.0、CMake 4.4.4，Debug / CLI+tests 开启 / JIT 关闭 / warnings-as-errors 开启。
依赖解包到用户目录，未修改全局工具链；构建目录 `build/decl-lsp`。

- `cmake --build build/decl-lsp --parallel 3` 成功；CTest 先核对注册，再全量 **460/460** 通过，含 238 个 TJS 测试。
- `tools/lsp/test_protocol.py --cial build/decl-lsp/cial`：27 项，26 通过、1 项 Windows drive URI 因平台跳过。
  真实 stdio 子进程覆盖原生/手写声明、参数与文档、中文/emoji 范围、命名空间/继承、缓冲/磁盘变化、
  外部 SDK、歧义、无效声明和超预算保守拒绝。
- `tests/cli/test_cli.py`：39 项通过，含 API JSON → 生成声明 → 核心符号的成员/签名/文档一致性，
  声明分析 JSON 及不执行输入的检查。
- 编辑器 JavaScript 检查与 17 项 Node 测试通过；旧注释配置断言改为接受合法字符串/对象两种形式。
- `cmake --install` 后，`tests/installed` 对实际安装头/库编译，`installed_host` 1/1 通过。
- clang-format 20.1.0 全仓校验、git diff 检查与改动文档的本地文件链接检查通过。

原始输出在 `out/declarations-*.log`。主体提交的远端矩阵已启动：
[43007349270](https://github.com/2468785842/Ciallang/actions/runs/43007349270)，
本记录建立时仍运行，未计为本机通过依据。

## 保留边界

没有实现静态类型检查、任意手写声明/实现一致性检查、重载选择或独立冲突诊断。
普通不完整源码仍使用原保守索引，C++ cial-lsp exe 仍为骨架；此次交付为实际使用的 Python LSP。
SDK 任意动态原生调用协议中无法确定的类型/构造参数仍保持未知。
声明最多分析 64 文件，单文件 2 MB，共享工作区内容预算，核心调用及一轮未缓存分析各有 5 秒上限。
未在本批启动真实 VSCode Extension Development Host，也不将 Linux 协议验证计作 Windows/macOS 客户端验收。
