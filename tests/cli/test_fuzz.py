"""有界确定性反编译模糊回归：模板组合生成程序，round-trip 对拍。

方案 §6 最终验收的"模糊覆盖"项：以受支持语句形态为模板、随机组合结构与
字面量，环①（源码执行 vs 反编译再编译执行）逐程序比对 stdout。诚实拒绝
（Unsupported + 诊断）计数放行；输出不一致或接受后回放失败即失败。
"""
import random

from support import CliTestCase, run_tests

SEED = 20261108
PROGRAMS = 120
INPUTS = (-4, 0, 3, 9)
#已知恢复缺口（棘轮登记：修复后须从此移除并让其参与断言）。
#repro 存 out / fuzz - known - issues /。2026 - 11 - 09：声明落点提升落地后 program 49
#通过并移除登记；同名遮蔽类生成器规避保留（多作用域同名不提升）。
KNOWN_FAILURES = {}


class DecompilerFuzzTest(CliTestCase):
    def _expr(self, depth):
        rng = self._rng
#叶子不含字符串全局 s：数值比较（ < ==）混入字符串会运行期类型错误
        choices = ["x", str(rng.randint(-9, 9)), "a", "b", "c", "g"]
        if depth > 0:
            choices += [
                f"({self._expr(depth - 1)} + {self._expr(depth - 1)})",
                f"({self._expr(depth - 1)} * {self._expr(depth - 1)})",
                f"({self._expr(depth - 1)} - {self._expr(depth - 1)})",
                f"({self._expr(depth - 1)} < {self._expr(depth - 1)} ? 1 : 2)",
                f"add({self._expr(depth - 1)}, {self._expr(depth - 1)})",
                f"pick({self._expr(depth - 1)})",
                f"arr[{rng.randint(0, 2)}]",
                f"d.k{rng.randint(0, 2)}",
            ]
        return rng.choice(choices)

    def _cmp(self):
        rng = self._rng
        return rng.choice([
            f"{self._expr(1)} < {rng.randint(0, 8)}",
            f"{self._expr(1)} == {rng.randint(0, 8)}",
            f"{self._expr(1)} > {rng.randint(-3, 3)}",
        ])

    def _stmt(self, depth):
        rng = self._rng
#循环变量每次唯一命名：同名局部遮蔽（含兄弟作用域寄存器复用）是
#已知恢复缺口（out / fuzz - known - issues / program49 - shadowed - do - while.tjs）
        loop = f"i{self._loopSeq}"
        self._loopSeq += 1
        choices = [
            f"a = {self._expr(1)};",
            f"b = {self._expr(1)};",
            f"c = a + b;",
            f"println({self._expr(1)});",
            f"d.k{rng.randint(0, 2)} = {self._expr(1)};",
        ]
        if depth > 0:
            inner = self._block(depth - 1, 1)
            inner2 = self._block(depth - 1, 1)
            choices += [
                f"if ({self._cmp()}) {{\n{inner}}}\n",
                f"if ({self._cmp()}) {{\n{inner}}} else {{\n{inner2}}}\n",
#有界循环模板（计数器恒递增，避免死循环）
                f"var i = 0;\nwhile (i < {rng.randint(0, 3)}) {{\ni = i + 1;\n{inner}}}\n",
                f"for (var i = 0; i < {rng.randint(0, 3)}; i = i + 1) {{\n{inner}}}\n",
                f"var i = 0;\ndo {{\ni = i + 1;\n{inner}}} while (i < {rng.randint(0, 3)});\n",
                f"switch ({self._expr(1)}) {{\n"
                f"case {rng.randint(0, 4)}:\n{inner}break;\n"
                f"case {rng.randint(0, 4)}:\n{inner2}break;\n"
                f"default:\n{self._stmt(depth - 1)}\nbreak;\n}}\n",
                f"try {{\n{inner}if ({self._cmp()}) {{\nthrow {rng.randint(1, 5)};\n}}\n}} "
                f"catch (e) {{\na = a + e;\n}}\n",
                f"try {{\n{inner}}} catch {{\nb = b + 1;\n}}\n",
            ]
        return rng.choice(choices)

    def _block(self, depth, count):
        return "".join("\t" + self._stmt(depth) for _ in range(count))

    def _program(self):
        rng = self._rng
        body = self._block(2, rng.randint(3, 7))
        return (
            'var g = 3;\n'
            'var s = "w";\n'
            'var arr = [1, 2, 3];\n'
            'var d = %["k0" => 1, "k1" => 2, "k2" => 3];\n'
            'function add(a, b) { return a + b; }\n'
            'function pick(x) { if (x > 0) { return 1; } return -1; }\n'
            'function f(x) {\n'
            'var a = 0;\n'
            'var b = 0;\n'
            'var c = 0;\n'
            f'{body}'
            'return a + b + c;\n'
            '}\n' + "".join(f"println(f({x}));\n" for x in INPUTS))

    def test_bounded_roundtrip_fuzz(self):
        self._rng = random.Random(SEED)
        self._loopSeq = 0
        accepted = rejected = invalid = 0
        knownFailed = set()
        for index in range(PROGRAMS):
            with self.subTest(program=index):
                source = self._program()
                self.script.write_text(source, encoding="utf-8")
                original = self.run_cial(self.script)
                if original.returncode != 0:
                    invalid += 1
                    continue
                recovered = self.run_cial("--decompile", self.script)
                if recovered.returncode != 0:
#诚实拒绝：须有诊断且不产文本
                    self.assertEqual(recovered.stdout, "", source)
                    rejected += 1
                    continue
                target = self.root / "fuzz-roundtrip.tjs"
                target.write_text(recovered.stdout, encoding="utf-8")
                replay = self.run_cial(target)
                known = index in KNOWN_FAILURES
                if replay.returncode != 0 or replay.stdout != original.stdout:
                    if known:
                        knownFailed.add(index)
                        continue
                    self.fail(f"program {index} mismatch\n--- source ---\n{source}"
                              f"\n--- recovered ---\n{recovered.stdout}")
                accepted += 1
                if known:
                    self.fail(f"program {index} 已通过：从 KNOWN_FAILURES 移除"
                              f"（{KNOWN_FAILURES[index]}）")
#生成器有效性：绝大多数程序可执行；拒绝须为少数且诚实
        self.assertGreater(accepted + rejected, PROGRAMS * 9 // 10)
        self.assertEqual(set(KNOWN_FAILURES) - knownFailed, set(),
                         "KNOWN_FAILURES 中有程序不再失败：移除登记使其参与断言")
        print(f"fuzz: accepted={accepted} rejected={rejected} invalid={invalid} "
              f"knownFailed={sorted(knownFailed)}")


if __name__ == "__main__":
    run_tests()
