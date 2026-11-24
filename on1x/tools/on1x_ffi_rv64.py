"""Static RV64IM LP64 FFI toolchain. No system sysroot or ABI fallback."""

from pathlib import Path


TARGET = ('riscv64-linux-gnu', 'LP64', 'LP64', 'lp64')
PREPROCESSOR_ABI = (
    'defined(__linux__) && defined(__riscv) && __riscv_xlen == 64 && '
    'defined(__riscv_mul) && defined(__LP64__) && defined(__riscv_float_abi_soft) && '
    '!defined(__riscv_float_abi_single) && !defined(__riscv_float_abi_double) && '
    '!defined(__riscv_float_abi_quad)'
)
COMPILE = (
    '--target=riscv64-linux-gnu', '-march=rv64im', '-mabi=lp64', '-mno-relax',
    '-ffreestanding', '-fno-builtin', '-fno-pic', '-fno-pie', '-fno-stack-protector',
    '-ffunction-sections', '-fdata-sections', '-nostdinc',
)


def check_elf(path, kind):
    """Verify the object/executable contract, not just the symbol or triple."""
    with Path(path).open('rb') as stream:
        header = stream.read(64)
    if (len(header) != 64 or header[:7] != b'\x7fELF\x02\x01\x01' or
            header[16:24] != bytes((kind, 0, 243, 0, 1, 0, 0, 0)) or
            header[48:54] != b'\0\0\0\0\x40\0'):
        # e_flags == 0: integer calling convention; also no RVC, RVE, or TSO.
        raise ValueError(f'FFI expected RV64IM ELF type {kind} with soft-float LP64 flags: {Path(path).name}')


class Toolchain:
    """A fixed explicit Clang/LLD/QEMU tuple and the matching Cephyr CRT/libc.

    Its caller owns private staging, bounded execution, and review/application
    publication. This helper never accepts an application as its ABI probe.
    """

    def __init__(self, cc, linker, runner, headers, runtime, *, run, pin):
        self.cc = self.absolute(cc)
        self.linker = self.absolute(linker)
        self.runner = self.absolute(runner)
        self.headers = self.absolute(headers)
        runtime = self.absolute(runtime)
        self.crt = (runtime / 'crt0.o').resolve(strict=True)
        self.libc = (runtime / 'libc.a').resolve(strict=True)
        self.run = run
        for path in (self.cc, self.linker, self.runner, self.crt, self.libc):
            pin(path)
        check_elf(self.crt, 1)
        with self.libc.open('rb') as stream:
            if stream.read(8) != b'!<arch>\n':  # Do not permit unpinned thin archive members.
                raise ValueError('FFI RV64 requires a regular Cephyr libc.a archive')
        self.flags = (*COMPILE, '-isystem', self.headers)

    @staticmethod
    def absolute(value):
        path = Path(value) if value else Path()
        if not path.is_absolute():
            raise ValueError('FFI RV64 requires explicit absolute tool and runtime paths')
        return path.resolve(strict=True)

    def driver(self, *args):
        return self.run([self.cc, *self.flags, *args])

    def link(self, output, objects, *, roots=(), record=None):
        for path in objects:
            check_elf(path, 1)
        argv = [self.linker, '-m', 'elf64lriscv', '-static', '--no-relax', '--gc-sections',
                '--no-undefined', '-z', 'noexecstack', '-o', output]
        # The independent prototype witness must retain its relocation even
        # when the application never invokes the selected foreign symbol.
        for root in roots:
            argv += ['-u', root]
        argv += [self.crt, *objects, self.libc]
        if record:
            record(argv)
        self.run(argv)
        check_elf(output, 2)

    def execute(self, executable):
        check_elf(executable, 2)
        return self.run([self.runner, executable])
