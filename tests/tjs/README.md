# TJS 脚本单元测试

语言和标准库的行为断言直接用 TJS 编写，两个解释器运行同一份文件。
每个文件是独立用例；原 Catch2 SECTION 拆为独立文件，避免全局变量、类/属性注册、
`RegExp.last` 等跨用例污染。目录按表达式、函数、循环、作用域、类、属性、异常、
预处理、with、集成场景和标准库分类，文件首行说明测试意图。

```sh
# 默认运行全部；也可在命令末尾指定一个文件或目录
python tests/run_tjs_tests.py --engine build/Linux/Debug/cial
python tests/run_tjs_tests.py --engine build/Linux/Debug/cial tests/tjs/property
python tests/run_tjs_tests.py --engine /path/to/TJSCompiler \
  --report build/tjs-reference-results.json

# 不依赖 Python runner，也可直接运行单个文件
build/Linux/Debug/cial tests/tjs/property/raw-registration.tjs
/path/to/TJSCompiler tests/tjs/property/raw-registration.tjs
```

测试先通过 `Scripts.execStorage` 加载 [assert.tjs](assert.tjs)，调用 `assertTrue` / `near`，
最后调用 `finishTests()`。失败抛出脚本异常；成功输出唯一的 `__TJS_TEST_PASS__:断言数`。
runner 同时要求进程成功退出、完成标记唯一且断言数大于零，防止空脚本、提前退出或
吞掉错误的执行器被误判为通过。每个进程默认限时 10 秒，`--timeout` 可覆盖；失败报告保留输出。

启用 `CIAL_BUILD_TESTS`、`CIAL_BUILD_CLI` 且 Python 可用时，CMake 自动将本目录脚本逐个
注册为 `CiallangTjs.*` CTest，用 `ctest --test-dir <build> -L tjs --output-on-failure` 运行。
新增脚本无需修改 C++ 清单；`assert.tjs` 是共享工具，不单独注册。
可选的 `-DCIAL_TJS_TEST_REFERENCE=/path/to/TJSCompiler` 同时注册 `ReferenceTjs.*`。

参考引擎必须接受脚本路径并提供 `Scripts.execStorage` 和 `Debug.message`，本次核对版本为
TJSCompiler `d8044c3`。runner 如实报告两个引擎的差异，不自动跳过、不归一化断言结果。
该版本在本机 Linux 上仍有原生库和崩溃问题：例如 RegExp 固定使用 UTF-16LE，而
`tjs_char` 为 32 位 `wchar_t`；因此“文件能交给参考引擎运行”不表示参考侧全量通过。
请保留 JSON 报告，区分解释器行为、参考版本和平台差异。

AST 结构、编译诊断、Value 的 C++ API、GC/调用帧/优化器内部状态、原生注册及宿主回调
继续使用 `tests/*_test.cpp` 的 Catch2 测试。跨多次 `VM::eval` 的宿主接口测试也保留。
尚未对齐的 `(const)` 容器等 Ciallang 专用回归不混入两引擎共用的语义用例。
