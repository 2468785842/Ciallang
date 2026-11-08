"""反编译接口、表达式、声明和行表回归。"""
import json

from support import CliTestCase, TJSBC_CORPUS, run_tests


class DecompileTest(CliTestCase):
    def test_anonymous_function_assignment_roundtrip(self):
        self.script.write_text(
            'var calls=0; var o=%[]; '
            'o.m=function(x){calls++;return x+1;}; '
            'println(o.m(2)); println(o.m(4)); println(calls);', encoding="utf-8")
        recovered = self.run_cial("--decompile", self.script)
        self.assertEqual(recovered.returncode, 0, recovered.stderr)
        target = self.root / "anonymous-assignment.tjs"
        target.write_text(recovered.stdout, encoding="utf-8")
        for source in (self.script, target):
            result = self.run_cial(source)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout, "3\n5\n2\n")

    def test_decompile_options_and_strict_mode(self):
        # 互斥与从属校验
        for flags in [("--decompile", "--tjsbc", self.script),
                      ("--decompile", "--time", self.script),
                      ("--view", self.script), ("--map", "m.json", self.script),
                      ("--source", "orig.tjs", self.script)]:
            with self.subTest(flags=flags):
                process = self.run_cial(*flags)
                self.assertEqual(process.returncode, 2, process.stderr)
                self.assertEqual(process.stdout, "")
        # 自产路径：直线表达式严格模式恢复成功（M2），退出码 0、产出可重编译文本
        process = self.run_cial("--decompile", self.script)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertIn("println(42);", process.stdout)
        self.assertNotIn("//   @", process.stdout)
        # 语句结构未落地（M5 try）：严格模式 Unsupported，退出码 2、不产出文本、逐单元诊断
        structured = self.root / "structured.tjs"
        structured.write_text("try { throw 1; } catch (e) { println(e); }", encoding="utf-8")
        process = self.run_cial("--decompile", structured)
        self.assertEqual(process.returncode, 2, process.stderr)
        self.assertEqual(process.stdout, "")
        self.assertIn("decompile unsupported", process.stderr)
        # 诊断视图：输出含行号与 TAC 反汇编，头部标注不可重编译，恢复结果以注释附加
        process = self.run_cial("--decompile", "--view", self.script)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertIn("not recompilable", process.stdout)
        self.assertIn("@0", process.stdout)
        self.assertIn("//   >> println(42);", process.stdout)
        # 行表 JSON：状态与行坐标
        mapPath = self.root / "map.json"
        process = self.run_cial("--decompile", "--view", "--map", mapPath, self.script)
        self.assertEqual(process.returncode, 0, process.stderr)
        payload = json.loads(mapPath.read_text(encoding="utf-8"))
        self.assertEqual(payload["status"], "unsupported")
        self.assertTrue(payload["units"])
        # TJS2 路径：语料反汇编视图 + --source 行号换算；非法输入拒绝
        corpus = TJSBC_CORPUS
        sample = corpus / "s6_class.tjsbc"
        process = self.run_cial("--decompile", "--view", sample)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertIn("context", process.stdout)
        source = corpus / "s6_class.tjs"
        process = self.run_cial("--decompile", "--view", "--source", source, sample)
        self.assertEqual(process.returncode, 0, process.stderr)
        garbage = self.root / "garbage.bin"
        garbage.write_bytes(b"\x01\x02\x03\x04 not a script or bytecode")
        process = self.run_cial("--decompile", garbage)
        self.assertNotEqual(process.returncode, 0)

    def test_decompile_outputdebug_source_lines(self):
        # 真实 outputdebug 产物（manifest 固定参考二进制 / 导出选项 / 样本哈希）经
        # -- source 做偏移→行号换算的端到端；合成用例在 decompile_test.cpp
        corpus = TJSBC_CORPUS / "debug"
        manifest = json.loads((corpus / "manifest.json").read_text(encoding="utf-8"))
        import hashlib

        for sample in manifest["samples"]:
            source = corpus / sample.get("source", f"{sample['name']}.tjs")
            bytecode = corpus / f"{sample['name']}.tjsbc"
            map_path = self.root / f"{sample['name']}_map.json"
            with self.subTest(sample=sample["name"]):
                for name, field in [("source", source), ("bytecode", bytecode)]:
                    digest = hashlib.sha256(field.read_bytes()).hexdigest()
                    self.assertEqual(digest, sample[f"{name}Sha256"], name)
                process = self.run_cial("--decompile", "--view", "--source", source,
                                        "--map", map_path, bytecode)
                self.assertEqual(process.returncode, 0, process.stderr)
                payload = json.loads(map_path.read_text(encoding="utf-8"))
                self.assertEqual(payload["status"], "unsupported")
        # 行号落在源文件行数内（0 基；末条目可指向末尾之后，由换算收敛）
                line_count = source.read_text(encoding="utf-8").count("\n") + 1
                # 同名类方法是不同单元，不能让后一个 describe 覆盖前一个。
                units = {}
                for unit in payload["units"]:
                    units.setdefault(unit["name"], []).extend(row.get("srcLine") for row in unit["lines"])
                for name, lines in units.items():
                    for line in lines:
                        if line is not None:
                            self.assertGreaterEqual(line, 0, name)
                            self.assertLess(line, line_count, f"{sample['name']}/{name}")
                for name, expected in sample["expectedLines"].items():
                    self.assertIn(name, units, f"{sample['name']}/{name}")
                    for line in expected:
                        self.assertIn(line, units[name], f"{sample['name']}/{name}")
        # 无 outputdebug 表的既有语料不携带 srcLine
        plain_map = self.root / "plain_map.json"
        process = self.run_cial("--decompile", "--view", "--source", corpus.parent / "s6_class.tjs",
                                "--map", plain_map, corpus.parent / "s6_class.tjsbc")
        self.assertEqual(process.returncode, 0, process.stderr)
        payload = json.loads(plain_map.read_text(encoding="utf-8"))
        self.assertFalse(any(row.get("srcLine") is not None
                             for unit in payload["units"] for row in unit["lines"]))

    def test_decompile_expression_roundtrip(self):
        # 环①（方案 §7）：直线表达式程序 源码执行 vs 严格反编译再编译执行，逐形态
        # 对拍可观察输出。覆盖字面量 / 算术 / 位 / 比较 / 逻辑 / 一元 / 成员 / 下标 / 调用 / 字面量
        # 构造（含折叠常量容器） / delete / 物化；每条独立成程序避免形态互相掩盖
        cases = [
        # 字面量与算术优先级（含括号、整除、取模、实数）
            'var a = 1 + 2 * 3; var b = (1 + 2) * 3; var c = 10 \\ 3; var d = 10 % 3;\n'
            'var e = 2.5 * 2; var f = 7 - (3 - 1); var g = 2 * 3 % 4;\n'
            'println(a); println(b); println(c); println(d); println(e); println(f); println(g);\n',
        # 位运算与移位
            'var x = 6 & 3 | 8 ^ 1; var y = 1 << 4 >> 1; var z = -16 >>> 2; var w = (6 | 1) & 3;\n'
            'println(x); println(y); println(z); println(w);\n',
        # 比较与逻辑（非短路双求值）
            'var p = 3 < 5 && 5 <= 5 || 1 > 2; var q = 1 == 1 != 0; var r = "1" === 1;\n'
            'var s = (1 || 0) && 0; var t = 1 !== 1;\n'
            'println(p); println(q); println(r); println(s); println(t);\n',
        # 一元族：负号、逻辑非、按位非（降糖 ^ -1）、typeof、转换、 # / $
            'var s = "abc"; println(-5); println(!0); println(~5); println(typeof s);\n'
            'println(int 2.9); println(real 3); println(string 42); println(#s); println($65);\n'
            'println(-(1 + 2)); println(!(1 && 0)); println(-s.length);\n',
        # 字符串转义与拼接
            'println("a\\"b\\\\c\\n" + "d"); println("x" + 1 + 2.5); println("" + (1 + 2));\n',
        # 数组 / 字典字面量（动态与折叠常量、嵌套）、下标读写、成员读写
            'var arr = [1, "s", 2.5]; arr[1] = "t"; println(arr[1]); println(arr.count);\n'
            'var d = %["k" => 1, "z" => "v"]; d.k = d.k + 4; println(d.k); println(d["z"]);\n'
            'var n = [[1, 2], %["a" => [3]]]; println(n[0][1]); println(n[1].a[0]);\n'
            'var fold = [1, 2, 3]; println(fold[2]); var fd = %["p" => 9]; println(fd.p);\n',
        # 方法调用、内置对象、链式成员
            'var arr = [3, 1, 2]; arr.sort(); println(arr.join(",")); println(Math.abs(-3));\n'
            'println("xyz".length); println([1, 2].join("-")); println(arr.count + 1);\n',
        # delete 语句形态与值形态（dst 被消费）
            'var dd = %["a" => 1, "b" => 2]; delete dd.a; println(dd.b);\n'
            'var ok = delete dd.b; println(ok); println(dd.count);\n',
        # 顶层多使用纯读复制（同一值分发到多处）
            'var k = 5; var m = k + k; var o = [k, k]; println(m); println(o[1]);\n',
        # 顶层 getter/调用混合链（临时值跨调用同树内联，落点传播）
            'class C { var a; var b; function m() { return 3; } }\n'
            'var o = C(); o.a = 1; o.b = 2;\n'
            'var r = o.a + o.b + o.m(); println(r);\n',
        # 顶层裸表达式语句混合链
            'class C { var a; var b; function m() { return 4; } }\n'
            'var o = C(); o.a = 2; o.b = 3;\n'
            'o.a + o.b + o.m(); println("done");\n',
        # 类体逗号字段声明（逐名注册，恢复为逐行 var）
            'class C { var a = 1, b = 2; function m() { return a + b; } }\n'
            'var o = C(); println(o.m());\n',
        ]
        for index, source in enumerate(cases):
            with self.subTest(case=index):
                original = self.root / f"rt{index}.tjs"
                original.write_text(source, encoding="utf-8")
                expected = self.run_cial(original)
                self.assertEqual(expected.returncode, 0, expected.stderr)
                decompiled = self.run_cial("--decompile", original)
                self.assertEqual(decompiled.returncode, 0, decompiled.stderr)
        # 严格模式产物不含反汇编行
                self.assertNotIn("//   @", decompiled.stdout)
                recompiled = self.root / f"rt{index}_rt.tjs"
                recompiled.write_text(decompiled.stdout, encoding="utf-8")
                actual = self.run_cial(recompiled)
                self.assertEqual(actual.returncode, 0, actual.stderr + "\n--- decompiled ---\n" + decompiled.stdout)
                self.assertEqual(actual.stdout, expected.stdout, "--- decompiled ---\n" + decompiled.stdout)

    def test_decompile_reflects_prefix_unary_precedence(self):
        # 前缀一元紧于乘除、松于后缀（tjs.y unary_expr）： - a * 2 发射 ChgSign 再 Mul，
        # 反编译按 TAC 结构不加括号；显式 - (a * 2) 与 int o.b 保持原形
        self.script.write_text('var a = 7; var o = %["b" => 3];\n'
                               'println(-a * 2); println(-(a * 2)); println(int o.b + 1);\n',
                               encoding="utf-8")
        process = self.run_cial("--decompile", self.script)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertIn("println(-a * 2);", process.stdout)
        self.assertIn("println(-(a * 2));", process.stdout)
        self.assertIn("println(int o.b + 1);", process.stdout)

    def test_decompile_class_comma_fields_recovered(self):
        # 类体 var a, b; 逗号链：解析逐名登记（Parser next 链），壳按成员表逐行恢复
        self.script.write_text('class C { var a, b; }\nvar o = C();\n', encoding="utf-8")
        process = self.run_cial("--decompile", self.script)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertIn("var a;", process.stdout)
        self.assertIn("var b;", process.stdout)

    def test_decompile_strict_rejects_structure_with_batch_reason(self):
        # 语句结构（try 属 M5、with 属 M3）在严格模式如实拒绝：退出码 2、不产文本、原因含批次号
        for source, batch in [("try { throw 1; } catch (e) { println(e); }", "M5"),
                              ("var o = %[];\nwith (o) { println(1); }\n", "M3")]:
            with self.subTest(batch=batch):
                self.script.write_text(source, encoding="utf-8")
                process = self.run_cial("--decompile", self.script)
                self.assertEqual(process.returncode, 2, process.stderr)
                self.assertEqual(process.stdout, "")
                self.assertIn(batch, process.stderr)

    def test_exception_regions_roundtrip(self):
        corpus = TJSBC_CORPUS / "decompile"
        manifest = json.loads((corpus / "manifest.json").read_text(encoding="utf-8"))
        samples = [sample for sample in manifest["samples"] if sample["name"].startswith("m5_exception_")]
        self.assertTrue(samples)
        import hashlib
        for sample in samples:
            with self.subTest(sample=sample["name"]):
                source = corpus / (sample["name"] + ".tjs")
                for name, field in (("source", source), ("bytecode", source.with_suffix(".tjsbc"))):
                    digest = hashlib.sha256(field.read_bytes()).hexdigest()
                    self.assertEqual(digest, sample[f"{name}Sha256"], sample["name"])
                original = self.run_cial(source)
                self.assertEqual(original.returncode, 0, original.stderr)
                translated = self.run_cial("--tjsbc", source.with_suffix(".tjsbc"))
                self.assertEqual(translated.returncode, 0, translated.stderr)
                self.assertEqual(translated.stdout, original.stdout)
                for input_path in (source, source.with_suffix(".tjsbc")):
                    recovered = self.run_cial("--decompile", input_path)
                    self.assertEqual(recovered.returncode, 0, recovered.stderr)
                    self.assertIn("catch (", recovered.stdout)
                    if sample.get("structured") and input_path.suffix == ".tjsbc":
                        # structured 标记描述参考字节码恢复：真 try/catch 输出，
                        # 不得退化为状态机平铺（自产源码路径的具名局部按设计回退）
                        self.assertNotIn("while (true)", recovered.stdout, sample["name"])
                    target = self.root / "exception-roundtrip.tjs"
                    target.write_text(recovered.stdout, encoding="utf-8")
                    replay = self.run_cial(target)
                    self.assertEqual(replay.returncode, 0, replay.stderr)
                    self.assertEqual(replay.stdout, original.stdout)

    def test_super_prologue_roundtrip(self):
        # M4 移交验收：派生构造器内的基类构造调用（prologue）恢复为可重编译文本
        source = (
            'var log = "";\n'
            'class B {\n'
            '    var v;\n'
            '    function B(n) {\n'
            '        v = n;\n'
            '        log += "B" + n + ";";\n'
            '    }\n'
            '}\n'
            'class D extends B {\n'
            '    var w;\n'
            '    function D() {\n'
            '        B(7);\n'
            '        w = 9;\n'
            '        log += "D;";\n'
            '    }\n'
            '}\n'
            'var o = new D();\n'
            'println(o.v);\n'
            'println(o.w);\n'
            'println(log);\n')
        self.script.write_text(source, encoding="utf-8")
        expected = self.run_cial(self.script)
        self.assertEqual(expected.returncode, 0, expected.stderr)
        decompiled = self.run_cial("--decompile", self.script)
        self.assertEqual(decompiled.returncode, 0, decompiled.stderr)
        self.assertIn("class D extends B {", decompiled.stdout)
        self.assertIn("B(7);", decompiled.stdout)
        recompiled = self.root / "super_rt.tjs"
        recompiled.write_text(decompiled.stdout, encoding="utf-8")
        actual = self.run_cial(recompiled)
        self.assertEqual(actual.returncode, 0, actual.stderr + "\n--- decompiled ---\n" + decompiled.stdout)
        self.assertEqual(actual.stdout, expected.stdout)

    def test_deref_property_bc_recovery(self):
        # M4 移交验收：property 引用（&o.x + *p 读写）的参考字节码恢复；
        # 解引用操作数按版本槽渲染（物化临时不感知缺陷已修）
        corpus = TJSBC_CORPUS / "decompile"
        source = corpus / "m5_deref_property.tjs"
        original = self.run_cial(source)
        self.assertEqual(original.returncode, 0, original.stderr)
        recovered = self.run_cial("--decompile", source.with_suffix(".tjsbc"))
        self.assertEqual(recovered.returncode, 0, recovered.stderr)
        self.assertIn("*(", recovered.stdout)
        target = self.root / "deref-recovery.tjs"
        target.write_text(recovered.stdout, encoding="utf-8")
        replay = self.run_cial(target)
        self.assertEqual(replay.returncode, 0, replay.stderr)
        self.assertEqual(replay.stdout, original.stdout)

    def test_reference_function_expressions(self):
        corpus = TJSBC_CORPUS / "decompile"
        for name, expected in (("m5_funclit_assignment", "3\n5\n2\n"), ("m5_funclit_argument", "5\n")):
            with self.subTest(sample=name):
                source = corpus / (name + ".tjs")
                recovered = self.run_cial("--decompile", source.with_suffix(".tjsbc"))
                self.assertEqual(recovered.returncode, 0, recovered.stderr)
                self.assertIn("function (", recovered.stdout)
                target = self.root / "function-expression.tjs"
                target.write_text(recovered.stdout, encoding="utf-8")
                for arguments in ((source,), ("--tjsbc", source.with_suffix(".tjsbc")), (target,)):
                    result = self.run_cial(*arguments)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertEqual(result.stdout, expected)

    def test_decompile_reference_flag_conditions(self):
        corpus = TJSBC_CORPUS / "decompile"
        source = corpus / "m5_flag_conditions.tjs"
        bytecode = source.with_suffix(".tjsbc")
        original = self.run_cial(source)
        translated = self.run_cial("--tjsbc", bytecode)
        self.assertEqual(original.returncode, 0, original.stderr)
        self.assertEqual(translated.returncode, 0, translated.stderr)
        self.assertEqual(translated.stdout, original.stdout)
        restored = self.run_cial("--decompile", bytecode)
        self.assertEqual(restored.returncode, 0, restored.stderr)
        self.assertIn(" === ", restored.stdout)
        target = self.root / "restored-flags.tjs"
        target.write_text(restored.stdout, encoding="utf-8")
        replay = self.run_cial(target)
        self.assertEqual(replay.returncode, 0, replay.stderr)
        self.assertEqual(replay.stdout, original.stdout)

    def test_decompile_reference_local_return(self):
        corpus = TJSBC_CORPUS / "decompile"
        source = corpus / "m5_return_local.tjs"
        original = self.run_cial(source)
        translated = self.run_cial("--tjsbc", source.with_suffix(".tjsbc"))
        restored = self.run_cial("--decompile", source.with_suffix(".tjsbc"))
        self.assertEqual(restored.returncode, 0, restored.stderr)
        target = self.root / "return-local.tjs"
        target.write_text(restored.stdout, encoding="utf-8")
        replay = self.run_cial(target)
        for result in (original, translated, replay):
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout, "6\n5\n-1\n")

    def test_decompile_declaration_roundtrip(self):
        # M4 壳组装：函数/类（继承、字段初始化顺序）/property 声明恢复为可重编译文本；
        # 环①逐输入执行比对（含 getter 调用计数不翻倍）
        source = (
            'var getterCalls = 0;\n'
            'function add(a, b) { return a + b; }\n'
            'class Box {\n'
            '    var v = 10;\n'
            '    var w = 2;\n'
            '    function Box() { v += 5; }\n'
            '    function total() { return v * w + add(0, 1); }\n'
            '}\n'
            'class Labeled extends Box {\n'
            '    function total() { return super.total() + 1; }\n'
            '}\n'
            'property p {\n'
            '    setter(x) { getterCalls = x; }\n'
            '    getter { getterCalls += 1; return getterCalls; }\n'
            '}\n'
            'var b = new Box();\n'
            'println(b.total());\n'
            'var lb = new Labeled();\n'
            'println(lb.total());\n'
            'p = 7;\n'
            'println(p);\n'
            'println(p);\n')
        self.script.write_text(source, encoding="utf-8")
        expected = self.run_cial(self.script)
        self.assertEqual(expected.returncode, 0, expected.stderr)
        decompiled = self.run_cial("--decompile", self.script)
        self.assertEqual(decompiled.returncode, 0, decompiled.stderr)
        self.assertNotIn("//   @", decompiled.stdout)
        self.assertIn("class Box {", decompiled.stdout)
        self.assertIn("class Labeled extends Box {", decompiled.stdout)
        self.assertIn("property p {", decompiled.stdout)
        # property 安装占据全局名：后续赋值不再补 var（行为等价但带 var 是多余前缀）
        self.assertNotIn("var p = 7;", decompiled.stdout)
        recompiled = self.root / "decl_rt.tjs"
        recompiled.write_text(decompiled.stdout, encoding="utf-8")
        actual = self.run_cial(recompiled)
        self.assertEqual(actual.returncode, 0, actual.stderr + "\n--- decompiled ---\n" + decompiled.stdout)
        self.assertEqual(actual.stdout, expected.stdout, "--- decompiled ---\n" + decompiled.stdout)

    def test_decompile_m4_compound_and_literal_roundtrip(self):
        # M4 收尾：全局/解引用目标的 +=/++ 逆融合、类成员源码序、实参位置匿名函数
        # 字面量；环①逐输入执行比对
        source = (
            'var g = 10;\n'
            'class Mix {\n'
            '    function early() { return 1; }\n'
            '    var late = 2;\n'
            '}\n'
            'function apply(f) { return f(); }\n'
            'function bump() {\n'
            '    g += 5;\n'
            '    g++;\n'
            '    return g;\n'
            '}\n'
            'property flag {\n'
            '    setter(x) { hidden = x; }\n'
            '    getter { return hidden; }\n'
            '}\n'
            'var probe = new Mix();\n'
            'probe.late += 3;\n'
            'probe.early();\n'
            'flag = 1;\n'
            'flag++;\n'
            'var lit = apply(function() { return 42; });\n'
            'if (bump() != 16) { println("bump-wrong"); }\n'
            'if (probe.late != 5) { println("late-wrong"); }\n'
            'if (flag != 2) { println("flag-wrong"); }\n'
            'if (lit != 42) { println("lit-wrong"); }\n'
            'println(g);\n'
            'println(probe.late);\n'
            'println(flag);\n'
            'println(lit);\n')
        self.script.write_text(source, encoding="utf-8")
        expected = self.run_cial(self.script)
        self.assertEqual(expected.returncode, 0, expected.stderr)
        decompiled = self.run_cial("--decompile", self.script)
        self.assertEqual(decompiled.returncode, 0, decompiled.stderr)
        self.assertNotIn("//   @", decompiled.stdout)
        # 逆融合写法还原（Top-level 与函数体内的全局目标）
        self.assertIn("g += 5;", decompiled.stdout)
        self.assertIn("g++;", decompiled.stdout)
        self.assertIn("flag++;", decompiled.stdout)
        # 类成员按源码序（方法注册不再前移）
        self.assertLess(decompiled.stdout.index("function early()"), decompiled.stdout.index("var late"))
        # 实参位置匿名函数字面量
        self.assertIn("apply(function () {", decompiled.stdout)
        recompiled = self.root / "m4_rt.tjs"
        recompiled.write_text(decompiled.stdout, encoding="utf-8")
        actual = self.run_cial(recompiled)
        self.assertEqual(actual.returncode, 0, actual.stderr + "\n--- decompiled ---\n" + decompiled.stdout)
        self.assertEqual(actual.stdout, expected.stdout, "--- decompiled ---\n" + decompiled.stdout)

    def test_decompile_deref_compound_roundtrip(self):
        # *p 目标逆融合：property 引用读写两侧各持匿名拷贝（同源即同值），融合为
        # *= 形态且 setter 计数不变；环①执行比对
        source = (
            'var setCalls = 0;\n'
            'class Cell {\n'
            '    var m;\n'
            '    property x {\n'
            '        setter(v) { setCalls += 1; m = v; }\n'
            '        getter { return m; }\n'
            '    }\n'
            '}\n'
            'var o = new Cell();\n'
            'function bump() {\n'
            '    var p = &o.x;\n'
            '    *p += 3;\n'
            '    return *p;\n'
            '}\n'
            'if (bump() != 3) { println("bump-wrong"); }\n'
            'if (setCalls != 1) { println("setCalls-wrong"); }\n'
            'println(o.x);\n'
            'println(setCalls);\n')
        self.script.write_text(source, encoding="utf-8")
        expected = self.run_cial(self.script)
        self.assertEqual(expected.returncode, 0, expected.stderr)
        decompiled = self.run_cial("--decompile", self.script)
        self.assertEqual(decompiled.returncode, 0, decompiled.stderr)
        self.assertIn("*p += 3;", decompiled.stdout)
        recompiled = self.root / "deref_rt.tjs"
        recompiled.write_text(decompiled.stdout, encoding="utf-8")
        actual = self.run_cial(recompiled)
        self.assertEqual(actual.returncode, 0, actual.stderr + "\n--- decompiled ---\n" + decompiled.stdout)
        self.assertEqual(actual.stdout, expected.stdout, "--- decompiled ---\n" + decompiled.stdout)


if __name__ == "__main__":
    run_tests()
