#!/usr/bin/env python3
"""Validate the actual packaged executable headers, not just build settings."""
import argparse
import pathlib
import struct
import sys


def inspect_binary(data, target):
    expected = 'arm64' if target.startswith('aarch64') else 'x86_64'
    if data[:4] == b'\xcf\xfa\xed\xfe':
        if len(data) < 32:
            raise ValueError('Truncated Mach-O header')
        cpu, = struct.unpack_from('<I', data, 4)
        arch = {0x1000007: 'x86_64', 0x100000c: 'arm64'}.get(cpu)
        count, = struct.unpack_from('<I', data, 16)
        offset, minimum = 32, None
        for _ in range(count):
            if offset + 8 > len(data):
                raise ValueError('Truncated Mach-O load command')
            command, size = struct.unpack_from('<II', data, offset)
            if size < 8 or offset + size > len(data):
                raise ValueError('Invalid Mach-O load command')
            if command in (0x32, 0x24):
                minimum, = struct.unpack_from('<I', data, offset + (12 if command == 0x32 else 8))
            offset += size
        if minimum is None:
            raise ValueError('Missing macOS deployment target')
        if minimum > 11 << 16:
            version = f'{minimum >> 16}.{(minimum >> 8) & 255}.{minimum & 255}'
            raise ValueError(f'Backend requires macOS {version}; app promises 11.0')
        kind = 'apple'
    elif data[:4] == b'\x7fELF' and len(data) >= 20:
        machine, = struct.unpack_from('<H' if data[5] == 1 else '>H', data, 18)
        arch = {62: 'x86_64', 183: 'arm64'}.get(machine)
        kind = 'linux'
    elif data[:2] == b'MZ' and len(data) >= 64:
        offset, = struct.unpack_from('<I', data, 60)
        if offset + 6 > len(data) or data[offset:offset+4] != b'PE\0\0':
            raise ValueError('Invalid PE header')
        machine, = struct.unpack_from('<H', data, offset + 4)
        arch = {0x8664: 'x86_64', 0xaa64: 'arm64'}.get(machine)
        kind = 'windows'
    else:
        raise ValueError('Unknown or truncated executable format')
    if arch != expected or kind not in target:
        raise ValueError(f'Executable architecture/platform {arch}/{kind} does not match {target}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', required=True)
    parser.add_argument('binaries', nargs='+', type=pathlib.Path)
    args = parser.parse_args()
    for path in args.binaries:
        try:
            inspect_binary(path.read_bytes(), args.target)
        except (ValueError, OSError, struct.error) as error:
            print(f'ERROR: {path}: {error}', file=sys.stderr)
            return 1
        print(f'Verified {path.name}: {args.target}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
