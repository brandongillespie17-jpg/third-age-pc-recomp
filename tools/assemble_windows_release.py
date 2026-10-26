#!/usr/bin/env python3
"""Assemble a reviewed Windows CI base ZIP and exact local portrait BMPs.

No network, executable launch, installer, font conversion, or image conversion.
Python is needed by the maintainer assembling the ZIP, never by its users.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import stat
import struct
import zipfile
from pathlib import Path, PurePosixPath

MANIFEST_SHA256 = '41cc3ac7361ffb0108392f30dfc2219eb12d14f233ea26868e420a466c7cb4bd'
FONT_MANIFEST_SHA256 = '8473afaf9fcddafd18fa25191645558c81647eba80f8fac82415ad30b06d318f'
FONT_ARCHIVE_SHA256 = '1a400cd55ece1ce992229ee167e1297c1e6373682d10ef309f8742f5b8d3d13b'
SDL_DLL_SHA256 = 'b37740a72a7a9706216df9f0134894bb7a850b356fd149398c67d874cbcfacb4'
SDL_ARCHIVE_SHA256 = 'af347939395a58b365846aaea27391e69f9ec9d4dd650d6ac40802159b418a6e'
BASE_FILES = frozenset({
    'third-age-pc.exe', 'SDL2.dll', 'README.txt', 'BUILD-INFO.json', 'SHA256SUMS.txt',
    'assets/PORTRAIT-MANIFEST.json', 'assets/PORTRAIT-NOTICE.md',
    'assets/FONT-MANIFEST.json', 'extras/Ringbearer-NOTICE.txt',
    'notices/SDL2-LICENSE.txt', 'notices/SDL2-README.txt',
    'notices/DISTRIBUTION-NOTICE.md', 'notices/LICENSING-INVENTORY.md',
    'notices/gbarecomp-LICENSE.txt', 'notices/THIRD_PARTY_ATTRIBUTION.md',
    'notices/MPL-2.0.txt', 'notices/tomlpp-LICENSE.txt',
    'notices/arm-recomp-core-LICENSE.txt', 'notices/rbengine-LICENSE.txt',
    'notices/recomp-net-LICENSE.txt', 'source/mpl/bios_hle.cpp', 'source/mpl/bios_hle.h',
})


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def file_hash(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def regular_file(path: Path) -> None:
    require(path.is_file() and not path.is_symlink(), f'Not a regular file: {path}')
    require(not any(p.is_symlink() for p in path.parents), f'Symlink parent: {path}')


def verify_pe(data: bytes, name: str) -> None:
    require(len(data) >= 256 and data[:2] == b'MZ', f'Not a Windows PE: {name}')
    offset = struct.unpack_from('<I', data, 0x3c)[0]
    require(offset + 26 <= len(data), f'Truncated PE header: {name}')
    require(data[offset:offset + 4] == b'PE\0\0', f'Invalid PE signature: {name}')
    require(struct.unpack_from('<H', data, offset + 4)[0] == 0x8664,
            f'Not an x64 PE: {name}')
    require(struct.unpack_from('<H', data, offset + 24)[0] == 0x20b,
            f'Not PE32+: {name}')


def parse_hashes(data: bytes) -> dict[str, str]:
    result = {}
    for line in data.decode('utf-8-sig').splitlines():
        match = re.fullmatch(r'([0-9a-f]{64})  ([a-zA-Z0-9_./-]+)', line)
        require(match is not None, 'Malformed base SHA256SUMS.txt')
        digest, name = match.groups()
        require(name not in result, 'Duplicate checksum name')
        result[name] = digest
    require(set(result) == BASE_FILES - {'SHA256SUMS.txt'}, 'Base checksum allowlist mismatch')
    return result


def read_base(path: Path, expected_hash: str, commit: str) -> tuple[dict[str, bytes], dict]:
    regular_file(path)
    require(file_hash(path) == expected_hash, 'Base ZIP SHA256 does not match reviewed CI artifact')
    expected_root = f'third-age-pc-windows-x64-preview-base-{commit[:12]}'
    contents = {}
    with zipfile.ZipFile(path) as archive:
        entries = archive.infolist()
        require(len(entries) == len(BASE_FILES), 'Base ZIP has missing/extra/duplicate entries')
        require(sum(e.file_size for e in entries) <= 256 * 1024 * 1024, 'Base ZIP exceeds size limit')
        seen = set()
        for entry in entries:
            name = entry.filename
            parts = PurePosixPath(name).parts
            require('\\' not in name and ':' not in name and not name.startswith('/')
                    and '..' not in parts and '.' not in parts, 'Unsafe ZIP path')
            require(len(parts) >= 2 and parts[0] == expected_root, 'Unexpected base ZIP root')
            relative = '/'.join(parts[1:])
            require(name == expected_root + '/' + relative, 'Noncanonical ZIP path')
            require(relative in BASE_FILES and relative.casefold() not in seen, 'Unexpected/duplicate ZIP entry')
            seen.add(relative.casefold())
            mode = entry.external_attr >> 16
            require(not stat.S_ISLNK(mode) and not entry.is_dir(), 'ZIP symlinks/directories are forbidden')
            require(not entry.flag_bits & 1 and entry.file_size <= 128 * 1024 * 1024, 'Invalid ZIP entry')
            contents[relative] = archive.read(entry)  # Checks the entry CRC as well.
    require(set(contents) == BASE_FILES, 'Base content allowlist mismatch')
    for name, digest in parse_hashes(contents['SHA256SUMS.txt']).items():
        require(sha256(contents[name]) == digest, f'Base file checksum mismatch: {name}')
    for name in ('third-age-pc.exe', 'SDL2.dll'):
        verify_pe(contents[name], name)
    require(sha256(contents['SDL2.dll']) == SDL_DLL_SHA256, 'SDL DLL differs from pinned official x64 binary')
    require(sha256(contents['assets/PORTRAIT-MANIFEST.json']) == MANIFEST_SHA256, 'Unapproved portrait manifest')
    require(contents['assets/PORTRAIT-NOTICE.md'].strip() != b'', 'Missing portrait rights notice')
    require(sha256(contents['assets/FONT-MANIFEST.json']) == FONT_MANIFEST_SHA256, 'Unapproved font archive manifest')
    require(contents['extras/Ringbearer-NOTICE.txt'].strip() != b'', 'Missing font archive notice')
    info = json.loads(contents['BUILD-INFO.json'])
    require(info.get('commit') == commit and info.get('packageKind') == 'code-only-base'
            and info.get('portraitCount') == 0 and info.get('ciAuditPassed') is True,
            'Base package provenance/type/audit fields do not match')
    require(info.get('architecture') == 'x64' and info.get('configuration') == 'Release'
            and info.get('target') == 'third-age-pc'
            and info.get('publicRuntimeCMakeOption') == 'THIRD_AGE_PORTABLE_RELEASE',
            'Unexpected base build contract')
    require(info.get('portraitManifestSha256') == MANIFEST_SHA256
            and info.get('sdlArchiveSha256') == SDL_ARCHIVE_SHA256, 'Dependency/asset pin mismatch')
    imports = info.get('importedDlls', {})
    require('sdl2.dll' in [v.lower() for v in imports.get('third-age-pc.exe', [])], 'Missing base SDL import audit')
    return contents, info


def portrait_inputs(contents: dict[str, bytes], directory: Path) -> dict[str, Path]:
    require(directory.is_dir() and not directory.is_symlink(), 'Portrait directory is missing or is a symlink')
    manifest = json.loads(contents['assets/PORTRAIT-MANIFEST.json'])
    require(manifest.get('schema') == 'third-age-approved-portrait-package-v1'
            and manifest.get('count') == 68 and len(manifest.get('files', [])) == 68,
            'Expected the exact 68-portrait manifest')
    portraits = {}
    for row in manifest['files']:
        name = row['file']
        require(re.fullmatch(r'assets/portraits/[a-z0-9-]+\.bmp', name) is not None
                and name not in portraits, 'Invalid portrait path')
        path = directory / PurePosixPath(name).name
        regular_file(path)
        require(path.stat().st_size == row['bytes'] and file_hash(path) == row['sha256'],
                f'Approved portrait bytes do not match: {name}')
        portraits[name] = path
    # Extra local files are never read/copied; only exact manifest paths are selected.
    return portraits


def font_input(contents: dict[str, bytes], path: Path) -> bytes:
    regular_file(path)
    manifest = json.loads(contents['assets/FONT-MANIFEST.json'])
    data = path.read_bytes()
    require(sha256(data) == FONT_ARCHIVE_SHA256 and len(data) == manifest['bytes'],
            'Optional font archive must be the complete unchanged approved original ZIP')
    require(manifest.get('schema') == 'third-age-optional-original-font-archive-v1'
            and manifest.get('file') == 'extras/Ringbearer-original.zip'
            and manifest.get('sha256') == FONT_ARCHIVE_SHA256, 'Invalid font archive manifest')
    expected = {entry['file']: entry for entry in manifest['entries']}
    require(len(expected) == 9, 'Expected all nine original font archive members')
    with zipfile.ZipFile(path) as archive:
        require(set(archive.namelist()) == set(expected) and len(archive.infolist()) == 9,
                'Font archive members differ from original package')
        for name, entry in expected.items():
            member = archive.read(name)
            require(len(member) == entry['bytes'] and sha256(member) == entry['sha256'],
                    'Modified original font archive member')
    return data  # Only the unchanged ZIP is copied; never extract or install it.


def zip_entry(name: str) -> zipfile.ZipInfo:
    entry = zipfile.ZipInfo(name, (2000, 1, 1, 0, 0, 0))
    entry.compress_type = zipfile.ZIP_DEFLATED
    entry.external_attr = (stat.S_IFREG | 0o644) << 16
    entry.create_system = 3
    return entry


def assemble(base: Path, expected_hash: str, commit: str, portraits_dir: Path,
             output_dir: Path, ci_run_url: str, font_archive: Path) -> Path:
    require(re.fullmatch(r'[0-9a-f]{40}', commit) is not None, 'Expected full commit SHA')
    require(re.fullmatch(r'[0-9a-f]{64}', expected_hash) is not None, 'Expected reviewed base SHA256')
    require(re.fullmatch(r'https://github\.com/brandongillespie17-jpg/third-age-pc-recomp/actions/runs/[0-9]+', ci_run_url) is not None,
            'Expected this repository\'s GitHub Actions run URL')
    require(not output_dir.exists(), 'Output directory must not already exist')
    contents, info = read_base(base, expected_hash, commit)
    portraits = portrait_inputs(contents, portraits_dir)
    font_bytes = font_input(contents, font_archive)
    manifest = json.loads(contents['assets/PORTRAIT-MANIFEST.json'])
    root = f'third-age-pc-windows-x64-preview-{commit[:12]}'
    # Preserve original CI evidence separately; final metadata distinguishes assembly.
    contents['BASE-BUILD-INFO.json'] = contents['BUILD-INFO.json']
    info.update(packageKind='final-preview-with-portraits', portraitCount=68,
                baseZipSha256=expected_hash, ciRunUrl=ci_run_url,
                assembly='Exact manifest-selected local BMP bytes and unchanged optional original font ZIP; no conversion or installation',
                optionalFontArchiveSha256=FONT_ARCHIVE_SHA256, fontArchiveIncluded=True, defaultFont='system font',
                nativeWindowsGameplay='not verified by assembly; desktop review remains required')
    contents['BUILD-INFO.json'] = (json.dumps(info, indent=2, ensure_ascii=False) + '\n').encode()
    readme = contents['README.txt'].decode('utf-8-sig').replace('\r\n', '\n')
    marker = 'CODE-ONLY CI BASE: portraits are NOT included. Assemble the final ZIP before delivery.'
    require(marker in readme, 'Base README does not clearly identify the code-only package')
    readme = readme.replace(marker, 'FINAL PREVIEW: all 68 approved portraits are included unchanged in assets/portraits.\nKeep the full assets directory with the executable. Native Windows gameplay remains pending.')
    base_asset_note = 'This CI base contains only\ntheir manifests/notices, NOT the images or font archive.'
    require(base_asset_note in readme, 'Base asset-status paragraph is missing')
    readme = readme.replace(base_asset_note, 'This final ZIP contains the images and unchanged optional font archive,\nas well as their manifests/notices.')
    readme += '\nFinal assembly preserved the CI executable/DLL bytes and added only the approved portraits\nand unchanged optional original font archive. BASE-BUILD-INFO.json retains the CI audit metadata.\n'
    contents['README.txt'] = readme.encode()
    contents['extras/Ringbearer-original.zip'] = font_bytes
    hashes = {name: sha256(data) for name, data in contents.items() if name != 'SHA256SUMS.txt'}
    hashes.update({row['file']: row['sha256'] for row in manifest['files']})
    contents['SHA256SUMS.txt'] = ''.join(f'{hashes[name]}  {name}\n' for name in sorted(hashes)).encode()
    hashes['SHA256SUMS.txt'] = sha256(contents['SHA256SUMS.txt'])
    expected_names = set(contents) | set(portraits)
    require(expected_names == BASE_FILES | {'BASE-BUILD-INFO.json', 'extras/Ringbearer-original.zip'} | set(portraits), 'Final allowlist mismatch')
    output_dir.mkdir(parents=True, exist_ok=False)
    partial = output_dir / (root + '.zip.partial')
    final = output_dir / (root + '.zip')
    try:
        with zipfile.ZipFile(partial, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
            for name in sorted(expected_names):
                entry = zip_entry(root + '/' + name)
                if name in contents:
                    archive.writestr(entry, contents[name])
                else:
                    with portraits[name].open('rb') as source, archive.open(entry, 'w') as target:
                        shutil.copyfileobj(source, target, 1024 * 1024)
        # Verify exact final names and decompressed bytes before calling it a release ZIP.
        with zipfile.ZipFile(partial) as archive:
            require(set(archive.namelist()) == {root + '/' + n for n in expected_names}
                    and len(archive.infolist()) == len(expected_names), 'Final ZIP entry mismatch')
            for name, digest in hashes.items():
                with archive.open(root + '/' + name) as stream:
                    actual = hashlib.file_digest(stream, 'sha256').hexdigest()
                require(actual == digest, f'Final ZIP checksum mismatch: {name}')
        partial.rename(final)
        digest = file_hash(final)
        (output_dir / (final.name + '.sha256')).write_text(f'{digest}  {final.name}\n', encoding='utf-8')
        return final
    except BaseException:
        partial.unlink(missing_ok=True)
        raise


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base-zip', required=True, type=Path)
    parser.add_argument('--expected-base-sha256', required=True, help='Compare with the reviewed successful CI artifact checksum')
    parser.add_argument('--expected-commit', required=True, help='Full SHA of the reviewed successful Windows job')
    parser.add_argument('--ci-run-url', required=True, help='Record the successful job run; this offline tool cannot authenticate remote run status')
    parser.add_argument('--portraits-dir', required=True, type=Path, help='Directory containing the existing 68 approved BMP files')
    parser.add_argument('--font-archive', required=True, type=Path, help='Complete unchanged original Ringbearer ZIP; copied only as optional extras')
    parser.add_argument('--output-dir', required=True, type=Path, help='New directory; existing output is never overwritten')
    args = parser.parse_args()
    try:
        output = assemble(args.base_zip, args.expected_base_sha256, args.expected_commit,
                          args.portraits_dir, args.output_dir, args.ci_run_url, args.font_archive)
    except (ValueError, OSError, zipfile.BadZipFile, KeyError, TypeError) as error:
        parser.exit(1, f'Assembly refused: {error}\n')
    print(f'Verified final preview ZIP: {output}')
    print('68 unchanged portrait BMPs and the unchanged optional original font ZIP included. Native Windows gameplay still requires review.')


if __name__ == '__main__':
    main()
