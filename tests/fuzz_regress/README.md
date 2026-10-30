# 前端回归语料

`.tjs` 文件包含曾触发问题或用于检查相邻边界的输入，允许语法无效。`tests/fuzz_regress_test.cpp` 同时用 lexer 和 parser 重放，要求安全返回；具体错误诊断和整数语义另由 `tests/preprocessor_test.cpp` 与 CLI 测试断言。

- `preprocessor-*-zero.tjs`：预处理除零、模除零。
- `class-invalid-name.tjs`、`function-invalid-name.tjs`：声明名不是标识符。
- `property-eof.tjs`：属性体在 EOF 处中断。
- `*-exponent-overflow.tjs`：指数累积超过 32 位整数。
- `nondecimal-integer-overflow.tjs`：超长非十进制整数。

fuzz 生成的大量覆盖率语料只放在构建目录；本目录保留可读、用途明确的回归输入。运行方式与资源限制见 [构建文档](../../doc/building.md#有界-fuzz)，本次结果见 [审查记录](../../doc/review.md#cloud-fuzz-增量验证2026-10-30)。
