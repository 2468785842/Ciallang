---
name: cial-diff
description: 与参考 TJS2 引擎（TJSCompiler）对拍脚本行为、字节码转译与反编译结果，登记语料身份与差异证据；验证语言兼容性或扩展对拍覆盖时使用。
---

# Ciallang 与参考 TJS2 对拍

兼容定位是可做 tjscompile 超集：允许接受 TJS2 拒绝的程序，发现比 TJS2 更严的缺口时
按需求评估，不为逐条对齐而修复（见根 [AGENTS.md](../../../AGENTS.md)）。

## 参考引擎可用性

- 本机参考仓库 `../TJSCompiler`；`d8044c3` 起提供 `TJSCompiler <script>` 与
  `--dump <script> <output>` CLI。不同版本入口能力需实际确认，指定 `CIAL_TJS_EXE` 前
  先确认程序确实执行输入脚本并支持所需导出接口，见
  [FAQ](../../../doc/faq.md#对拍前怎样确认参考引擎可用)。
- 参考引擎须接受脚本路径并提供 `Scripts.execStorage` 与 `Debug.message`。
  核对版本在该机仍有原生库与崩溃问题（如 RegExp 固定 UTF-16LE 而 `tjs_char` 为
  32 位 `wchar_t`），"文件能交给参考引擎运行"不表示参考侧全量通过；
  保留 JSON 报告，区分解释器行为、参考版本与平台差异。

## 对拍路径

- 脚本行为：`python tests/run_tjs_tests.py --engine <参考引擎> --report <out/ 下的 json>`，
  或配置 `-DCIAL_TJS_TEST_REFERENCE=<参考引擎>` 注册 `ReferenceTjs.*` 后经 CTest 运行。
- 字节码转译：`bench/bytecode_conformance.py --reference` 探测参考 CLI；
  已有源码与字节码 fixture 可独立使用，无需参考程序。命令与报告语义见
  [构建指南](../../../doc/building.md#测试与基准)与[转译文档](../../../doc/bytecode/mapping.md)。
- 反编译 round-trip：按[反编译方案 §7 四环](../../../doc/bytecode/decompiler-plan.md#7-验证闭环)
  逐批验收；语料与固定清单（参考二进制哈希、导出选项、pendingUnits）见
  `tests/test_files/tjsbc/debug/manifest.json`。
- 差异先分类再处理：语言行为差异（评估兼容定位）、参考引擎平台缺陷（记录并绕开）、
  语料身份不匹配（核对哈希与版本）。

## 交付证据

记录参考引擎路径/版本/哈希、语料清单、命令与退出码、逐样本状态与首个差异；
报告放 Git 忽略的 `out/`，长期结论写入对应 note 或 [兼容性说明](../../../doc/compatibility.md)，
注明参考版本与平台。未复现的差异不关闭；参考侧能力不以 README 声明为准。
