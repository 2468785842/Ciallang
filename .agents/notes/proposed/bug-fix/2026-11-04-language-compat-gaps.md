# 语言兼容缺口（待评估）

Status: proposed

## 问题

两处已知与 TJS2 的行为差异，尚未评估是否修复：

1. `throw` 后接二目表达式解析失败：`throw "W" + x;` 报 "except a return value of
   expression"，而 `throw "W";` 合法；TJS2 语法中 throw 取表达式。
2. 嵌套函数自由变量解析失败：函数体内 `function inner(){ return a + 2; }` 引用外层
   局部 `a`，运行期报 "in global object Not found property"；嵌套作用域链未实现。

## 已知事实与证据

- 兼容定位见 [README](../../../../README.md)：允许做 tjscompile 超集，
  发现比 TJS2 更严的缺口时按需求评估，不为逐条对齐而修复。
- 反编译侧已可恢复嵌套函数形态，执行语义待补
  （见[反编译器 note](../architecture/2026-11-04-decompiler-m5.md)）。
- 复现入口：CLI 直接运行上述形态脚本；解析入口 `src/parser/Parser.cpp`。

## 建议下一步

逐条评估影响面（真实脚本语料出现频率、修复波及的解析/IRGen 范围），
确认要修再排批次；评估结论回写本 note 并同步[兼容性说明](../../../../doc/compatibility.md)。

## 完成条件

每条缺口有明确的"修复/不修复"决定及理由；决定修复的项另立实施批次，
断言进 `tests/tjs/` 对应目录并通过双引擎或单引擎验证。

## 风险或备选方案

嵌套作用域涉及 IRGen 变量解析与优化器，改动面大于 throw 表达式；
若暂不修复，在兼容性说明中如实记录差异即可。
