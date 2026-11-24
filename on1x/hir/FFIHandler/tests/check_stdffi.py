#!/usr/bin/env python3
"""CI gate for complete generated inventory and read-only freshness."""
import argparse
from collections import Counter
import hashlib
from pathlib import Path
import re
import subprocess
import sys


TOOLS = Path(__file__).resolve().parents[1] / 'tools'
sys.path.insert(0, str(TOOLS))
from ffi_inventory import load  # noqa: E402
from ffi_publish import DEFAULT_TARGET, TARGETS, check_package  # noqa: E402

_CODE = re.compile(r'[a-z][a-z0-9_]{0,95}')
_SHA256 = re.compile(r'[0-9a-f]{64}')
_GENERATION_TOOLS = frozenset({
    'ffgen.py', 'ffi_types.py', 'cephyr_interface.py', 'cephyr_syntax.py', 'project_interface.py',
    'ffi_inventory.py', 'ffi_publish.py', 'ffi_plan.py', 'ffic.py', 'FFIHandler.llsynth',
})


def _matches(value, pattern):
    return isinstance(value, str) and pattern.fullmatch(value) is not None


def check_generation_sources(index, tools=TOOLS):
    """Detect local generator/grammar drift without requiring Cephyr or LLSynth."""
    generation = index.get('generation')
    sources = generation.get('tools') if isinstance(generation, dict) else None
    if not isinstance(sources, dict) or sources.keys() != _GENERATION_TOOLS:
        raise ValueError('stdffi index does not record the current generator sources')
    for name, recorded in sorted(sources.items()):
        path = (tools.parent if name == 'FFIHandler.llsynth' else tools) / name
        if not _matches(recorded, _SHA256) or hashlib.sha256(path.read_bytes()).hexdigest() != recorded:
            raise ValueError('stdffi generator source is stale: ' + name)


def check_counts(index, inventory):
    """Reconcile registration/module/function populations without counting admission."""
    if index.get('inventory') != inventory.index_metadata:
        raise ValueError('stdffi manifest/planner/include inventory is stale')
    modules = Counter()
    symbols = Counter()
    attempted = projected = functions = non_candidates = emitted = owned = 0
    if index.get('unattempted') != []:
        raise ValueError('complete stdffi catalogue cannot have unattempted registrations')
    for record in index['modules']:
        key = record['key']
        entry = inventory.by_key[key]
        if (record.get('module'), record.get('header')) != (entry.module, entry.header_path):
            raise ValueError('noncanonical stdffi module identity: ' + key)
        status = record.get('status')
        if status not in ('failed', 'unsupported', 'discovery_only', 'representable'):
            raise ValueError('invalid stdffi module outcome: ' + key)
        modules[status] += 1
        if record.get('attempted') is not True or record.get('projected') is not (status != 'failed'):
            raise ValueError('inconsistent stdffi projection state: ' + key)
        reason = record.get('reason')
        if ((reason is not None or status == 'failed') and not _matches(reason, _CODE)
                or status in ('representable', 'discovery_only') and reason is not None):
            raise ValueError('invalid stdffi module reason: ' + key)
        projection = record.get('projection_sha256')
        signature = record.get('normalized_signature_sha256')
        # The independent reader can reject bytes emitted by a successful
        # provider. Those bytes have a digest, but no normalized signature.
        if (projection is not None and not _matches(projection, _SHA256)
                or record['projected'] and (projection is None or not _matches(signature, _SHA256))
                or not record['projected'] and signature is not None):
            raise ValueError('inconsistent stdffi projection digest: ' + key)
        attempted += 1
        projected += record['projected']
        candidates = record.get('symbols')
        count = record.get('function_count', 0)
        if (not isinstance(candidates, list) or type(count) is not int or count < len(candidates)
                or (not record['projected'] and count)):
            raise ValueError('inconsistent stdffi symbol count: ' + key)
        functions += count
        non_candidates += count - len(candidates)
        seen = set()
        for symbol in candidates:
            if not isinstance(symbol, dict) or symbol.get('status') not in (
                    'excluded', 'unsupported', 'discovery_only', 'representable'):
                raise ValueError('invalid stdffi symbol outcome: ' + key)
            name, state, reason = symbol.get('symbol'), symbol['status'], symbol.get('reason')
            if not isinstance(name, str) or not name or name in seen:
                raise ValueError('invalid or duplicate stdffi symbol: ' + key)
            seen.add(name)
            ownership = symbol.get('ownership')
            is_owned = ownership in ('audited_selection', 'verified_origin')
            if (not is_owned and ownership not in
                    ('ambiguous_export', 'reserved_other_module', 'foreign_origin')
                    or type(symbol.get('emitted')) is not bool
                    or state == 'representable' and (not is_owned or reason is not None)
                    or state != 'representable' and not _matches(reason, _CODE)
                    or state == 'excluded' and symbol['emitted']):
                raise ValueError('inconsistent stdffi symbol state: ' + key)
            symbols[state] += 1
            owned += is_owned
            emitted += state == 'representable' and symbol['emitted']
    counts = index.get('counts')
    expected = {
        'registered_modules': len(inventory.entries), 'attempted_modules': attempted,
        'projected_modules': projected, 'unattempted_modules': 0,
        'module_outcomes': dict(modules), 'symbol_outcomes': dict(symbols),
        'projected_functions': functions, 'non_candidate_functions': non_candidates,
        'owned_functions': owned, 'emitted_scalar_candidates': emitted,
    }
    if (not isinstance(counts, dict) or any(counts.get(key) != value for key, value in expected.items())
            or any(type(counts[key]) is not int for key, value in expected.items() if type(value) is int)):
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
                        help='check inventory, tools, counts and package digests without projection')
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
        check_generation_sources(index)
        if not options.inventory_only:
            argv = [sys.executable, '-B', str(TOOLS / 'ffgen.py'), '--mulib-root', str(root),
                    '--output', str(output), '--target', options.target, '--all', '--check']
            subprocess.run([*argv, *generation], check=True, timeout=7200)
        inventory.verify_unchanged()
        check_generation_sources(index)
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f'check_stdffi: {error}', file=sys.stderr)
        return 1
    print(f'{len(inventory.entries)} registered MuLIB modules accounted for; '
          + ('inventory only' if options.inventory_only else 'read-only regeneration matches'))
    return 0


if __name__ == '__main__':
    sys.exit(main())
