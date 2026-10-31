# 2026-10-31 TJS2 语义差异归因任务清单

状态：待处理。本文供在其他设备/会话继续修复使用，自包含全部复现材料。
前置阅读：[2026-10-31 参考侧复核](2026-10-31-reference-audit.md)、[tests/tjs/README.md](../../tests/tjs/README.md)。

## 1. 环境与工具（本节数据的测量条件）

- 本机：Windows x64；Ciallang 提交 `9d82caf`（诊断重构首批之后），Debug 构建
  `build/Windows/Debug/cial.exe`，本目录 226 个用例全数通过（CTest 232/232 含 C++ 侧）。
- 参考仓库 `../TJSCompiler` 提交 `40a736c`，二进制 `bin/Windows/TJSCompiler.exe`，
  SHA-256 `8f370f2bb7460c8b7b8090a8bc71b7d0fb94a25e98ee2ff20d01bf3df358c60b`
  （与 2026-10-31 审计同一二进制；注意 `bin/Windows/TJSCompiler-release.exe` 是另一个
  未审计构建，不要混用）。
- 对拍命令：

```sh
python tests/run_tjs_tests.py \
  --engine /path/to/TJSCompiler.exe --timeout 5 \
  --report build/tjs-reference-2026-10-31b.json
```

## 2. 本轮总览（2026-10-31 第二轮，226 文件）

| 结果 | 数量 |
|---|---:|
| 参考、本项目双侧通过 | 162 |
| 参考侧失败（本项目通过） | 64 |
| — 其中 3 次复跑 0/3 通过（稳定崩溃） | 24 |
| — 其中 3 次复跑部分通过（闪失） | 26 |
| — 其中 3 次复跑 3/3 通过（纯噪声） | 14 |
| — 首轮即已输出 `__TJS_TEST_PASS__` 后崩溃 | 4 |

64 个失败退出码全部为 Windows 异常码（`0xC0000005` 访问违例 / `0xC0000409` fastfail /
`0xC0000374` 堆损坏）；同一输入重复运行结果漂移（例：`scope/block/04.tjs` 三次
`0x7f,0,0`），复现脚本见第 4 节。原始数据：
`build/tjs-reference-2026-10-31b.json`（首轮）、`build/tjs-reference-retry3.json`（复跑）。

对 64 个失败逐个重复运行并检索输出，发现 **6 个文件存在带时间戳的真实断言失败**
（`00:00:00 assertion N failed:`），即真实语义差异候选；其余 58 个从未出现断言失败文本
（退出转储里的 `var failure = "assertion "` 是 assert.tjs 源码回显，勿误判）。

## 3. 六个语义差异候选（已最小化复现）

复现脚本统一形态（`$TEMP/diff/tN.tjs`，N 见下）：

```tjs
// 见各小节脚本原文；双侧执行：
# 参考（输出行带 "00:00:00 " 时间戳前缀）
/path/to/TJSCompiler.exe tN.tjs
# 本项目
build/Windows/Debug/cial.exe tN.tjs
```

### 3.1 `native-function/regexp/02.tjs`、`03.tjs`、`regexp-state/03.tjs` — RegExp `index` 恒为我方 2 倍

失败断言分别为 assertion 3 / 1 / 1（`g.index` 的期望值）。最小复现：

```tjs
// t4.tjs —— pattern /x(\d+)x/g, 目标 "a1x22x b x333x c"，匹配 "x22x" 于字符位 2
var g = /x(\d+)x/g;
var first = g.exec("a1x22x b x333x c");
Debug.message("T4 f0=" + first[0] + " f1=" + first[1] + " index=" + g.index + " lastIndex=" + g.lastIndex);
// t5.tjs —— /b/ exec "abcb"，匹配于字符位 1
var e = /b/;
Debug.message("T5 m0=" + e.exec("abcb")[0] + " index=" + e.index);
// t6.tjs —— /x(\d+)x/g exec "中文x22x b x333x"，码点位 2
var g = /x(\d+)x/g;
g.exec("中文x22x b x333x");
Debug.message("T6 i1=" + g.index + " li1=" + g.lastIndex);
```

实测：

| 例 | 参考 index | 我方 index | 参考 lastIndex | 我方 lastIndex |
|---|---:|---:|---:|---:|
| T4（ASCII，起始 2） | **4** | 2 | 6 | 6 |
| T5（ASCII，起始 1） | **2** | 1 | — | — |
| T6（CJK，码点位 2） | **4** | 2 | 6 | 6 |

