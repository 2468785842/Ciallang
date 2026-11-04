# 2026-10-31 共用脚本参考侧复核

Windows x64，参考仓库提交 `40a736c48f40dff7c737e9c5dc9c0e46f145e9e5`，工作树干净；现有 windows-release 构建
`TJSCompilerCli` 目标无待构建项。二进制 SHA-256：`8f370f2bb7460c8b7b8090a8bc71b7d0fb94a25e98ee2ff20d01bf3df358c60b`。
入口已实际验证接受脚本路径、加载 assert.tjs 并输出测试完成标记。

首轮 224 文件中 156 通过、68 失败；对失败文件原样复测，24 个转为通过，44 个再次失败。
这不是历史 Linux/GCC 14/d8044c3 的 191/224 复现，不能合并统计；本机参考仓库不存在 `d8044c3` 提交。
`expression/instanceof-primitive.tjs` 独立连续三次出现通过、语法错误并异常退出、通过。
同一输入结果漂移，说明当前参考二进制尚不能作为全部失败项的语义裁判；本批不修改参考仓库。

失败均逐项记录如下。“报告语法错误”仅描述输出，不代表本项目应拒绝该语法；
“复测通过”也没有将首轮失败改记为通过。异常退出即使先输出通过标记仍然算失败。
本项目对应脚本由 Debug CTest 验证；新增常量容器脚本另计，不混入该次 224 文件统计。

结构化证据见 [JSON](2026-10-31-reference-audit.json)；完整原始输出保存在本机构建目录
`build/tjs-reference-todo.json`、`build/tjs-reference-todo-retry.json`。
运行命令见 [脚本指南](../../tests/tjs/README.md)，首轮使用默认 10 秒超时、复测使用 3 秒。
下一步需先取得无随机崩溃的参考构建，再将重复失败缩减为最小用例逐项确认语言差异。

| 类别 | 数量 |
|---|---:|
| 参考侧运行错误或崩溃，需稳定环境归因 | 24 |
| 输出通过标记后异常退出 | 4 |
| 复测通过，参考侧不稳定 | 24 |
| 参考侧报告语法错误 | 16 |

