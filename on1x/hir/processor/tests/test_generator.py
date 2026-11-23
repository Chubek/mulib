#!/usr/bin/env python3
"""Exercise the real LLSynth parser and On1xMETA validation/emission boundary."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument('--root', type=Path, required=True)
    cli.add_argument('--work', type=Path, required=True)
    cli.add_argument('--cxx', default='c++')
    cli.add_argument('--perl', default='perl')
    cli.add_argument('--llsynth', type=Path)
    args = cli.parse_args()
    root, work = args.root.resolve(), args.work.resolve()
    on1x = root.parents[1]
    work.mkdir(parents=True, exist_ok=True)
    llsynth = args.llsynth or on1x / 'third_party/libstone/cornerstone/LLSynth/scripts/llsynth.pl'
    spec, generator = root / 'On1xMETA.llsynth', root / 'generate.py'
    parser = work / 'parser.py'

    def run(argv, success=True):
        result = subprocess.run(list(map(str, argv)), text=True, capture_output=True, timeout=60)
        if success:
            assert result.returncode == 0, (argv, result.returncode, result.stderr)
        return result

    # The parser under test is freshly produced from the actual pinned LLSynth.
    run([args.perl, llsynth, '--check', '--mode=interpret', spec])
    for output in [parser, work / 'parser-again.py']:
        run([args.perl, llsynth, '--mode=interpret', '-T', 'python3', '-o', output, spec])
    assert parser.read_bytes() == (work / 'parser-again.py').read_bytes(), 'parser nondeterminism'

    def generate(inputs, directory, *, success=True):
        directory.mkdir(parents=True, exist_ok=True)
        return run([sys.executable, generator, '--parser', parser,
                    '--output', directory / 'on1x_meta_rules.hpp',
                    '--manifest', directory / 'manifest.json', *inputs], success)

    valid = work / 'identity.o1m'
    shutil.copyfile(root / 'tests/fixtures/identity.o1m', valid)
    one, two = work / 'one', work / 'two'
    generate([valid], one)
    relocated = work / 'relocated/identity.o1m'
    relocated.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(valid, relocated)
    generate([relocated], two)
    for name in ['on1x_meta_rules.hpp', 'manifest.json']:
        assert (one / name).read_bytes() == (two / name).read_bytes(), name + ' depends on source directory'
    manifest = json.loads((one / 'manifest.json').read_text())
    assert manifest['sources'] == [{'file': 'identity.o1m',
                                   'sha256': hashlib.sha256(valid.read_bytes()).hexdigest()}]
    assert manifest['rules'] == 2 and manifest['constraints'] == 1
    assert manifest['output_sha256'] == hashlib.sha256((one / 'on1x_meta_rules.hpp').read_bytes()).hexdigest()
    assert 'LLSynth' in manifest['parser']
    for field, path in [('parser_sha256', parser), ('generator_sha256', generator),
                        ('schema_sha256', on1x / 'include/on1x/meta_schema.hpp')]:
        assert manifest[field] == hashlib.sha256(path.read_bytes()).hexdigest(), field
    # Do not merely assert changed hashes: compile the table and inspect its
    # selected replacement and source provenance through its public C++ schema.
    driver = work / 'table.cpp'
    driver.write_text('''#include <on1x_meta_rules.hpp>
#include <iostream>
int main() {
 namespace g = on1x::meta::generated;
 for (const auto& r:g::rules)
  std::cout << r.name << ":" << g::nodes[r.replacement].text << ":"
            << r.file << ":" << r.line << "\\n";
}
''')
    binary = work / 'table'
    run([args.cxx, '-std=c++20', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
         '-I', one, '-I', on1x / 'include', driver, '-o', binary])
    assert run([binary]).stdout == 'remove_double_not:value:identity.o1m:3\nlazy_and::identity.o1m:7\n'

    # Input order is irrelevant, including disjoint semantic-only modules.
    second = work / 'extra.o1m'
    second.write_text('meta 1;\nbudget steps 100000 nodes 100000 depth 128;\n'
                      'constraint bool_type phase semantic match Boolean require type "bool" message "wrong type";\n')
    generate([valid, second], one)
    generate([second, valid], two)
    assert (one / 'on1x_meta_rules.hpp').read_bytes() == (two / 'on1x_meta_rules.hpp').read_bytes()
    assert (one / 'manifest.json').read_bytes() == (two / 'manifest.json').read_bytes()

    header = 'meta 1;\nbudget steps 100000 nodes 100000 depth 128;\n'

    def rule(match, replace, guard='boolean', name='bad', priority=100):
        return (f'rule {name} phase canonicalize priority {priority}\n'
                f'match {match}\nreplace {replace}\nguard {guard};\n')

    good = rule('Unary("!", Unary("!", $x))', '$x')
    cases = [
        ('missing_header', good, ''),
        ('bad_version', header.replace('meta 1', 'meta 2') + good, 'version'),
        ('trailing_junk', header + good + '@', ''),
        ('missing_semicolon', header + good[:-2], ''),
        ('unknown_phase', header + good.replace('canonicalize', 'unknown'), ''),
        ('unknown_constructor', header + good.replace('Unary', 'Mystery', 1), 'constructor'),
        ('arity', header + rule('Unary("!", $x, $y)', '$x'), 'operands'),
        ('unknown_guard', header + good.replace('boolean', 'unknown'), 'guard'),
        ('pure_guard', header + good.replace('boolean', 'pure'), 'prove operand types'),
        ('always_capture', header + good.replace('boolean', 'always'), 'operand types'),
        ('unknown_capture', header + rule('Unary("!", Unary("!", $x))', '$missing'), 'unbound'),
        ('duplicate_binding', header + rule('Binary("&&", $x, $x)', '$x'), 'unique'),
        ('duplicate_use', header + rule('Binary("&&", $x, $y)', 'Binary("==", $x, $x)'), 'duplicates'),
        ('drop_effect', header + rule('Binary("&&", $x, Boolean("false"))', 'Boolean("false")'), 'evaluation order'),
        ('or_drop_effect', header + rule('Binary("||", $x, Boolean("true"))', 'Boolean("true")'), 'evaluation order'),
        ('force_skipped_effect', header + rule('Binary("&&", Boolean("false"), $x)', '$x'), 'evaluation order'),
        ('drop_skipped_capture', header + rule('Binary("||", Boolean("true"), $x)', 'Boolean("true")'), 'drops a capture'),
        ('reorder_effect', header + rule('Binary("&&", $x, $y)', 'Select("", $y, $x, Boolean("false"))'), 'evaluation order'),
        # Same truth table for all four inputs. Only the trace distinguishes
        # this from the admitted elimination of two negated operands.
        ('swap_negated_equality', header + rule('Binary("==", Unary("!", $x), Unary("!", $y))',
                                               'Binary("==", $y, $x)'), 'evaluation order'),
        ('eager_effect', header + rule('Binary("&&", $x, $y)', 'Binary("==", $x, $y)'), 'evaluation order'),
        ('change_value', header + rule('Unary("!", Boolean("true"))', 'Boolean("true")'), 'value'),
        ('self_cycle', header + rule('Unary("!", $x)', 'Unary("!", $x)'), 'termination'),
        ('growth', header + rule('Unary("!", $x)', 'Unary("!", Unary("!", $x))'), 'termination'),
        ('unchecked_overflow', header + rule('Unary("-", Unary("-", $x))', '$x', 'integer'),
         'unproved checked-i64 negation'),
        ('nonzero_negation', header + rule('Unary("-", Integer("1"))', 'Integer("-1")', 'integer'),
         'only literal zero'),
        ('integer_drop', header + rule('Binary("*", $x, Integer("0"))', 'Integer("0")', 'integer'), 'multiplicity'),
        ('integer_change', header + rule('Binary("+", $x, Integer("1"))', '$x', 'integer'), 'unproved'),
        ('integer_range', header + rule('Binary("+", $x, Integer("9223372036854775808"))', '$x', 'integer'), 'decimal i64'),
        ('integer_format', header + rule('Binary("+", $x, Integer("00"))', '$x', 'integer'), 'decimal i64'),
        ('bad_boolean', header + rule('Unary("!", Boolean("1"))', 'Boolean("true")'), 'true or false'),
        ('bad_operator', header + rule('Unary("~", $x)', '$x'), 'operator'),
        ('duplicate_rule', header + good + good, 'duplicate'),
        ('ambiguous', header + good + good.replace('rule bad', 'rule other'), 'overlap'),
        ('priority', header + good.replace('priority 100', 'priority 65536'), 'priority'),
        ('zero_steps', header.replace('steps 100000', 'steps 0') + good, 'META_BUDGET'),
        ('huge_nodes', header.replace('nodes 100000', 'nodes 4294967296') + good, 'META_BUDGET'),
        ('huge_depth', header.replace('depth 128', 'depth 4294967296') + good, 'META_BUDGET'),
        ('deep_tree', header + rule('Unary("!", ' * 40 + '$x' + ')' * 40, '$x'), 'META_BUDGET'),
        ('unknown_kind', header + 'constraint c phase semantic match Mystery require arity "1" message "bad";', 'kind'),
        ('unknown_property', header + 'constraint c phase semantic match Binary require mystery "1" message "bad";', 'property'),
        ('invalid_type', header + 'constraint c phase semantic match Binary require type "magic" message "bad";', 'type'),
        ('invalid_arity', header + 'constraint c phase semantic match Binary require arity "-1" message "bad";', 'arity'),
        ('invalid_callee', header + 'constraint c phase semantic match Call require callee "maybe" message "bad";', 'callee'),
        ('invalid_resolved', header + 'constraint c phase semantic match Variable require resolved "false" message "bad";', 'true'),
        ('empty_message', header + 'constraint c phase semantic match Binary require arity "2" message "";', 'message'),
        ('oversize', header + '#' + 'x' * 262144, 'META_BUDGET'),
        ('declarations', header + ''.join(f'constraint c{i} phase semantic match Boolean require type "bool" message "bad";\n'
                                        for i in range(513)), 'META_BUDGET'),
    ]
    rejected = work / 'rejected'
    rejected.mkdir(exist_ok=True)
    for name, source, diagnostic in cases:
        path = work / (name + '.o1m')
        path.write_text(source)
        for output in ['on1x_meta_rules.hpp', 'manifest.json']:
            (rejected / output).write_text('preserved\n')
        result = generate([path], rejected, success=False)
        assert result.returncode == 1 and diagnostic in result.stderr, (name, result.returncode, result.stderr)
        assert 'On1xMETA:' in result.stderr and 'Traceback' not in result.stderr, (name, result.stderr)
        assert not result.stdout, (name, result.stdout)
        for output in ['on1x_meta_rules.hpp', 'manifest.json']:
            assert (rejected / output).read_text() == 'preserved\n', (name, output)

    # File encoding and output collisions are errors without corrupting inputs.
    bad_utf8 = work / 'bad-utf8.o1m'
    bad_utf8.write_bytes(b'\xff')
    result = generate([bad_utf8], rejected, success=False)
    assert result.returncode == 1 and 'Traceback' not in result.stderr
    before = valid.read_bytes()
    result = run([sys.executable, generator, '--parser', parser, '--output', valid, valid], False)
    assert result.returncode == 1 and valid.read_bytes() == before and 'overwrite' in result.stderr
    result = run([sys.executable, generator, '--parser', parser,
                  '--output', rejected / 'same', '--manifest', rejected / 'same', valid], False)
    assert result.returncode == 1 and 'collision' in result.stderr
    result = generate([valid, relocated], rejected, success=False)
    assert result.returncode == 1 and 'basenames' in result.stderr
    second.write_text(second.read_text().replace('steps 100000', 'steps 99999'))
    result = generate([valid, second], rejected, success=False)
    assert result.returncode == 1 and 'identical budgets' in result.stderr

    # Validate all shipping rule modules, not just the standalone fixture.
    shipping = sorted(root.glob('*.o1m'))
    assert len(shipping) >= 3, 'canonicalize/desugar/semantic modules must all ship'
    generate(shipping, work / 'shipping')
    generate(list(reversed(shipping)), work / 'shipping-again')
    for name in ['on1x_meta_rules.hpp', 'manifest.json']:
        assert (work / 'shipping' / name).read_bytes() == (work / 'shipping-again' / name).read_bytes()
    print(f'On1xMETA real LLSynth regeneration, table compilation, relocation/order determinism: PASS; '
          f'{len(cases)+5} rejected inputs')


if __name__ == '__main__':
    main()
