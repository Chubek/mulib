"""Linux little-endian AAPCS64 scalar checked-C11 target policy."""

import struct

TARGET = ['aarch64-linux-gnu', 'LP64', 'AAPCS64', 'aapcs64']
ELF_MACHINE = 183  # EM_AARCH64
PRECONDITION = ('!defined(__aarch64__) || !defined(__AARCH64EL__) || '
                '!defined(__LP64__)')
LOADER = 'ld-linux-aarch64.so.1'
PACKAGE = 'AArch64'
BANK = 'aarch64'


def check_elf(path, kinds, *, static=False):
    with path.open('rb') as stream:
        header = stream.read(64)
        if (len(header) != 64 or header[:7] != b'\x7fELF\x02\x01\x01' or
                int.from_bytes(header[16:18], 'little') not in kinds or
                int.from_bytes(header[18:20], 'little') != ELF_MACHINE or
                header[20:24] != b'\x01\x00\x00\x00' or
                header[48:54] != b'\x00\x00\x00\x00\x40\x00'):
            raise ValueError(f'FFI expected little-endian AArch64 ELF type {kinds}: {path.name}')
        if static:
            start = struct.unpack_from('<Q', header, 32)[0]
            width, count = struct.unpack_from('<HH', header, 54)
            if start < 64 or width != 56 or not 0 < count <= 128:
                raise ValueError(f'FFI invalid AArch64 program headers: {path.name}')
            stream.seek(start)
            table = stream.read(width * count)
            # PT_DYNAMIC=2 and PT_INTERP=3 must both be absent. Do not run a
            # cross program that may pick up an emulator-owned guest loader.
            if len(table) != width * count or any(
                    struct.unpack_from('<I', table, pos)[0] in (2, 3)
                    for pos in range(0, len(table), width)):
                raise ValueError(f'FFI expected a static AArch64 executable: {path.name}')
