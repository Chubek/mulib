#!/usr/bin/env python3
"""Exercise real Clang/LLD Windows binaries through Shiraz's interpreter and ELF AOT."""
import argparse
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--shiraz', required=True, type=Path)
    parser.add_argument('--registry', required=True, type=Path)
    args = parser.parse_args()
    shiraz, registry = args.shiraz.resolve(), args.registry.resolve()
    clang, linker, dlltool = (shutil.which(x) for x in ('clang', 'lld-link', 'llvm-dlltool'))
    if not all((clang, linker, dlltool)):
        print('SKIP: Shiraz interoperability needs clang, lld-link and llvm-dlltool')
        return
    with tempfile.TemporaryDirectory(prefix='shiraz-check-', dir='/tmp/opencode' if Path('/tmp/opencode').is_dir() else None) as directory:
        work = Path(directory)
        env = dict(os.environ, SHIRAZ_REGISTRY=str(work / 'registry.hive'))

        def run(command, code=0, output=None):
            result = subprocess.run([str(x) for x in command], cwd=work, env=env,
                                    capture_output=True, text=True, timeout=60)
            if result.returncode != code or output is not None and result.stdout != output:
                raise AssertionError(f'{command}: exit {result.returncode}, expected {code}\n{result.stdout}\n{result.stderr}')
            return result

        source = work / 'windows.c'
        source.write_text(r'''
typedef unsigned int DWORD;
typedef unsigned long long HANDLE;
#define IMPORT __declspec(dllimport)
IMPORT HANDLE GetStdHandle(DWORD);
IMPORT int WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
IMPORT int ReadFile(HANDLE, void *, DWORD, DWORD *, void *);
IMPORT HANDLE CreateFileA(const char *, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
IMPORT int CloseHandle(HANDLE);
IMPORT HANDLE GetProcessHeap(void);
IMPORT void *HeapAlloc(HANDLE, DWORD, unsigned long long);
IMPORT int HeapFree(HANDLE, DWORD, void *);
IMPORT int RegCreateKeyExA(HANDLE, const char *, DWORD, char *, DWORD, DWORD, void *, HANDLE *, DWORD *);
IMPORT int RegSetValueExA(HANDLE, const char *, DWORD, DWORD, const void *, DWORD);
IMPORT int RegQueryValueExA(HANDLE, const char *, DWORD *, DWORD *, void *, DWORD *);
IMPORT int RegCloseKey(HANDLE);
IMPORT int RegSetValueExW(HANDLE, const unsigned short *, DWORD, DWORD, const void *, DWORD);
static const char text[] = "Shiraz Windows guest\n";
static const char *volatile message = text; /* forces a DIR64 base relocation */
int main(void) {
    DWORD written=0, value=37, result=0, bytes=4, type=0;
    HANDLE key=0;
    HANDLE file, heap;
    DWORD *allocated;
    WriteFile(GetStdHandle((DWORD)-11), message, sizeof(text)-1, &written, 0);
    RegCreateKeyExA(0x80000001ULL,"Software\\Shiraz",0,0,0,0xf003f,0,&key,0);
    RegSetValueExA(key,"Answer",0,4,&value,4);
    RegQueryValueExA(key,"Answer",0,&type,&result,&bytes);
    RegSetValueExW(key,L"Greeting",0,1,L"hello",12);
    RegCloseKey(key);
    heap=GetProcessHeap(); allocated=(DWORD *)HeapAlloc(heap,8,4);
    *allocated=result;
    file=CreateFileA("guest.bin",0x40000000,0,0,2,0x80,0);
    WriteFile(file,0,0,&written,0);
    WriteFile(file,allocated,4,&written,0); CloseHandle(file);
    HeapFree(heap,0,allocated);
    file=CreateFileA("guest.bin",0x80000000,0,0,3,0x80,0);
    ReadFile(file,&result,4,&written,0); CloseHandle(file);
    return (int)result;
}
''')
        for dll, names in [('kernel32', ['GetStdHandle', 'WriteFile', 'ReadFile', 'CreateFileA', 'CloseHandle', 'GetProcessHeap', 'HeapAlloc', 'HeapFree']),
                           ('advapi32', ['RegCreateKeyExA', 'RegSetValueExA', 'RegSetValueExW', 'RegQueryValueExA', 'RegCloseKey'])]:
            definition = work / (dll + '.def')
            definition.write_text('LIBRARY ' + dll + '.dll\nEXPORTS\n' + '\n'.join(names) + '\n')
            run([dlltool, '-m', 'i386:x86-64', '-d', definition, '-l', work / (dll + '.lib')])
        obj, pe = work / 'windows.obj', work / 'windows.exe'
        run([clang, '--target=x86_64-pc-windows-msvc', '-O1', '-ffreestanding', '-fno-stack-protector', '-c', source, '-o', obj])
        run([linker, '/entry:main', '/subsystem:console', '/nodefaultlib', '/dynamicbase', '/out:' + str(pe), obj,
             work / 'kernel32.lib', work / 'advapi32.lib'])
        for image in (pe, obj):
            run([shiraz, 'run', image], 37, 'Shiraz Windows guest\n')
            run([registry, env['SHIRAZ_REGISTRY'], 'get', r'HKCU\Software\Shiraz', 'Answer'], output='37\n')
            run([registry, env['SHIRAZ_REGISTRY'], 'get', r'HKCU\Software\Shiraz', 'Greeting'], output='hello\n')
            assert (work / 'guest.bin').read_bytes() == b'\x25\x00\x00\x00'
            for mode in ([], ['--shared-runtime'], ['--static']):
                elf = work / ('translated ' + image.suffix[1:] + str(len(mode)) + (mode[0] if mode else ''))
                run([shiraz, 'compile', image, elf, *mode])
                data = elf.read_bytes()
                assert data[:4] == b'\x7fELF'
                if mode == ['--static']:
                    phoff = struct.unpack_from('<Q', data, 32)[0]
                    entsize, count = struct.unpack_from('<HH', data, 54)
                    assert all(struct.unpack_from('<I', data, phoff + i * entsize)[0] != 3 for i in range(count)), 'static ELF has PT_INTERP'
                run([elf], 37, 'Shiraz Windows guest\n')
        run([shiraz, 'run', pe, '--base', '0x150000000'], 37, 'Shiraz Windows guest\n')
        rebased = work / 'rebased.elf'
        run([shiraz, 'compile', pe, rebased, '--base', '0x150000000'])
        run([rebased], 37, 'Shiraz Windows guest\n')
        run([registry, env['SHIRAZ_REGISTRY'], 'set', 'HKLM\\Software\\Shiraz', 'Greeting', 'sz', 'hello'])
        run([registry, env['SHIRAZ_REGISTRY'], 'get', 'hklm/software/shiraz', 'greeting'], output='hello\n')
        run([registry, env['SHIRAZ_REGISTRY'], 'delete', 'HKLM\\Software\\Shiraz', 'Greeting'])
        run([registry, env['SHIRAZ_REGISTRY'], 'get', 'HKLM\\Software\\Shiraz', 'Greeting'], 1)
        # Truncated files and unsupported instructions must not produce an ELF.
        bad = work / 'bad.exe'
        bad.write_bytes(pe.read_bytes()[:100])
        run([shiraz, 'run', bad], 125)
        run([shiraz, 'compile', bad, work / 'bad.elf'], 125)
        assert not (work / 'bad.elf').exists()
        # Compiler paths are argv, never shell fragments.
        env['SHIRAZ_CC'] = 'cc;touch injected'
        run([shiraz, 'compile', pe, work / 'bad.elf'], 125)
        assert not (work / 'injected').exists()
        print('Shiraz: PE/COFF interpretation, rebasing, registry and static/shared ELF AOT passed')


if __name__ == '__main__':
    main()
