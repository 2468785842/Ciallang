"""Generate build-local CMake and C++ version definitions from vcpkg.json."""
import argparse
import json
from pathlib import Path
import re


def read_version(manifest):
    data = json.loads(Path(manifest).read_text(encoding='utf-8'))
    if not isinstance(data, dict):
        raise ValueError('vcpkg.json must contain a JSON object')
    value = data.get('version-string')
    if not isinstance(value, str):
        raise ValueError('vcpkg.json must provide a version-string')
    numeric = r'(0|[1-9][0-9]*)'
    match = re.fullmatch(numeric + r'\.' + numeric + r'\.' + numeric
                         + r'(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?', value)
    if match is None:
        raise ValueError('version-string must be major.minor.patch with an optional prerelease suffix')
    major, minor, patch, prerelease = match.groups()
    prerelease = prerelease or ''
    if any(part.isdigit() and len(part) > 1 and part[0] == '0' for part in prerelease.split('.')):
        raise ValueError('Numeric prerelease identifiers must not have leading zeros')
    return {'major': major, 'minor': minor, 'patch': patch,
            'base': '.'.join((major, minor, patch)), 'prerelease': prerelease, 'string': value}


def write_if_changed(path, content):
    encoded = content.encode('utf-8')
    if not path.exists() or path.read_bytes() != encoded:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(encoded)


def generate(manifest, output):
    version = read_version(manifest)
    output = Path(output)
    cmake = '# Generated from vcpkg.json; do not edit.\n'
    for key, value in [('CIAL_VERSION', version['base']), ('CIAL_VERSION_PRERELEASE', version['prerelease']),
                       ('CIAL_VERSION_STRING', version['string'])]:
        cmake += f'set({key} "{value}")\n'
    header = '// Generated from vcpkg.json; do not edit.\n#pragma once\n\n'
    for part in ('major', 'minor', 'patch'):
        header += f'#define CIAL_VERSION_{part.upper()} {version[part]}\n'
    header += f'#define CIAL_VERSION_STRING "{version["string"]}"\n'
    write_if_changed(output / 'Version.cmake', cmake)
    write_if_changed(output / 'cial/Version.hpp', header)
    return version


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        generate(args.manifest, args.output_dir)
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()
