#!/usr/bin/env python3
"""Exercise target lookup and installation with a minimal indexed CMake fixture."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


HANDLER = Path(__file__).resolve().parents[1]
TARGETS = {
    'x86_64-linux-gnu': ('SysVAMD64', 'sysv'),
    'aarch64-linux-gnu': ('AAPCS64', 'aapcs64'),
    'riscv64-linux-gnu': ('LP64', 'lp64'),
}


def package(directory, target, key):
    directory.mkdir(parents=True, exist_ok=True)
    abi, cc = TARGETS[target]
    document = directory / (key + '.ffi')
    document.write_text(f'''ffi 2;
target "{target}" LP64 "{abi}" {cc};
foreign mulib.{key} {{
    provider mulib "{key}";
    header "mulib-{key}.h";
    unavailable reason no_owned_exports;
}}
''')
    digest = hashlib.sha256(document.read_bytes()).hexdigest()
    (directory / 'index.json').write_text(json.dumps({
        'schema': 'on1x.stdffi.mulib/v1', 'target': target,
        'modules': [{'key': key, 'file': document.name, 'ffi_sha256': digest}],
    }))


def run(*argv, error=None):
    result = subprocess.run([str(arg) for arg in argv], text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=90)
    if error is None:
        assert result.returncode == 0, result.stdout
    else:
        assert result.returncode != 0 and error in result.stdout, result.stdout


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument('--work', type=Path, required=True)
    cli.add_argument('--cmake', required=True)
    cli.add_argument('--libstone-root', type=Path, required=True)
    cli.add_argument('--cc', required=True)
    cli.add_argument('--cxx', required=True)
    cli.add_argument('--cflags', default='')
    cli.add_argument('--cxxflags', default='')
    cli.add_argument('--target', choices=TARGETS, required=True)
    cli.add_argument('--cross', action='store_true', help='cc and cxx are the host Clang drivers')
    options = cli.parse_args()
    target = options.target
    other = next(t for t in TARGETS if t != target)
    with tempfile.TemporaryDirectory(dir=options.work) as temp:
        work = Path(temp)
        project = work / 'src'
        source = project / 'on1x/hir/FFIHandler'
        source.mkdir(parents=True)
        for part in ('CMakeLists.txt', 'FFIHandler.llsynth', 'ffi.cpp'):
            shutil.copyfile(HANDLER / part, source / part)
        shutil.copytree(HANDLER / 'tools', source / 'tools')
        (project / 'dummy.cpp').write_text('int dummy;\n')
        (project / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.20)
project(CatalogueFixture LANGUAGES C CXX)
set(BUILD_TESTING OFF)
add_library(on1x_frontend STATIC dummy.cpp)
add_subdirectory(on1x/hir/FFIHandler)
''')
        base = project / 'on1x/stdffi/mulib'
        package(base, target, 'flat')
        package(base / other, other, 'sibling')
        compilers = [f'-DCMAKE_C_COMPILER={options.cc}', f'-DCMAKE_CXX_COMPILER={options.cxx}',
                     f'-DCMAKE_C_FLAGS={options.cflags}', f'-DCMAKE_CXX_FLAGS={options.cxxflags}']
        if options.cross:
            compilers += ['-DCMAKE_SYSTEM_NAME=Linux',
                          '-DCMAKE_SYSTEM_PROCESSOR=' + target.split('-')[0],
                          '-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY',
                          f'-DCMAKE_C_COMPILER_TARGET={target}', f'-DCMAKE_CXX_COMPILER_TARGET={target}']
            if target.startswith('riscv64'):
                compilers += ['-DCMAKE_C_FLAGS=-march=rv64im -mabi=lp64',
                              '-DCMAKE_CXX_FLAGS=-march=rv64im -mabi=lp64']

        def configure(build, *flags, error=None):
            run(options.cmake, '-S', project, '-B', work / build,
                f'-DON1X_LIBSTONE_ROOT={options.libstone_root}', *compilers, *flags, error=error)

        configure('missing', error=f'indexed {target} catalogue')
        configure('sibling', f'-DON1X_FFI_CATALOG={base / other}', error='must use the real')
        outside = work / 'explicit'
        package(outside, other, 'explicit')
        configure('wrong_index', f'-DON1X_FFI_CATALOG={outside}', error='index target mismatch')
        package(outside, target, 'explicit')
        configure('standalone', f'-DON1X_FFI_CATALOG={outside}')
        if target.startswith('riscv64') and options.cross:
            configure('wrong_abi', f'-DON1X_FFI_CATALOG={outside}',
                      '-DCMAKE_C_FLAGS=-march=rv64gc -mabi=lp64d', error='must both target the FFI ABI')

        package(base / target, target, 'selected')
        configure('build')
        run(options.cmake, '--build', work / 'build', '--target', 'on1x_ffi_tables', '--parallel', 1)
        prefix = work / 'prefix'
        installed = prefix / 'share/on1x/stdffi'
        # An existing package from a separate invocation must not be touched.
        package(installed / 'mulib' / other, other, 'existing')
        sibling = (installed / 'mulib' / other / 'index.json').read_bytes()
        run(options.cmake, '--install', work / 'build', '--prefix', prefix)
        slot = installed / 'mulib' / target
        assert {p.name for p in slot.iterdir()} == {'index.json', 'selected.ffi'}
        for name in ('index.json', 'selected.ffi'):
            assert (slot / name).read_bytes() == (base / target / name).read_bytes()
        report = json.loads((installed / target / 'ffi-manifest.json').read_bytes())
        assert report['target'] == target
        assert (installed / 'mulib' / other / 'index.json').read_bytes() == sibling
        assert not (installed / 'ffi-manifest.json').exists()
        assert not (installed / 'mulib/index.json').exists()
    print(f'PASS CMake lookup and installation for {target}')


if __name__ == '__main__':
    main()
