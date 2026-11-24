#!/usr/bin/env python3
"""CI gate for complete generated inventory and read-only freshness."""
import argparse
from pathlib import Path
import subprocess
import sys


TOOLS = Path(__file__).resolve().parents[1] / 'tools'
sys.path.insert(0, str(TOOLS))
from ffi_inventory import load  # noqa: E402
from ffi_publish import DEFAULT_TARGET, TARGETS, check_package  # noqa: E402


def main():
    args = sys.argv[1:]
    if '--' in args:
        split = args.index('--')
        args, generation = args[:split], args[split + 1:]
    else:
        generation = []
    parser = argparse.ArgumentParser(description=__doc__,
                                     epilog='Supply explicit ffgen tool/profile flags after --.')
    parser.add_argument('--mulib-root', required=True, type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--target', choices=sorted(TARGETS), default=DEFAULT_TARGET)
    parser.add_argument('--inventory-only', action='store_true',
                        help='check exact manifest keys and file digests only, not freshness')
    options = parser.parse_args(args)
    if options.inventory_only == bool(generation):
        parser.error('select --inventory-only or pass explicit generation options after --')
    fixed = {'--mulib-root', '--output', '--target', '--all', '--kernel', '--check', '--help'}
    if any(arg == '-h' or (arg.startswith('--') and any(
            option.startswith(arg.split('=', 1)[0]) for option in fixed)) for arg in generation):
        parser.error('generation options cannot override selection, target, output or check mode')
    root = options.mulib_root.resolve()
    if options.output is None and options.target != DEFAULT_TARGET:
        parser.error('non-default target requires --output')
    output = (options.output or root / 'on1x/stdffi/mulib').absolute()
    try:
        inventory = load(root)
        check_package(output, options.target, inventory.by_key)
        if not options.inventory_only:
            argv = [sys.executable, '-B', str(TOOLS / 'ffgen.py'), '--mulib-root', str(root),
                    '--output', str(output), '--target', options.target, '--all', '--check']
            subprocess.run([*argv, *generation], check=True, timeout=7200)
        inventory.verify_unchanged()
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f'check_stdffi: {error}', file=sys.stderr)
        return 1
    print(f'{len(inventory.entries)} registered MuLIB modules accounted for; '
          + ('inventory only' if options.inventory_only else 'read-only regeneration matches'))
    return 0


if __name__ == '__main__':
    sys.exit(main())
