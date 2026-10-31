"""Release version, archive boundary and installed CLI verification regressions."""
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from generate_version import generate, read_version

spec = importlib.util.spec_from_file_location('cial_package', Path(__file__).resolve().parents[1] / 'tools/package.py')
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


class PackagingTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='cial package test ')
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def archive(self, files):
        archive = self.root / 'package.zip'
        with zipfile.ZipFile(archive, 'w') as bundle:
            for name, data in files.items():
                bundle.writestr(name, data)
        return archive

    def test_source_version_and_tag_agree(self):
        version = package.project_version()
        package.validate_tag('v' + version, version)
        with self.assertRaises(ValueError):
            package.validate_tag('v99.0.0', version)

    def test_manifest_is_the_only_version_source(self):
        manifest = self.root / 'vcpkg.json'
        for version in ('0.1.0-preview.1', '2.3.4', '1.0.0-rc.2'):
            manifest.write_text(json.dumps({'version-string': version}))
            self.assertEqual(package.project_version(self.root), version)
        for version in (None, 123, '1.2', '01.2.3', '1.2.3-preview.01', '1.2.3-bad;set(X 1)'):
            with self.subTest(version=version):
                manifest.write_text(json.dumps({'version-string': version}))
                with self.assertRaises(ValueError):
                    read_version(manifest)
        for invalid in ('{}', 'null', '[]', '{invalid'):
            manifest.write_text(invalid)
            with self.assertRaises(ValueError):
                read_version(manifest)

    def test_generator_updates_both_outputs_without_rewriting_unchanged_files(self):
        manifest = self.root / 'vcpkg.json'
        output = self.root / 'generated version files'
        manifest.write_text('{"version-string":"1.2.3-preview.1"}')
        generate(manifest, output)
        cmake, header = output / 'Version.cmake', output / 'cial/Version.hpp'
        self.assertIn('set(CIAL_VERSION "1.2.3")', cmake.read_text())
        self.assertIn('set(CIAL_VERSION_PRERELEASE "preview.1")', cmake.read_text())
        self.assertIn('#define CIAL_VERSION_MINOR 2', header.read_text())
        self.assertIn('#define CIAL_VERSION_STRING "1.2.3-preview.1"', header.read_text())
        stamps = (cmake.stat().st_mtime_ns, header.stat().st_mtime_ns)
        generate(manifest, output)
        self.assertEqual(stamps, (cmake.stat().st_mtime_ns, header.stat().st_mtime_ns))
        header.unlink()
        generate(manifest, output)
        self.assertTrue(header.is_file())
        self.assertEqual(cmake.stat().st_mtime_ns, stamps[0])
        manifest.write_text('{"version-string":"2.0.0"}')
        generate(manifest, output)
        self.assertIn('set(CIAL_VERSION "2.0.0")', cmake.read_text())
        self.assertIn('set(CIAL_VERSION_PRERELEASE "")', cmake.read_text())
        self.assertIn('#define CIAL_VERSION_MAJOR 2', header.read_text())
        self.assertIn('#define CIAL_VERSION_STRING "2.0.0"', header.read_text())

    def test_release_requires_clean_worktree_and_tag_at_head(self):
        with patch.object(package.subprocess, 'check_output', return_value=' M CMakeLists.txt\n'):
            with self.assertRaises(ValueError):
                package.clean_revision(self.root, None)
        with patch.object(package.subprocess, 'check_output', side_effect=['', 'head\n', 'other\n']):
            with self.assertRaises(ValueError):
                package.clean_revision(self.root, 'v0.1.0-preview.1')
        with patch.object(package.subprocess, 'check_output', side_effect=['', 'head\n', 'head\n']):
            self.assertEqual(package.clean_revision(self.root, 'v0.1.0-preview.1'), 'head')

    def test_zip_escape_paths_are_rejected(self):
        for number, name in enumerate(('../escape', '/absolute', 'C:/drive', '..\\escape')):
            with self.subTest(name=name):
                with self.assertRaises(ValueError):
                    package.extract_archive(self.archive({name: 'bad'}), self.root / f'unpack{number}')
        self.assertFalse((self.root / 'escape').exists())

    def test_tar_links_are_rejected(self):
        archive = self.root / 'link.tar.gz'
        with tarfile.open(archive, 'w:gz') as bundle:
            member = tarfile.TarInfo('root/link')
            member.type = tarfile.SYMTYPE
            member.linkname = '../../escape'
            bundle.addfile(member)
        with self.assertRaises(ValueError):
            package.extract_archive(archive, self.root / 'unpacked')

    def test_tar_preserves_executable_permission(self):
        archive = self.root / 'valid.tar.gz'
        with tarfile.open(archive, 'w:gz') as bundle:
            member = tarfile.TarInfo('root/bin/cial')
            member.mode = 0o755
            member.size = 3
            bundle.addfile(member, io.BytesIO(b'bin'))
        root = package.extract_archive(archive, self.root / 'unpacked')
        self.assertEqual((root / 'bin/cial').read_bytes(), b'bin')
        if os.name != 'nt':
            self.assertTrue((root / 'bin/cial').stat().st_mode & 0o111)

    def package_files(self):
        info = {'schema_version': 1, 'version': '0.1.0-preview.1', 'configuration': 'Release',
                'package_name': 'ciallang-test', 'system': 'Windows' if os.name == 'nt' else 'Linux'}
        files = {'build-info.json': json.dumps(info), 'LICENSE': 'license', 'README.md': 'readme',
                 'examples/hello.tjs': 'println(42);', 'bin/cial.exe' if os.name == 'nt' else 'bin/cial': 'binary'}
        files.update({f'licenses/{port}/copyright': 'license' for port in ('fmt', 'frozen', 'utf8proc', 'argparse')})
        return {'ciallang-test/' + name: data for name, data in files.items()}

    def test_missing_license_and_wrong_version_fail_before_execution(self):
        files = self.package_files()
        archive = self.archive(files)
        with patch.object(package.subprocess, 'run') as execute:
            with self.assertRaises(ValueError):
                package.verify_archive(archive, '99.0.0')
            files.pop('ciallang-test/licenses/fmt/copyright')
            with self.assertRaises(ValueError):
                package.verify_archive(self.archive(files))
            execute.assert_not_called()

    def test_verifier_checks_actual_output_in_a_clean_environment(self):
        archive = self.archive(self.package_files())
        results = [subprocess.CompletedProcess([], 0, '0.1.0-preview.1\n', ''),
                   subprocess.CompletedProcess([], 0, '"Hello from Ciallang: 42"\n', ''),
                   subprocess.CompletedProcess([], 0, '{"diagnostics":[]}', ''),
                   subprocess.CompletedProcess([], 1, '{"diagnostics":[{"message":"syntax"}]}', '')]
        with patch.object(package.subprocess, 'run', side_effect=results) as execute:
            package.verify_archive(archive, '0.1.0-preview.1')
            self.assertEqual(execute.call_count, 4)
            for call in execute.call_args_list:
                self.assertTrue(Path(call.args[0][0]).is_absolute())
                self.assertNotIn('vcpkg', call.kwargs['env']['PATH'])
                self.assertNotIn('LD_LIBRARY_PATH', call.kwargs['env'])
                self.assertNotIn('CIAL_DUMP', call.kwargs['env'])
        with patch.object(package.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0, 'wrong', '')):
            with self.assertRaises(ValueError):
                package.verify_archive(archive)


if __name__ == '__main__':
    unittest.main()
