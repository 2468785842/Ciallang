"""switch 源码及固定语料：每条输入只恢复一次，结构检查由 manifest 声明。"""
import hashlib
import json

from support import CliTestCase, TJSBC_CORPUS, run_tests


class SwitchTest(CliTestCase):
    def test_decompile_switch_roundtrip(self):
        # M5 自产派发链：求值次数、多值、贯穿、default与无匹配。
        sources = [
            'function f(x) { var r = ""; switch(x) { case 1: case 2: '
            'r += "a"; case 3: r += "b"; break; default: r += "z"; } return r; } '
            'println(f(1)); println(f(2)); println(f(3)); println(f(9));',
            'function f(x) { switch(x) { case 1: return 7; case 2: return 8; } '
            'return 9; } println(f(1)); println(f(2)); println(f(3));',
            'var calls = 0; function key() { calls += 1; return 2; } '
            'switch(key()) { case 1: println(1); break; case 2: println(2); '
            'break; default: println(3); } println(calls);',
            'var x = 1; switch(x) { case 1: break; } println(42);',
            'function f(x) { switch(x) { case 1: if(x > 0) { println(7); } '
            'else { println(8); } break; default: println(9); } } f(1); f(2);',
            'function f(x, c) { var r = 0; switch(x) { case 1: r += 1; break; '
            'default: if(c > 0) { r += 2; } else { r += 3; } break; } return r; } '
            'println(f(1, 5)); println(f(9, 5)); println(f(9, -5));',
            'function f(x, c) { var r = 0; switch(x) { case 1: if(c > 0) { r += 1; } '
            'if(c > 1) { r += 2; } r += 100; break; case 2: r += 7; break; } return r; } '
            'println(f(1, 2)); println(f(1, 1)); println(f(1, -1)); println(f(2, 0));',
            'function f(x, c) { var r = 0; switch(x) { case 1: if(c > 0) { r += 1; } '
            'else { r += 2; break; } r += 100; break; case 2: r += 7; break; } return r; } '
            'println(f(1, 1)); println(f(1, -1)); println(f(2, 0));',
            'var x = "a"; switch(x) { case "a": println(1); break; default: println(2); }',
            'function f(x) { switch(x) { case "a": case "b": println(1); break; '
            'case "c": println(2); break; default: println(3); } } '
            'f("a"); f("b"); f("c"); f("z");',
            'var i = 0; while(i < 4) { i += 1; switch(i) { case 2: continue; '
            'case 3: println(30); break; default: println(i); } println(99); }',
            'var i=0; do { i++; switch(i) { case 1: continue; case 2: println(20); '
            'break; default: println(i); } println(99); } while(i<3);',
        ]
        for source in sources:
            with self.subTest(source=source):
                self.script.write_text(source, encoding="utf-8")
                original = self.run_cial(self.script)
                self.assertEqual(original.returncode, 0, original.stderr)
                recovered = self.run_cial("--decompile", self.script)
                self.assertEqual(recovered.returncode, 0, recovered.stderr)
                self.assertIn("switch (", recovered.stdout)
                self.assertNotIn("[dbg]", recovered.stderr)
                target = self.root / "recovered.tjs"
                target.write_text(recovered.stdout, encoding="utf-8")
                replay = self.run_cial(target)
                self.assertEqual(replay.returncode, 0, replay.stderr)
                self.assertEqual(replay.stdout, original.stdout)


    def test_decompile_switch_for_register_carried_init_rejected(self):
        self.script.write_text(
            'for(var i=0; i<4; i++) { switch(i) { case 1: continue; '
            'case 2: println(20); break; default: println(i); } println(99); }',
            encoding="utf-8")
        recovered = self.run_cial("--decompile", self.script)
        self.assertEqual(recovered.returncode, 2, recovered.stderr)
        self.assertEqual(recovered.stdout, "")
        self.assertIn("循环携带变量", recovered.stderr)


    def test_switch_corpus(self):
        corpus = TJSBC_CORPUS / "decompile"
        manifest = json.loads((corpus / "manifest.json").read_text(encoding="utf-8"))
        samples = [s for s in manifest["samples"] if s["name"].startswith("m5_switch_")]
        self.assertTrue(samples, "switch corpus must not be empty")
        self.assertEqual(len(samples), len({s["name"] for s in samples}), "duplicate sample names")
        for sample in samples:
            with self.subTest(sample=sample["name"]):
                source = corpus / (sample["name"] + ".tjs")
                bytecode = source.with_suffix(".tjsbc")
                for path, field in ((source, "sourceSha256"), (bytecode, "bytecodeSha256")):
                    self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), sample[field], path.name)
                original = self.run_cial(source)
                self.assertEqual(original.returncode, 0, original.stderr)
                translated = self.run_cial("--tjsbc", bytecode)
                self.assertEqual(translated.returncode, 0, translated.stderr)
                self.assertEqual(translated.stdout, original.stdout)
                # Source success is independent of bytecode recovery: exercise it even for rejected BC.
                if "sourceChecks" in sample:
                    with self.subTest(input="source"):
                        self.assert_switch_recovery(source, original.stdout, sample["sourceChecks"])
                with self.subTest(input="bytecode"):
                    if sample["expectedRecovery"] == "unsupported":
                        recovered = self.run_cial("--decompile", bytecode)
                        self.assertEqual(recovered.returncode, 2, recovered.stderr)
                        self.assertEqual(recovered.stdout, "")
                        self.assertIn(sample.get("rejectReason", "寄存器合流"), recovered.stderr)
                    else:
                        self.assertEqual(sample["expectedRecovery"], "success")
                        self.assert_switch_recovery(bytecode, original.stdout, sample.get("bytecodeChecks", {}))

    def assert_switch_recovery(self, input_path, expected, checks):
        self.assertFalse(set(checks) - {"minSwitches", "contains", "entryLocals"}, "unknown recovery checks")
        recovered = self.run_cial("--decompile", input_path)
        self.assertEqual(recovered.returncode, 0, recovered.stderr)
        self.assertNotIn("[dbg]", recovered.stderr)
        self.assertGreaterEqual(recovered.stdout.count("switch ("), checks.get("minSwitches", 1))
        for fragment in checks.get("contains", []):
            self.assertIn(fragment, recovered.stdout)
        if checks.get("entryLocals"):
            # TJS2 case locals are scoped; shared storage must be declared at function entry.
            self.assertRegex(recovered.stdout, r"function n[(].*[)] [{]\s+var v\d+;")
        target = self.root / "recovered-switch.tjs"
        target.write_text(recovered.stdout, encoding="utf-8")
        replay = self.run_cial(target)
        self.assertEqual(replay.returncode, 0, replay.stderr)
        self.assertEqual(replay.stdout, expected, recovered.stdout)


if __name__ == "__main__":
    run_tests()