| 脚本（tests/tjs/） | 首轮退出码 | 复测退出码 | 分类 |
|---|---|---|---|
| [class-extends/bound-slot.tjs](../../tests/tjs/class-extends/bound-slot.tjs) | 0xc0000005 | 0xc0000005 | 参考侧运行错误或崩溃，需稳定环境归因 |
| [class-extends/combined.tjs](../../tests/tjs/class-extends/combined.tjs) | 0xc0000005 | 0xc0000374 | 输出通过标记后异常退出 |
| [class-extends/default-constructor.tjs](../../tests/tjs/class-extends/default-constructor.tjs) | 0xc0000005 | 0xc0000005 | 参考侧运行错误或崩溃，需稳定环境归因 |
| [exceptions/control-flow/04.tjs](../../tests/tjs/exceptions/control-flow/04.tjs) | 0xc0000409 | 0x0 | 复测通过，参考侧不稳定 |
| [expression/assignment/08.tjs](../../tests/tjs/expression/assignment/08.tjs) | 0xc0000005 | 0x0 | 复测通过，参考侧不稳定 |
| [expression/binary/04.tjs](../../tests/tjs/expression/binary/04.tjs) | 0xc0000005 | 0x0 | 复测通过，参考侧不稳定 |
| [expression/containers/02.tjs](../../tests/tjs/expression/containers/02.tjs) | 0xc0000005 | 0xc0000005 | 参考侧运行错误或崩溃，需稳定环境归因 |
| [expression/containers/05.tjs](../../tests/tjs/expression/containers/05.tjs) | 0xc0000409 | 0xc0000005 | 参考侧报告语法错误 |
| [expression/conversion/01.tjs](../../tests/tjs/expression/conversion/01.tjs) | 0xc0000409 | 0xc0000005 | 参考侧报告语法错误 |
| [expression/conversion/03.tjs](../../tests/tjs/expression/conversion/03.tjs) | 0xc0000005 | 0x0 | 复测通过，参考侧不稳定 |
| [expression/function-expression/01.tjs](../../tests/tjs/expression/function-expression/01.tjs) | 0xc0000005 | 0xc0000005 | 参考侧报告语法错误 |
| [expression/function-expression/04.tjs](../../tests/tjs/expression/function-expression/04.tjs) | 0xc0000409 | 0x0 | 复测通过，参考侧不稳定 |
| [expression/function-expression/06.tjs](../../tests/tjs/expression/function-expression/06.tjs) | 0xc0000005 | 0xc0000409 | 参考侧运行错误或崩溃，需稳定环境归因 |
| [expression/function-expression/07.tjs](../../tests/tjs/expression/function-expression/07.tjs) | 0xc0000005 | 0x0 | 复测通过，参考侧不稳定 |
| [expression/instanceof-primitive.tjs](../../tests/tjs/expression/instanceof-primitive.tjs) | 0xc0000409 | 0xc0000005 | 参考侧报告语法错误 |
| [expression/interpolation/01.tjs](../../tests/tjs/expression/interpolation/01.tjs) | 0xc0000005 | 0x0 | 复测通过，参考侧不稳定 |
| [expression/interpolation/04.tjs](../../tests/tjs/expression/interpolation/04.tjs) | 0xc0000005 | 0xc0000005 | 参考侧报告语法错误 |
| [expression/interpolation/05.tjs](../../tests/tjs/expression/interpolation/05.tjs) | 0xc0000005 | 0xc0000005 | 参考侧运行错误或崩溃，需稳定环境归因 |
| [expression/literals/03.tjs](../../tests/tjs/expression/literals/03.tjs) | 0xc0000409 | 0xc0000409 | 参考侧报告语法错误 |
| [expression/prefix-delete/02.tjs](../../tests/tjs/expression/prefix-delete/02.tjs) | 0xc0000005 | 0xc0000005 | 参考侧运行错误或崩溃，需稳定环境归因 |
| [expression/prefix-delete/03.tjs](../../tests/tjs/expression/prefix-delete/03.tjs) | 0xc0000409 | 0x0 | 复测通过，参考侧不稳定 |
| [expression/template-string/01.tjs](../../tests/tjs/expression/template-string/01.tjs) | 0xc0000005 | 0x0 | 复测通过，参考侧不稳定 |
| [expression/unary/02.tjs](../../tests/tjs/expression/unary/02.tjs) | 0xc0000409 | 0xc0000409 | 参考侧报告语法错误 |
| [expression/void-addition.tjs](../../tests/tjs/expression/void-addition.tjs) | 0xc0000005 | 0xc0000005 | 参考侧运行错误或崩溃，需稳定环境归因 |
| [function/body/01.tjs](../../tests/tjs/function/body/01.tjs) | 0xc0000409 | 0xc0000005 | 参考侧报告语法错误 |
| [function/variadic/02.tjs](../../tests/tjs/function/variadic/02.tjs) | 0xc0000374 | 0xc0000374 | 输出通过标记后异常退出 |
| [integration/containers.tjs](../../tests/tjs/integration/containers.tjs) | 0xc0000409 | 0xc0000409 | 参考侧运行错误或崩溃，需稳定环境归因 |
| [judgment/switch/03.tjs](../../tests/tjs/judgment/switch/03.tjs) | 0xc0000005 | 0x0 | 复测通过，参考侧不稳定 |
| [judgment/switch/07.tjs](../../tests/tjs/judgment/switch/07.tjs) | 0xc0000005 | 0xc0000005 | 参考侧运行错误或崩溃，需稳定环境归因 |
| [judgment/switch/08.tjs](../../tests/tjs/judgment/switch/08.tjs) | 0xc0000005 | 0xc0000005 | 参考侧报告语法错误 |
| [loop/const/01.tjs](../../tests/tjs/loop/const/01.tjs) | 0xc0000409 | 0x0 | 复测通过，参考侧不稳定 |
| [loop/iteration/04.tjs](../../tests/tjs/loop/iteration/04.tjs) | 0xc0000005 | 0x0 | 复测通过，参考侧不稳定 |
| [loop/iteration/05.tjs](../../tests/tjs/loop/iteration/05.tjs) | 0xc0000409 | 0x0 | 复测通过，参考侧不稳定 |
| [loop/iteration/07.tjs](../../tests/tjs/loop/iteration/07.tjs) | 0xc0000409 | 0x0 | 复测通过，参考侧不稳定 |
| [native-function/containers/02.tjs](../../tests/tjs/native-function/containers/02.tjs) | 0xc0000409 | 0xc0000409 | 参考侧运行错误或崩溃，需稳定环境归因 |
| [native-function/containers/03.tjs](../../tests/tjs/native-function/containers/03.tjs) | 0xc0000409 | 0xc0000409 | 参考侧运行错误或崩溃，需稳定环境归因 |
| [native-function/date/02.tjs](../../tests/tjs/native-function/date/02.tjs) | 0xc0000005 | 0x0 | 复测通过，参考侧不稳定 |
| [native-function/eval/04.tjs](../../tests/tjs/native-function/eval/04.tjs) | 0xc0000005 | 0xc0000005 | 参考侧运行错误或崩溃，需稳定环境归因 |
| [native-function/math/01.tjs](../../tests/tjs/native-function/math/01.tjs) | 0xc0000409 | 0x0 | 复测通过，参考侧不稳定 |
| [native-function/math/02.tjs](../../tests/tjs/native-function/math/02.tjs) | 0xc0000005 | 0xc0000005 | 参考侧运行错误或崩溃，需稳定环境归因 |
| [native-function/math/03.tjs](../../tests/tjs/native-function/math/03.tjs) | 0xc0000005 | 0x0 | 复测通过，参考侧不稳定 |
| [native-function/numeric-literals/01.tjs](../../tests/tjs/native-function/numeric-literals/01.tjs) | 0xc0000005 | 0xc0000005 | 参考侧报告语法错误 |
| [native-function/numeric-literals/02.tjs](../../tests/tjs/native-function/numeric-literals/02.tjs) | 0xc0000005 | 0x0 | 复测通过，参考侧不稳定 |
| [native-function/numeric-literals/03.tjs](../../tests/tjs/native-function/numeric-literals/03.tjs) | 0xc0000005 | 0xc0000005 | 参考侧运行错误或崩溃，需稳定环境归因 |
| [native-function/regexp/01.tjs](../../tests/tjs/native-function/regexp/01.tjs) | 0xc0000005 | 0xc0000005 | 参考侧运行错误或崩溃，需稳定环境归因 |
| [native-function/regexp/02.tjs](../../tests/tjs/native-function/regexp/02.tjs) | 0xc0000409 | 0xc0000409 | 参考侧运行错误或崩溃，需稳定环境归因 |
| [native-function/regexp/03.tjs](../../tests/tjs/native-function/regexp/03.tjs) | 0xc0000409 | 0xc0000409 | 参考侧运行错误或崩溃，需稳定环境归因 |
| [native-function/regexp/05.tjs](../../tests/tjs/native-function/regexp/05.tjs) | 0xc0000409 | 0xc0000374 | 输出通过标记后异常退出 |
| [native-function/regexp/07.tjs](../../tests/tjs/native-function/regexp/07.tjs) | 0xc0000005 | 0xc0000005 | 参考侧运行错误或崩溃，需稳定环境归因 |
| [native-function/regexp-state/01.tjs](../../tests/tjs/native-function/regexp-state/01.tjs) | 0xc0000005 | 0xc0000005 | 输出通过标记后异常退出 |
| [native-function/regexp-state/03.tjs](../../tests/tjs/native-function/regexp-state/03.tjs) | 0xc0000005 | 0xc0000409 | 参考侧报告语法错误 |
| [native-function/string/01.tjs](../../tests/tjs/native-function/string/01.tjs) | 0xc0000409 | 0x0 | 复测通过，参考侧不稳定 |
| [native-function/string/03.tjs](../../tests/tjs/native-function/string/03.tjs) | 0xc0000005 | 0xc0000005 | 参考侧运行错误或崩溃，需稳定环境归因 |
| [native-function/string/05.tjs](../../tests/tjs/native-function/string/05.tjs) | 0xc0000005 | 0xc0000409 | 参考侧报告语法错误 |
| [native-function/string/09.tjs](../../tests/tjs/native-function/string/09.tjs) | 0xc0000005 | 0xc0000005 | 参考侧报告语法错误 |
| [native-function/string-length.tjs](../../tests/tjs/native-function/string-length.tjs) | 0xc0000005 | 0xc0000005 | 参考侧运行错误或崩溃，需稳定环境归因 |
| [oop/member-lookup/01.tjs](../../tests/tjs/oop/member-lookup/01.tjs) | 0xc0000409 | 0xc0000409 | 参考侧报告语法错误 |
| [oop/member-lookup/02.tjs](../../tests/tjs/oop/member-lookup/02.tjs) | 0xc0000409 | 0xc0000005 | 参考侧运行错误或崩溃，需稳定环境归因 |
| [oop/member-lookup/03.tjs](../../tests/tjs/oop/member-lookup/03.tjs) | 0xc0000409 | 0xc0000409 | 参考侧运行错误或崩溃，需稳定环境归因 |
| [preprocessor/disabled.tjs](../../tests/tjs/preprocessor/disabled.tjs) | 0xc0000409 | 0x0 | 复测通过，参考侧不稳定 |
| [preprocessor/literals.tjs](../../tests/tjs/preprocessor/literals.tjs) | 0xc0000409 | 0x0 | 复测通过，参考侧不稳定 |
| [property/getter.tjs](../../tests/tjs/property/getter.tjs) | 0xc0000409 | 0x0 | 复测通过，参考侧不稳定 |
| [property/handler-exceptions.tjs](../../tests/tjs/property/handler-exceptions.tjs) | 0xc0000409 | ERROR | 参考侧报告语法错误 |
| [scope/block/01.tjs](../../tests/tjs/scope/block/01.tjs) | 0xc0000005 | 0x0 | 复测通过，参考侧不稳定 |
| [scope/block/07.tjs](../../tests/tjs/scope/block/07.tjs) | 0xc0000005 | 0x0 | 复测通过，参考侧不稳定 |
| [scope/block/08.tjs](../../tests/tjs/scope/block/08.tjs) | 0xc0000005 | 0xc0000005 | 参考侧运行错误或崩溃，需稳定环境归因 |
| [scope/class-context.tjs](../../tests/tjs/scope/class-context.tjs) | 0xc0000005 | 0xc0000409 | 参考侧运行错误或崩溃，需稳定环境归因 |
| [with/04.tjs](../../tests/tjs/with/04.tjs) | 0xc0000409 | 0xc0000409 | 参考侧报告语法错误 |
