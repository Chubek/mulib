#!/usr/bin/env python3
"""Independent GNU cpio and LLVM EFI interoperability checks (no firmware calls)."""
import argparse
from pathlib import Path
import subprocess
import tempfile


def run(args, **kwargs):
    return subprocess.run(args, check=True, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, **kwargs).stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', required=True)
    parser.add_argument('--efi', required=True)
    parser.add_argument('--cpio', required=True)
    parser.add_argument('--clang', required=True)
    parser.add_argument('--linker', required=True)
    args = parser.parse_args()
    parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
    with tempfile.TemporaryDirectory(prefix='mulib-boot-', dir=parent) as directory:
        root = Path(directory)
        archive = root / 'generated.cpio'
        run([args.archive, 'emit', str(archive)])
        listing = run([args.cpio, '-it', '--quiet'], input=archive.read_bytes()).decode()
        assert listing.splitlines() == ['init', 'dev', 'dev/console'], listing
        # Extract only the regular file; no device-node privileges required.
        extracted = root / 'extract'
        extracted.mkdir()
        run([args.cpio, '-id', '--quiet', 'init'], input=archive.read_bytes(), cwd=extracted)
        assert (extracted / 'init').read_bytes() == b'#!/bin/sh\n'
        # GNU -> MuLIB, both encodings. Includes a hardlink with deferred data.
        (extracted / 'alias').hardlink_to(extracted / 'init')
        for format_name in ('newc', 'crc'):
            data = run([args.cpio, '-o', '-H', format_name, '--quiet'],
                       input=b'init\nalias\n', cwd=extracted)
            external = root / (format_name + '.cpio')
            external.write_bytes(data)
            run([args.archive, 'read', str(external)])
        (extracted / 'link').symlink_to('init')
        data = run([args.cpio, '-o', '-H', 'newc', '--quiet'],
                   input=b'init\nalias\nlink\n', cwd=extracted)
        external = root / 'symlinks.cpio'
        external.write_bytes(data)
        run([args.archive, 'read', str(external)])
        source = root / 'app.c'
        source.write_text('unsigned long long efi_main(void *image, void *table) '
                          '{ (void)image; (void)table; return 0; }\n')
        obj = root / 'app.obj'
        image = root / 'app.efi'
        run([args.clang, '--target=x86_64-pc-windows-msvc', '-ffreestanding',
             '-fno-stack-protector', '-c', str(source), '-o', str(obj)])
        run([args.linker, '/subsystem:efi_application', '/entry:efi_main',
             '/nodefaultlib', '/out:' + str(image), str(obj)])
        run([args.efi, str(image)])
    print('GNU newc/crc and LLVM EFI interoperability passed')


if __name__ == '__main__':
    main()
