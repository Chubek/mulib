#!/usr/bin/env python3
"""CI gate for complete generated inventory and read-only freshness."""
import argparse
from collections import Counter
from pathlib import Path
import subprocess
import sys


TOOLS = Path(__file__).resolve().parents[1] / 'tools'
sys.path.insert(0, str(TOOLS))
from ffi_inventory import load  # noqa: E402
from ffi_publish import DEFAULT_TARGET, TARGETS, check_package  # noqa: E402


def check_counts(index, inventory):
    """Reconcile registration/module/function populations without counting admission."""
    if index.get('inventory') != inventory.index_metadata:
        raise ValueError('stdffi manifest/planner/include inventory is stale')
    modules = Counter()
    symbols = Counter()
    attempted = projected = functions = non_candidates = emitted = 0
    if index.get('unattempted') != []:
        raise ValueError('complete stdffi catalogue cannot have unattempted registrations')
    for record in index['modules']:
        status = record.get('status')
        if status not in ('failed', 'unsupported', 'discovery_only', 'representable'):
            raise ValueError('invalid stdffi module outcome: ' + record['key'])
        modules[status] += 1
        if record.get('attempted') is not True or record.get('projected') is not (status != 'failed'):
            raise ValueError('inconsistent stdffi projection state: ' + record['key'])
        attempted += 1
        projected += record['projected']
        candidates = record.get('symbols')
        count = record.get('function_count', 0)
        if (not isinstance(candidates, list) or type(count) is not int or count < len(candidates)
                or (not record['projected'] and count)):
            raise ValueError('inconsistent stdffi symbol count: ' + record['key'])
        functions += count
        non_candidates += count - len(candidates)
        for symbol in candidates:
            if not isinstance(symbol, dict) or symbol.get('status') not in (
                    'excluded', 'unsupported', 'discovery_only', 'representable'):
                raise ValueError('invalid stdffi symbol outcome: ' + record['key'])
            symbols[symbol['status']] += 1
            emitted += symbol['status'] == 'representable' and symbol.get('emitted') is True
    counts = index.get('counts')
    expected = {
        'registered_modules': len(inventory.entries), 'attempted_modules': attempted,
        'projected_modules': projected, 'unattempted_modules': 0,
        'module_outcomes': dict(modules), 'symbol_outcomes': dict(symbols),
        'projected_functions': functions, 'non_candidate_functions': non_candidates,
        'emitted_scalar_candidates': emitted,
    }
    if not isinstance(counts, dict) or any(counts.get(key) != value for key, value in expected.items()):
        raise ValueError('stdffi index counts do not reconcile')


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
                        help='check source inventory, counts and package digests without projection')
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
        index = check_package(output, options.target, inventory.by_key)
        check_counts(index, inventory)
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
