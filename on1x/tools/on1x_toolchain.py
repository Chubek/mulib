"""Native C11 tool selection shared by the project and single-file drivers."""
import os
from pathlib import Path
import platform
import re
import shutil
import struct
import subprocess
import sys

CHOICES = {'compiler': ('cephyr', 'host'), 'assembler': ('sandas', 'host'),
           'linker': ('yellowstone', 'host')}
ARGUMENTS = ('assembler-arg', 'linker-arg')
DIRECTIVES = frozenset((*CHOICES, *ARGUMENTS))
ROOT_ONLY = re.compile(rb'(?m)^[ \t\r]*(?:compiler|assembler|linker|assembler-arg|linker-arg)(?=[ \t\r\n#]|$)')
HOST = dict.fromkeys(CHOICES, 'host')
DEFAULT = {stage: choices[0] for stage, choices in CHOICES.items()}
AMD64 = 'x86_64-linux-gnu'
HOST_HELP = 'select --compiler=host --assembler=host --linker=host (or --cc cc with no explicit stage selections)'


def arguments(parser):
    for stage, choices in CHOICES.items():
        parser.add_argument('--' + stage, choices=choices, help=f'select the native {stage}')
    for arg in ARGUMENTS:
        parser.add_argument('--' + arg, action='append', default=[], metavar='ARG',
                            help=f'append one literal argument; use --{arg}=ARG')
    parser.add_argument('--cephyr-prefix', type=Path,
                        help='trusted Cephyr installation with its native tools, bank and runtime')


def requested(args):
    return any(getattr(args, key.replace('-', '_')) for key in (*DIRECTIVES, 'cephyr-prefix'))


def arg_token(value):
    if not value or len(value.encode('utf-8')) > 4096 or any(ord(ch) < 32 or ord(ch) == 127 for ch in value):
        raise ValueError('native tool argument must be nonempty, control-free and at most 4096 bytes')
    return value


def validate_argv(tool, stage, argv):
    # A manifest may tune the selected stage; it may not replace the output,
    # load a native plugin or Lua script, use a response file, or choose a tool.
    # Keep argument boundaries even when a value contains whitespace or commas.
    if len(argv) > 64:
        raise ValueError(f'too many {stage} arguments (maximum 64)')
    argv = iter(map(arg_token, argv))
    for arg in argv:
        option, sep, value = arg.partition('=')
        if stage == 'assembler':
            if tool == 'host' and arg in ('-g', '--noexecstack', '--fatal-warnings', '--warn', '--no-warn'):
                continue
            if tool == 'sandas' and option in ('-F', '--feature'):
                value = value if sep else next(argv, '')
                if re.fullmatch(r'-?[a-zA-Z][a-zA-Z0-9_.+-]{0,63}', value):
                    continue
        elif arg in ('-s', '--strip-debug', '--gc-sections', '--no-undefined'):
            continue
        elif tool == 'host' and arg == '-z':
            if next(argv, '') in ('now', 'lazy', 'relro', 'norelro', 'noexecstack'):
                continue
        elif tool == 'host' and option in ('-rpath', '--rpath'):
            # Only the host driver uses a dynamic libc. This remains one argv
            # element, not a -Wl comma list or a shell expression.
            if (value if sep else next(argv, '')):
                continue
        raise ValueError(f'unsupported {tool} {stage} argument: {arg!r}')


def resolve(args, config=None, *, force_host=False):
    """CLI > root > explicit legacy --cc host defaults > context defaults."""
    config = config or {}
    defaults = HOST if force_host or args.cc is not None or not config else DEFAULT
    tools = {key: getattr(args, key) or config.get(key, defaults[key]) for key in CHOICES}
    for key in ARGUMENTS:
        argv = [*config.get(key, []), *getattr(args, key.replace('-', '_'))]
        validate_argv(tools[key.removesuffix('-arg')], key.removesuffix('-arg'), argv)
        tools[key] = argv
    return tools


def validate_manifest(config):
    for key in ARGUMENTS:
        if key not in config:
            continue
        stage = key.removesuffix('-arg')
        candidates = (config[stage],) if stage in config else CHOICES[stage]
        for candidate in candidates:
            try:
                validate_argv(candidate, stage, config[key])
            except ValueError as error:
                last_error = error
            else:
                break
        else:
            raise last_error


def is_host(tools):
    return all(tools[key] == 'host' for key in CHOICES)


def resolve_files(tools, prefix=None, cephyr_path=None):
    """Resolve actual executables and ABI inputs, without running tools or falling back."""
    files, missing = {}, []
    if is_host(tools):
        if prefix is not None or cephyr_path is not None:
            raise ValueError('--cephyr-prefix requires a selected LibStone stage')
        return files, missing
    if sys.platform != 'linux' or platform.machine() != 'x86_64' or sys.maxsize <= 2**32:
        raise ValueError(f'LibStone project tools require native Linux AMD64 LP64; {HOST_HELP}')
    if tools['compiler'] == 'host':
        raise ValueError(f'mixing the host C compiler with SandAS or Yellowstone is not qualified; {HOST_HELP}')
    prefix = prefix.resolve() if prefix else None
    cephyr = prefix / 'bin/cephyr' if prefix else cephyr_path or shutil.which('cephyr')
    cephyr = Path(cephyr).resolve() if cephyr else None
    package = prefix / 'share/cephyr/native-packages' if prefix else None
    if package is None and cephyr is not None:
        candidates = (cephyr.parent.parent / 'share/cephyr/native-packages',
                      cephyr.parent / 'native-packages')
        package = next((path for path in candidates if path.is_dir()), candidates[0])

    def require(key, path, label, executable=False):
        if path and path.is_file() and (not executable or os.access(path, os.X_OK)):
            files[key] = path.resolve()
        else:
            missing.append(label if path is None else f'{label} ({path})')

    if tools['compiler'] == 'cephyr':
        require('cephyr', cephyr, 'Cephyr C11 CLI (cephyr)', True)
        require('crt', package / 'AMD64/crt0.o' if package else None, 'Cephyr AMD64 startup (crt0.o)')
        require('libc', package / 'AMD64/libc.a' if package else None, 'Cephyr AMD64 libc (libc.a)')
    if tools['assembler'] == 'sandas':
        require('sandas', package / 'tools/sandstone-as' if package else None, 'SandAS (sandstone-as)', True)
        require('bank', package / 'banks/amd64.symas' if package else None, 'SandAS AMD64 AT&T bank (amd64.symas)')
    if tools['linker'] == 'yellowstone':
        require('yellowstone', package / 'tools/qobjld' if package else None, 'Yellowstone ld (qobjld)', True)
    return files, missing


