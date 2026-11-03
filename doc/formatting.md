# cial-fmt 源码格式化

`cial-fmt` 是 TJS/Ciallang 源码格式化工具，随源码构建的 CLI 默认生成；已有的
`v0.1.0-preview.1` 压缩包不包含它。它不执行脚本，不求值预处理表达式，不删除条件分支。

## 使用

```sh
# 标准输入或单文件 → 标准输出；原文件不变
cial-fmt < startup.tjs
cial-fmt startup.tjs

# 就地格式化多个文件
cial-fmt --write startup.tjs lib/math.tjs
cial-fmt -i --indent-width 2 startup.tjs

# CI 检查：列出需要格式化的路径，不写回
cial-fmt --check startup.tjs lib/math.tjs

# 以 - 开头的文件名
cial-fmt --write -- -example.tjs
```

无文件参数或文件名 `-` 表示标准输入；`--write` 必须指定实际文件。多个文件必须使用
`--write` 或 `--check`，避免把多个程序拼接到标准输出。路径支持空格与中文。
`--indent-width N` / `--indent-width=N` 接受 1–16，默认 4 个空格；`--help` 查看参数，
`--version` 与 `cial` 共用 manifest 版本。

退出码：0 表示成功（检查时所有文件已经规范）；1 表示 `--check` 发现格式差异；
2 表示参数、读取、词法、结构或写入错误。诊断写入 stderr，行列从 1 开始，列按 UTF-8 字节计。

## 格式规则与保留范围

采用同行左花括号、块体缩进、运算符与逗号间距；区分 `for` 头的分号、前后缀运算符、
成员访问、类型注解及字典分隔符。无花括号的控制语句体换行缩进，switch 的 case 体再缩进一级。
保留表达式已有断行，多行数组/参数列表按层级缩进，最多保留一个空白行，非空输出以换行结束。
不按列宽自动折行，不补分号、排序声明或改写表达式。

字符串、正则、octet、模板（包含模板内部表达式）、注释正文及完整 `@if` 区域原样保留；
`@set` 只保留原文，不执行。条件区域内的语法错误可以保留，但仍需词法上能识别引号、注释及区域终点；
词法不完整、条件区域后 `/` 的正则/除法上下文不明确、跨区域括号不配对等情况会报错，不猜测改写。预处理区域内部不重新排版。

保留 UTF-8 BOM，外部空白的换行使用首个 LF/CRLF 的样式；字面量和注释内部换行不转换。
输入必须是合法 UTF-8，嵌套结构上限为 512 层，结构分析超过复杂度预算也会拒绝。格式化后重新扫描并核对词素类型和原始正文，
发现变化则拒绝输出；再次格式化应产生同样文本。它不代替完整语法/语义检查，后者使用 `cial --check-json`。

`--write` 先格式化全部输入；任一输入失败时不写回任何文件。需要修改的文件采用同目录排他临时文件、
完整写入、保留权限后替换；已经规范的文件不重写。拒绝写入符号链接，读入后发生内容变化则报错。
写盘失败会中止，不提供跨文件回滚事务。

## 构建与验证

```sh
cmake --preset "Linux Debug"
cmake --build --preset "Linux Debug" --target cial cial-fmt CiallangTests
ctest --test-dir build/Linux/Debug -R CiallangFormatter --output-on-failure --no-tests=error
```

Windows/macOS 使用对应预设，Windows 程序名为 `cial-fmt.exe`。`CIAL_BUILD_FORMATTER`
默认跟随 `CIAL_BUILD_CLI`；嵌入式库模式默认关闭。只需要 formatter 时可选择：

```sh
cmake -S . -B build/formatter -DCIAL_BUILD_CLI=OFF -DCIAL_BUILD_FORMATTER=ON -DCIAL_BUILD_TESTS=OFF
cmake --build build/formatter --target cial-fmt
cmake --install build/formatter --component Runtime --prefix /your/install/prefix
```

仍需核心前端的 fmt、frozen、utf8proc、Oniguruma，不需要 argparse。`CiallangFormat` 是
仓库内部工具目标，不进入安装 SDK 的兼容承诺。Runtime 打包包含启用的 formatter；
官方打包脚本显式启用它，并在解压后的精简环境中实际检查格式化输出。

端到端测试位于 `tests/formatter_test.py`，覆盖 CLI、字面量/预处理保留、幂等性、
中文路径、权限、错误不写回、运行结果对照，以及 `tests/tjs` 语料格式化前后重新编译。
源码使用 `src/parser/Lexer.*` 的真实词法规则；布局和命令行实现在 `src/format/`。
