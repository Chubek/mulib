#!/usr/bin/env python3
"""Exercise the real LLSynth parser and On1xMETA validation/emission boundary."""
import argparse
import hashlib
import json
import os
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
    parser_generator = root / 'generate_parser.py'
    bootstrap = root / 'generated/On1xMETA.py'
    parser = work / 'parser.py'

    def run(argv, success=True, *, env=None):
        result = subprocess.run(list(map(str, argv)), text=True, capture_output=True,
                                timeout=60, env=env)
        if success:
            assert result.returncode == 0, (argv, result.returncode, result.stderr)
        return result

    # Give actual LLSynth two different grammar locations, output directories,
    # and Perl hash seeds. Raw Python emission embeds the full source path.
    run([args.perl, llsynth, '--check', '--mode=interpret', spec])
    relocated_grammar = work / 'relocated söurce' / spec.name
    relocated_grammar.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(spec, relocated_grammar)
    relocated_parser = work / 'relocated output' / parser.name
    for grammar, output, seed in [(spec, parser, '601'),
                                  (relocated_grammar, relocated_parser, '987')]:
        run([sys.executable, '-B', parser_generator, '--perl', args.perl,
             '--llsynth', llsynth, '--grammar', grammar, '--output', output],
            env=dict(os.environ, PERL_HASH_SEED=seed, PERL_PERTURB_KEYS='2'))
    assert parser.read_bytes() == relocated_parser.read_bytes(), 'parser depends on its location'

    # An added grammar dependency must not be silently hidden by normalization.
    dependent_grammar = work / 'dependent source' / spec.name
    dependent_grammar.parent.mkdir(parents=True, exist_ok=True)
    (dependent_grammar.parent / 'extra.llsynth').write_text('# additional source\n', encoding='utf-8')
    dependent_grammar.write_text('.include "extra.llsynth"\n' + spec.read_text(encoding='utf-8'),
                                 encoding='utf-8')
    before = relocated_parser.read_bytes()
    rejected_parser = run([sys.executable, '-B', parser_generator, '--perl', args.perl,
                           '--llsynth', llsynth, '--grammar', dependent_grammar,
                           '--output', relocated_parser], False)
    assert rejected_parser.returncode == 1 and 'expected one self-contained grammar' in rejected_parser.stderr
    assert relocated_parser.read_bytes() == before

    def generate(inputs, directory, *, success=True, using=parser, source=False, output='on1x_meta_rules.hpp'):
        directory.mkdir(parents=True, exist_ok=True)
        return run([sys.executable, '-B', generator, *(['--source'] if source else []), '--parser', using,
                      '--output', directory / output,
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
        ('unknown_phase', header + good.replace('canonicalize', 'unknown'), 'unknown rewrite phase'),
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

    # Compare the generated and bootstrap parse trees and lexical token
    # positions on accepted input, and exact lexer/parser diagnostics on errors.
    operators = root / 'source/operators.o1m'
    accepted = sorted(root.glob('*.o1m')) + sorted((root / 'tests/fixtures').glob('*.o1m')) + [operators]
    bad_syntax = ['trailing_junk', 'missing_semicolon']
    for path in accepted + [work / (name + '.o1m') for name in bad_syntax]:
        for mode in ['--tokens', '--parse']:
            old, fresh = (run([sys.executable, '-B', p, mode, path], False)
                          for p in [bootstrap, parser])
            assert (old.returncode, old.stdout, old.stderr) == (
                fresh.returncode, fresh.stdout, fresh.stderr), (path, mode)
            if path in accepted:
                assert fresh.returncode == 0 and fresh.stdout, (path, mode)
            elif mode == '--parse':
                assert fresh.returncode == 1 and fresh.stderr, path

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
    generate(shipping, work / 'shipping-bootstrap', using=bootstrap)
    for name in ['on1x_meta_rules.hpp', 'manifest.json']:
        assert (work / 'shipping' / name).read_bytes() == (work / 'shipping-again' / name).read_bytes()
    # Only parser identity may change in a refresh, not the authored table.
    def table_payload(directory):
        directory = work / directory
        table = (directory / 'on1x_meta_rules.hpp').read_text(encoding='utf-8')
        identities = ('inline constexpr char parser_sha256[]', 'inline constexpr char program_sha256[]')
        manifest = json.loads((directory / 'manifest.json').read_text(encoding='utf-8'))
        for field in ['parser_sha256', 'program_sha256', 'output_sha256']:
            del manifest[field]
        return [line for line in table.splitlines() if not line.startswith(identities)], manifest

    assert table_payload('shipping') == table_payload('shipping-bootstrap')
    source_one, source_two = work / 'source-one', work / 'source-two'
    generate([operators], source_one, source=True)
    generate([operators], source_two, source=True, using=bootstrap)
    for name in ('on1x_meta_rules.hpp', 'manifest.json'):
        assert (source_one / name).read_bytes() == (source_two / name).read_bytes(), name
    data = (source_one / 'on1x_meta_rules.hpp').read_text()
    assert 'Form::OperatorTernary, Replacement::Call, 3, {0,1,2}' in data
    assert 'Form::OperatorDeclaration, Replacement::DefineOperator, 1, {0,0,0}' in data
    assert 'compiled separately' in generate([operators], rejected, success=False).stderr
    assert 'compiled separately' in generate([valid], rejected, source=True, success=False).stderr
    valid_source = operators.read_text()
    # The production interpreter executes the selected source payload, not a
    # native pass selected by any particular rule name.
    alternate = work / 'alternate-source.o1m'
    alternate.write_text(valid_source.replace('operator_binary phase source priority 100',
                                               'renamed_binary phase source priority 101'))
    runtime = work / 'source-runtime'
    generate([alternate], runtime, source=True, output='on1x_meta_source.hpp')
    driver = runtime / 'probe.cpp'
    driver.write_text('''#include "source_desugar.hpp"
#include <iostream>
int main() {
  namespace m = on1x::meta::source;
  using on1x::Expr;
  Expr e;
  e.kind = Expr::Kind::OperatorBinary;
  for (auto text : {"1", "2"}) {
    auto operand = std::make_unique<Expr>();
    operand->kind = Expr::Kind::Integer;
    operand->text = text;
    e.children.push_back(std::move(operand));
  }
  m::Program program;
  auto ordinary = program.expression(e, "host-id", m::select(m::form_of(e.kind), e.where));
  ordinary.publish();
  if (e.kind != Expr::Kind::Call || e.callee->text != "host-id") return 1;
  const auto report = program.report(0);
  std::cout << report.rules[0].name << ' ' << e.children[0]->text << e.children[1]->text;
}
''')
    executable = runtime / 'probe'
    run([args.cxx, '-std=c++20', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
         '-I', runtime, '-I', root, '-I', on1x / 'include', driver,
         root / 'source_desugar.cpp', on1x / 'src/type.cpp', '-o', executable])
    assert run([executable]).stdout == 'renamed_binary 12'
    source_cases = [
        (valid_source.replace('$left, $right)\nguard', '$right, $left)\nguard'),
         'evaluation order'),
        (valid_source.replace('$left, $right)\nguard', '$left, $left)\nguard'),
         'evaluation order'),
        (valid_source.replace('$left, $right)\nguard', '$left)\nguard'),
         'evaluation order'),
        (valid_source.replace('Call(BoundOperator(), $left', 'Call(BoundOperator("forged"), $left'),
         'cannot contain literal text'),
        (valid_source.replace('Call(BoundOperator(), $left', 'Call($right, $left'),
         'host-bound callee'),
        (valid_source.replace('guard resolved_fixity', 'guard host_binding'),
         'requires the resolved_fixity guard'),
        (valid_source[:valid_source.index('rule operator_fixity')], 'missing mandatory source forms'),
    ]
    for i, (text, diagnostic) in enumerate(source_cases):
        path = work / ('invalid-source-' + str(i) + '.o1m')
        path.write_text(text)
        result = generate([path], rejected, source=True, success=False)
        assert result.returncode == 1 and diagnostic in result.stderr, result.stderr
        assert (rejected / 'on1x_meta_rules.hpp').read_text() == 'preserved\n'
    print(f'On1xMETA real LLSynth regeneration, table compilation, relocation/order determinism: PASS; '
          f'{len(cases)+8+len(source_cases)} rejected inputs; bootstrap/fresh parser and table payload parity: PASS')


if __name__ == '__main__':
    main()