def assert_available(missing):
    if missing:
        raise ValueError('native toolchain unavailable: ' + ', '.join(missing) +
                         f'; install the Cephyr native package or {HOST_HELP}')


def forward(option, args):
    return [value for arg in args for value in (option, arg)]


def elf(path, kind):
    with path.open('rb') as stream:
        header = stream.read(64)
    if (len(header) != 64 or header[:7] != b'\x7fELF\x02\x01\x01' or
            header[16:20] != bytes((kind, 0, 62, 0))):
        raise ValueError(f'{path.name}: tool did not produce the expected Linux AMD64 ELF artifact')


def runtime_reference(objects):
    """Find unresolved On1x symbols in verified ELF64 ET_REL symbol tables."""
    defined, undefined = set(), set()
    for obj in objects:
        if obj.stat().st_size > 64 * 1024 * 1024:
            raise ValueError('native object exceeds symbol inspection budget')
        data = obj.read_bytes()
        table, = struct.unpack_from('<Q', data, 40)
        stride, count = struct.unpack_from('<HH', data, 58)
        if stride != 64 or not count or table + stride * count > len(data):
            raise ValueError(f'{obj.name}: invalid ELF section table')
        for index in range(count):
            base = table + index * stride
            kind, = struct.unpack_from('<I', data, base + 4)
            if kind != 2:  # SHT_SYMTAB
                continue
            offset, size, link = struct.unpack_from('<QQI', data, base + 24)
            symbol_stride, = struct.unpack_from('<Q', data, base + 56)
            if link >= count or symbol_stride != 24 or size % 24 or offset + size > len(data):
                raise ValueError(f'{obj.name}: invalid ELF symbol table')
            linked = table + link * stride
            string_kind, = struct.unpack_from('<I', data, linked + 4)
            strings, length = struct.unpack_from('<QQ', data, linked + 24)
            if string_kind != 3 or strings + length > len(data):
                raise ValueError(f'{obj.name}: invalid ELF string table')
            for position in range(offset, offset + size, 24):
                name, info, _, section = struct.unpack_from('<IBBH', data, position)
                if info >> 4 not in (1, 2) or name >= length or not data.startswith(b'on1x_', strings + name, strings + length):
                    continue
                end = data.find(b'\0', strings + name, min(strings + length, strings + name + 512))
                if end < 0:
                    raise ValueError(f'{obj.name}: invalid On1x ELF symbol name')
                (defined if section else undefined).add(data[strings + name:end])
    return bool(undefined - defined)


def build_cephyr(tools, files, directory, cc, sources, runtime, flags, binary):
    """Compile every C TU with Cephyr, assemble separately, and link its own CRT/libc."""
    config = directory / 'Cephyr.toml'
    config.write_text(f'target = "{AMD64}"\n', encoding='ascii')
    compiler = [files['cephyr'], '--config', config, '--stdlib=cephyr', '--no-factir',
                '--no-sandslab', '-std=c11', '-Wall', '-Wextra', '-pedantic', '-Werror']
    if tools['assembler'] == 'sandas':
        assembler = [files['sandas'], '--bank', files['bank'], '-s', 'att', '-f', 'elf',
                     *tools['assembler-arg']]
    else:
        assembler = [cc, '-m64', '-c', '-x', 'assembler', *forward('-Xassembler', tools['assembler-arg'])]

    def compile_unit(i, source):
        assembly, obj = directory / f'unit-{i}.s', directory / f'unit-{i}.o'
        subprocess.run([*compiler, *flags, '-S', source, '-o', assembly], check=True, timeout=120)
        subprocess.run([*assembler, assembly, '-o', obj], check=True, timeout=120)
        elf(obj, 1)
        return obj

    objects = [compile_unit(i, source) for i, source in enumerate(sources)]
    # Like an archive, the runtime need not be built for programs which
    # import none of its symbols. If needed, compile the real ABI, not a stub.
    if runtime_reference(objects):
        objects.append(compile_unit(len(objects), runtime))
    if tools['linker'] == 'yellowstone':
        linker = [files['yellowstone'], '--format=elf', '-m', 'elf_x86_64', '--static', *tools['linker-arg']]
    else:
        # The driver selects a host linker, but must never insert host startup
        # or libc into a link compiled against Cephyr's bundled headers.
        if any(arg.partition('=')[0] in ('-rpath', '--rpath') for arg in tools['linker-arg']):
            raise ValueError('rpath is not applicable to the static Cephyr runtime')
        linker = [cc, '-m64', '-nostdlib', '-static', '-no-pie', *forward('-Xlinker', tools['linker-arg'])]
    subprocess.run([*linker, files['crt'], *objects, files['libc'], '-o', binary], check=True, timeout=120)
    elf(binary, 2)
