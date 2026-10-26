#!/usr/bin/env python3
"""Read-only environment and GBA image inspection. Does not execute the ROM."""
import argparse
import hashlib
import json
import re
import shutil
import sys
from pathlib import Path


def inspect_rom(path):
    size = path.stat().st_size
    if not 192 <= size <= 32 * 1024 * 1024:
        raise ValueError('Expected an uncompressed GBA cartridge image (192 bytes to 32 MiB).')
    data = path.read_bytes()
    expected = (-sum(data[0xA0:0xBD]) - 0x19) & 255
    word = int.from_bytes(data[:4], 'little')
    is_branch = word & 0xFF000000 == 0xEA000000
    offset = word & 0xFFFFFF
    if offset & 0x800000:
        offset -= 1 << 24
    target = 0x08000008 + offset * 4 if is_branch else None
    signatures = sorted({m.group().decode('ascii') for m in re.finditer(
        rb'(?:EEPROM|SRAM_F|SRAM|FLASH1M|FLASH512|FLASH)_V[0-9]{3}', data)})
    return {
        'filename': path.name,
        'bytes': size,
        'sha1': hashlib.sha1(data).hexdigest(),
        'sha256': hashlib.sha256(data).hexdigest(),
        'title': data[0xA0:0xAC].rstrip(b'\0').decode('ascii', errors='replace'),
        'game_code': data[0xAC:0xB0].decode('ascii', errors='replace'),
        'maker_code': data[0xB0:0xB2].decode('ascii', errors='replace'),
        'revision': data[0xBC],
        'fixed_byte_valid': data[0xB2] == 0x96,
        'header_checksum_valid': data[0xBD] == expected,
        'arm_entry_branch': is_branch,
        'entry_target': f'0x{target:08x}' if target is not None else None,
        'save_signature_hints': signatures,
        'note': 'Header checks do not prove game identity, full-file integrity, or compatibility. Save signatures are hints only.'
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', nargs='?', type=Path, help='Your uncompressed .gba file')
    args = parser.parse_args()
    report = {'python': sys.version.split()[0], 'tools': {
        name: bool(shutil.which(name)) for name in
        ('git', 'c++', 'cmake', 'ninja', 'pkg-config', 'mgba', 'mgba-qt')
    }}
    try:
        if args.rom:
            report['rom'] = inspect_rom(args.rom.expanduser())
    except (OSError, ValueError) as exc:
        parser.exit(1, f'Cannot inspect ROM: {exc}\n')
    print(json.dumps(report, indent=2))
    if args.rom and not all(report['rom'][key] for key in
                           ('fixed_byte_valid', 'header_checksum_valid', 'arm_entry_branch')):
        return 2
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