判定线索：三例参考 `index` 恰为我方 **2 倍**（16 位字节的偏移），而其 `lastIndex`
又与字符位一致——同一对象两种单位不自洽；叠加 tests/tjs/README 已记录的参考缺陷
（RegExp 固定 UTF-16LE 而 `tjs_char` 为 32 位 `wchar_t`），**倾向参考侧缺陷，我方按
TJS2 字符位语义**。处理建议：
1. 在参考仓库 `src/syntax/library` 的 RegExp 实现中确认 index 的计算单位（预期找到
   ×2/字节偏移痕迹）作为最终证据；
2. 保持我方行为，在本文件与 [compatibility.md](../compatibility.md) 增补差异条目
   （沿用 §4 表格风格）；
3. 若确认要兼容参考输出（不建议），须经 `CIAL_TJS_TEST_REFERENCE` 全量对拍复核。

### 3.2 `native-function/eval/04.tjs` — 后缀 `!` 编译失败的可捕获性

失败断言 assertion 1。最小复现：

```tjs
// t3.tjs
var bad = "1 +";
var caught = 0;
try { bad!; } catch(ex) { caught = 1; }
Debug.message("T3 caught=" + caught);
```

参考输出 `T3 caught=0`，我方 `T3 caught=1`：参考的后缀 `!`（tjs.y `T_EVAL`）编译失败
**不被脚本 catch 捕获**（可能抛为原生错误直接终止或未抛）。处理建议：
1. 查参考 `src/syntax` 的 T_EVAL 执行路径与错误传播方式（`tjsInterCodeExec.cpp` 对应分支），
   确认其行为是"未实现可捕获"还是"语义如此"；
2. 以 TJS2 リファレンス 的 eval 异常章节裁决；若参考确属未实现，保持我方（与 kirkr2
   行为一致：编译失败抛 Exception 可捕获），并在 compatibility.md §4 增行。

### 3.3 `expression/containers/02.tjs`、`integration/containers.tjs` — 字典字面量与下标赋值

失败断言分别为 assertion 1（`%["k" => 1, num : 2].count == 2`）与 assertion 4
（`%["a"=>1,"b"=>2]; d["c"]=3; count == 3`）。最小复现：

```tjs
// t1.tjs
var d = %["k" => 1, num : 2];
Debug.message("T1 count=" + d.count + " k=" + d["k"] + " num=" + d.num);
// t2.tjs
var d = %["a" => 1, "b" => 2];
d["c"] = 3;
Debug.message("T2 count=" + d.count + " c=" + d["c"]);
```

参考侧 9/9 次执行无输出（崩溃），我方输出 `count=2 k=1 num=2` / `count=3 c=3`。
对拍全量轮中参考曾各出现一次执行到断言失败（count 与期望不符）的记录，即偶发不崩时
count 也不对。处理建议：
1. 用更稳定的参考构建（见第 5 节）重跑 t1/t2 取得非崩溃样本，确认参考 count 实际值
   （怀疑混合键 `sym:` 形式被忽略、或字面量后成员数按内部桶计数）；
2. 以参考 `tjsHashList`/inline_dic 生成代码裁决哪侧正确；确认前**不要**修改我方行为
   （我方与 TJS2 リファレンス 的字面量语义一致的概率高）。

## 4. 参考侧随机崩溃的证据脚本

```sh
R=/path/to/TJSCompiler.exe
for i in 1 2 3; do $R tests/tjs/scope/block/04.tjs >/dev/null 2>&1; printf "rc=%x " $?; done
# 观察到 rc=7f,0,0 一类漂移；native-function/string/02.tjs 为 0,崩,崩
```

退出码：`0xc0000005` 访问违例、`0xc0000409` fastfail（栈 cookie）、`0xc0000374` 堆损坏。
稳定性三分类明细在 `build/tjs-reference-retry3.json`（`retry_passes` 字段）。

## 5. 下一步（按优先级）

1. **取得稳定参考构建**（本清单绝大多数项的前置）：在参考仓库重建 Release/带 ASan 的
   Windows 构建，或定位其未初始化/堆错误（0xC0000409 集中在 `oop/member-lookup/*`、
   `scope/block/*`、`native-function/containers/*`，可作为入手文件集）；重建后必须重跑
   第 1 节命令并核对 SHA 记录进报告。
2. 用稳定构建裁决 §3.3 两个容器项与 §3.1 的 index 单位证据（查参考源码）。
3. §3.2 按参考源码 + リファレンス裁决，落 compatibility.md 差异条目。
4. 全部裁决后：将确认的我方修复项拆独立提交；参考侧缺陷项只记录不追仿；
   更新 [TODO](../../TODO.md) 与 [2026-10-31 审计报告](2026-10-31-reference-audit.md)。
5. 验收：`python tests/run_tjs_tests.py --engine <新参考> --report ...` 全量对拍，
   差异逐项归因到本清单编号或新条目；CTest 全绿。
