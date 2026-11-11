# 测试导航

## 目录与职责

| 位置 | 用途 |
|---|---|
| `*_test.cpp` | Catch2：内部状态、原生绑定、AST、GC、VM 与优化器；显式登记于 CMakeLists.txt |
| `tjs/` | 语言与标准库行为，TJS 断言；见 [脚本指南](tjs/README.md)，由 CMake 自动登记；`.d.tjs` 声明文件不登记为脚本测试（[声明文件](../doc/declarations.md)） |
| `cli/test_cli.py` | 命令行参数、执行、诊断、JSON、stdin、REPL |
| `cli/test_decompile.py` | 反编译接口、严格模式、表达式、声明、字面量、行表 |
| `cli/test_switch.py` | switch 派发、循环组合、嵌套、参考字节码与拒绝回归 |
| `cli/support.py` | 共用进程执行、临时目录、参数及语料路径；不定义测试 |
| `formatter_test.py` / `dbg_test.py` / `cli/test_disasm.py` | 格式化器、调试器与反汇编器进程回归（按构建目标注册） |
| `run_tjs_tests.py` / `test_tjs_runner.py` | TJS 执行器与执行器自身回归 |
| `sdk_install_test.py` / `test_packaging.py` | SDK 安装消费与打包验证，入口见 CMake/CI |
| `test_files/` | C++/CLI 固定输入、预期及参考导出的字节码；不是独立脚本测试目录 |
| `fuzz_regress/` | 前端 fuzz 回归输入 |

反编译语料在 `test_files/tjsbc/decompile/`；`manifest.json` 保存源/字节码哈希、
参考引擎身份与字节码恢复的预期状态。源码恢复与字节码恢复能力分别验收，
不能因某份源码能恢复就将对应字节码的 unsupported 改为 success。
switch 固定语料只由 `SwitchTest.test_switch_corpus` 遍历一次：所有样本检查源码
与转译执行一致，再按清单断言字节码恢复成功或严格拒绝。

- `sourceChecks`：存在时额外验证源码恢复，与字节码预期独立。
- `bytecodeChecks`：成功恢复后的额外结构检查。
- 两种检查均支持 `minSwitches`、`contains`、`entryLocals`；保留嵌套层数、
  if 结构及函数入口局部声明的原有断言，不再用多组测试重复反编译同一份字节码。
- `debug/manifest.json` 的可选 `source` 为相对该清单的源码路径，省略时用
  `<name>.tjs`；允许 debug/无 debug 字节码共享完全一致的源码，仍各自校验哈希。

原始日志、临时探测和对拍报告放 Git 忽略的 `out/`。

## 运行

```sh
ctest --test-dir build/Windows/Debug -R "^(CiallangCli|CiallangDecompile|CiallangSwitch)$" --output-on-failure --no-tests=error
python tests/cli/test_cli.py --cial build/Windows/Debug/cial.exe
python tests/cli/test_decompile.py --cial build/Windows/Debug/cial.exe
python tests/cli/test_switch.py --cial build/Windows/Debug/cial.exe
# 单个用例（类名和方法名可从对应脚本查看）
python tests/cli/test_switch.py --cial build/Windows/Debug/cial.exe SwitchTest.test_switch_corpus
```

Linux/macOS 使用相应构建目录与不带 `.exe` 的可执行文件。
CLI 脚本独立接收 `--cial`、可选 `--expected-version`，其它参数交给 unittest。
每个测试使用独立临时目录；共用辅助代码不在导入时解析参数或执行测试。
新增进程测试优先归入对应主题，新增独立脚本须同步 CTest 注册与本文索引。
相同执行流程优先新增清单样本或检查项；独立协议/行为断言保留独立测试。
TJS runner 的单元测试检查执行器能否识别失败，不等同于语言用例；SDK 安装
消费与发布打包的边界也不同，不能仅因都调用子进程就合并或删除。
