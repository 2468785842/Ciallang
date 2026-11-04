# throw 语句操作数为整表达式

Status: implemented

## 问题

`throw "W" + x;` 解析失败（报 "except a return value of expression"），
而 `throw "W";` 合法；tjs.y 中 throw 取整表达式
（`throw : "throw" expr ";"`，expr 含 comma_expr 与后缀 if）。

## 已知事实与证据

- 根因：`throw` 此前挂在 `UnaryOperatorPrefixParser`（前缀运算符，绑定紧于二目），
  `throw "W" + x` 被读作 `(throw "W") + x`；throw 节点不产值
  （`IRGenerator::generate(PrefixUnaryExprNode)` 发射 `Throw` 后不设 optReg），
  二目取值即报错。
- 对拍（TJSCompiler `bin/Windows/TJSCompiler-release.exe`，2026-11-04）：
  `throw "W" + 1;` 参考侧编译执行正常；`var x = throw 1;` 参考侧语法错误。
  修复后两侧行为一致。

## 修复与验证

- `ThrowStmtParser` 语句路径（对齐 tjs.y）：整表达式操作数 + 分号；
  复用 `PrefixUnaryExprNode(throw)` + `ExprStmtNode` 装配，IRGenerator 原
  Throw 特判经空 optReg 正确发射，无新增 AST 节点。表达式位置 `throw`
  改为语法错误（对齐参考侧）。
- 回归：`tests/tjs/exceptions/throw-expression.tjs`（二目拼接/算术优先级/
  后缀 if 共 5 断言）；`tests/tjs/exceptions/` 既有 6 文件全过；
  round-trip：顶层 `throw "W" + 1 + "!";` 反编译文本逐字还原、
  双侧执行同抛 "W1!"（`out/m5-probe/throw_t.tjs`）。

## 风险与后续

`x = throw 1;` 由"可编译（语义未定义）"变为语法错误——与参考引擎一致，
属对齐性收紧；旧脚本若有此写法本就依赖未定义行为。
