"""Build, test, package and verify the standalone CLI; never publishes a release."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath, PureWindowsPath
import shutil
import subprocess
import tarfile
import tempfile
import zipfile

from generate_version import read_version

SOURCE = Path(__file__).resolve().parents[1]


def run(command, **kwargs):
    print('+', ' '.join(map(str, command)), flush=True)
    return subprocess.run(list(map(str, command)), check=True, **kwargs)


def project_version(source=SOURCE):
    return read_version(source / 'vcpkg.json')['string']


def validate_tag(tag, version):
    if tag is not None and tag != 'v' + version:
        raise ValueError(f'Tag {tag!r} must be v{version}')


def clean_revision(source, tag):
    status = subprocess.check_output(['git', 'status', '--porcelain'], cwd=source, text=True)
    if status:
        raise ValueError('Release packaging requires a clean Git worktree')
    head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=source, text=True).strip()
    if tag:
        tagged = subprocess.check_output(['git', 'rev-parse', f'refs/tags/{tag}^{{commit}}'], cwd=source, text=True).strip()
        if tagged != head:
            raise ValueError('Release tag must identify the checked out commit')
    return head


def member_path(destination, name):
    # Reject both POSIX and Windows escapes, regardless of the extraction host.
    posix = PurePosixPath(name)
    if '\\' in name or posix.is_absolute() or PureWindowsPath(name).drive or '..' in posix.parts:
        raise ValueError('Unsafe archive member: ' + name)
    result = destination.joinpath(*posix.parts)
    result.resolve().relative_to(destination.resolve())
    return result


def extract_archive(archive, destination):
    destination.mkdir(parents=True)
    if zipfile.is_zipfile(archive):
        with zipfile.ZipFile(archive) as bundle:
            for item in bundle.infolist():
                target = member_path(destination, item.filename)
                kind = (item.external_attr >> 16) & 0o170000
                if kind not in (0, 0o040000, 0o100000):
                    raise ValueError('Archive links and special files are not supported')
                if item.is_dir():
                    target.mkdir(parents=True, exist_ok=True)
                else:
                    target.parent.mkdir(parents=True, exist_ok=True)
                    with bundle.open(item) as src, target.open('wb') as dst:
                        shutil.copyfileobj(src, dst)
                    if os.name != 'nt' and item.external_attr >> 16:
                        target.chmod((item.external_attr >> 16) & 0o777)
    else:
        with tarfile.open(archive, 'r:gz') as bundle:
            for item in bundle:
                target = member_path(destination, item.name)
                if item.isdir():
                    target.mkdir(parents=True, exist_ok=True)
                elif item.isfile():
                    target.parent.mkdir(parents=True, exist_ok=True)
                    with bundle.extractfile(item) as src, target.open('wb') as dst:
                        shutil.copyfileobj(src, dst)
                    target.chmod(item.mode & 0o777)
                else:
                    raise ValueError('Archive links and special files are not supported')
    roots = list(destination.iterdir())
    if len(roots) != 1 or not roots[0].is_dir():
        raise ValueError('Expected a single top-level package directory')
    return roots[0]


def verify_archive(archive, expected_version=None):
    with tempfile.TemporaryDirectory(prefix='cial package verify ') as temporary:
        directory = Path(temporary)
        root = extract_archive(archive, directory / 'unpacked')
        info = json.loads((root / 'build-info.json').read_text(encoding='utf-8'))
        if info.get('schema_version') != 1 or info.get('configuration') != 'Release':
            raise ValueError('Unsupported build metadata or non-Release package')
        if expected_version and info['version'] != expected_version:
            raise ValueError('Archive version does not match the source version')
        if root.name != info['package_name']:
            raise ValueError('Package directory does not match build metadata')
        for relative in ['LICENSE', 'README.md', 'examples/hello.tjs',
                         *[f'licenses/{port}/copyright' for port in ('fmt', 'frozen', 'utf8proc', 'argparse')]]:
            if not (root / relative).is_file():
                raise ValueError('Missing package file: ' + relative)
        executable = root / 'bin' / ('cial.exe' if info['system'] == 'Windows' else 'cial')
        environment = os.environ.copy()
        for key in ('CIAL_DUMP', 'CIAL_TJSBC_DUMP', 'LD_LIBRARY_PATH', 'DYLD_LIBRARY_PATH', 'DYLD_FALLBACK_LIBRARY_PATH'):
            environment.pop(key, None)
        environment['PATH'] = (str(Path(environment.get('SYSTEMROOT', environment.get('SystemRoot', 'C:/Windows'))) / 'System32')
                               if os.name == 'nt' else '/usr/bin:/bin')
        work = directory / 'unrelated working directory'
        work.mkdir()

        def execute(*arguments):
            return subprocess.run([str(executable), *map(str, arguments)], cwd=work, env=environment,
                                  capture_output=True, text=True, encoding='utf-8', timeout=15)

        result = execute('--version')
        if result.returncode != 0 or result.stdout.strip() != info['version']:
            raise ValueError(f'Packaged --version failed: {result.stdout} {result.stderr}')
        result = execute(root / 'examples/hello.tjs')
        if result.returncode != 0 or result.stdout.strip() != '"Hello from Ciallang: 42"':
            raise ValueError(f'Packaged example failed: {result.stdout} {result.stderr}')
        script = work / 'check.tjs'
        for content, code, errors in [('throw 123;', 0, False), ('var broken = ;', 1, True)]:
            script.write_text(content, encoding='utf-8')
            result = execute('--check-json', script)
            report = json.loads(result.stdout)
            if result.returncode != code or bool(report['diagnostics']) != errors:
                raise ValueError('Packaged checker failed: ' + result.stdout + result.stderr)
        print('Verified extracted archive:', archive, flush=True)
        return info


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--preset', help='Configure preset; defaults to the native Release preset for new builds')
    parser.add_argument('--output', type=Path, default=SOURCE / 'build/packages')
    parser.add_argument('--parallel', type=int, default=4)
    parser.add_argument('--tag', help='Require an exact v<project-version> tag name')
    parser.add_argument('--require-clean', action='store_true')
    parser.add_argument('--check-version', action='store_true', help='Validate versions and tag, without building')
    parser.add_argument('--verify', type=Path, help='Verify an existing native archive, without building')
    args = parser.parse_args(argv)
    version = project_version()
    validate_tag(args.tag, version)
    revision = clean_revision(SOURCE, args.tag) if args.require_clean else None
    if args.check_version:
        print(version)
        return
    if args.verify:
        verify_archive(args.verify.resolve(), version)
        return
    if args.parallel < 1:
        parser.error('--parallel must be positive')
    system = {'win32': 'Windows', 'darwin': 'MacOS'}.get(__import__('sys').platform, 'Linux')
    build = (args.build_dir or SOURCE / 'build' / system / 'Release').resolve()
    preset = args.preset or (None if (build / 'CMakeCache.txt').exists() else system + ' Release')
    configure = ['cmake', '--preset', preset, '-B', build] if preset else ['cmake', '-S', SOURCE, '-B', build]
    run([*configure, '-DCMAKE_BUILD_TYPE=Release', '-DCIAL_ENABLE_PACKAGING=ON', '-DCIAL_BUILD_CLI=ON',
         '-DCIAL_BUILD_TESTS=ON', '-DCIAL_ENABLE_JIT=OFF'], cwd=SOURCE)
    run(['cmake', '--build', build, '--config', 'Release', '--parallel', args.parallel])
    run(['ctest', '--test-dir', build, '-C', 'Release', '--output-on-failure', '--no-tests=error'])
    if revision and clean_revision(SOURCE, args.tag) != revision:
        raise ValueError('Source revision changed during the build')
    info = json.loads((build / 'packaging/Release/build-info.json').read_text(encoding='utf-8'))
    if info['version'] != version or (revision and (info['revision'] != revision or info['dirty'] != 'false')):
        raise ValueError('Build provenance does not match the requested release')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    # A private staging directory prevents stale archives from entering the release.
    with tempfile.TemporaryDirectory(prefix='package-', dir=output) as temporary:
        run(['cpack', '--config', build / 'CPackConfig.cmake', '-C', 'Release', '-B', temporary])
        extension = '.zip' if info['system'] == 'Windows' else '.tar.gz'
        archive = Path(temporary) / (info['package_name'] + extension)
        verify_archive(archive, version)
        final = output / archive.name
        shutil.copy2(archive, final)
        digest = hashlib.sha256(final.read_bytes()).hexdigest()
        final.with_name(final.name + '.sha256').write_text(f'{digest}  {final.name}\n', encoding='ascii')
        print('Package ready:', final, flush=True)


if __name__ == '__main__':
    main()
