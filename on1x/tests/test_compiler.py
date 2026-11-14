#!/usr/bin/env python3
"""Executable tests for On1x parsing, checking and strict C11 lowering."""
from support import NativeTests, run

tests = NativeTests()
root = tests.root
tests.valid('hello', '21\n')
tests.valid('control', '45\n')
tests.valid('import', '49\n')
tests.valid('shadow', '42\n21\n')
tests.valid('evaluation', '1\n2\n3\n99\n99\n', extra=[root / 'examples/evaluation.c'])
tests.valid('external', '42\n', extra=[root / 'examples/external.c'])
tests.valid('i64_min', '-9223372036854775808\n')
tests.valid('literals', '10\n9\n0\n\\0\nC trigraph: ??/\n雪7\nquote: " tab:\t\n')
tests.valid('precedence', '7\n5\n15\n-7\ntrue\nfalse\ntrue\n')
tests.valid('unused', '', text='''
fn ignore(unused: i64) { let unused_local = 42; }
fn main() { ignore(12); var assigned_but_unused = 0; assigned_but_unused = 1; }
''')
tests.valid('json_cleanup', '0\n0\n0\n0\n',
            source=root / 'tests/fixtures/json_cleanup.on1x',
            extra=[root / 'tests/fixtures/json_cleanup.c'])

cases = [
    ('type mismatch', 'fn main() -> i64 { let x: bool = 3; return 0; }', 'expected bool'),
    ('undefined', 'fn main() -> i64 { return missing; }', 'undefined variable'),
    ('wrong arity', 'fn main() -> i64 { println(); return 0; }', 'wrong arity'),
    ('immutable assignment', 'fn main() -> i64 { let x = 1; x = 2; return x; }', 'immutable'),
    ('integer out of range', 'fn main() -> i64 { return 9223372036854775808; }', 'out of i64 range'),
    ('missing return', 'fn main() -> i64 { println(12); }', 'not all paths return'),
    ('unterminated', 'fn main() -> i64 { return 0;', 'unterminated block'),
    ('invalid character', 'fn main() -> i64 { ` }', 'invalid token'),
    ('resource alias', 'fn main() -> i64 { let a = json_parse_or_die("{}"); let b = a; return 0; }', 'resource aliases'),
    ('resource temporary', 'fn main() -> i64 { let a = json_get_or_die(json_parse_or_die("{}"), "a"); return 0; }', 'named live owner'),
    ('resource expression', 'fn main() -> i64 { json_parse_or_die("{}"); return 0; }', 'discarding an owned'),
    ('binary NUL', 'fn main() -> i64 { println("bad\\0"); return 0; }', 'NUL escape'),
    ('C keyword extern', 'extern fn switch() -> i64; fn main() -> i64 { return 0; }', 'C keyword'),
    ('resource returns', 'fn take() -> json { return json_parse_or_die("{}"); } fn main() -> i64 { return 0; }', 'resource returns'),
    ('duplicate local', 'fn main() { let a = 1; let a = 2; }', 'duplicate local'),
    ('duplicate function', 'fn main() {} fn main() {}', 'duplicate function'),
    ('duplicate parameter', 'fn f(a: i64, a: i64) {} fn main() {}', 'duplicate parameter'),
    ('bad assignment', 'fn main() { var a = 1; a = false; }', 'expected i64'),
    ('unreachable', 'fn main() { return; println(1); }', 'unreachable'),
    ('borrow escape', 'fn main() { { let root = json_parse_or_die("{}"); } json_print(root); }', 'undefined variable'),
    ('mutable resource', 'fn main() { var root = json_parse_or_die("{}"); }', 'mutable resources'),
    ('chained ordering', 'fn main() { println(1 < 2 < 3); }', 'expected i64'),
    ('bad logical operand', 'fn main() { println(1 && true); }', 'expected bool'),
    ('bad unary operand', 'fn main() { println(!1); }', 'expected bool'),
    ('bad arithmetic operand', 'fn main() { println(true + 1); }', 'expected i64'),
]
for index, (name, text, expected) in enumerate(cases):
    path = tests.source(f'negative_{index}', text)
    p = run([tests.compiler, '--check', path])
    assert p.returncode == 1 and expected in p.stderr, (name, p.returncode, p.stderr)

# Diagnostics must retain byte-based line/column positions after layout/comments.
source = tests.source('location', '# first\nfn main() {\n\t  missing;\n}\n')
p = run([tests.compiler, '--check', source])
assert p.returncode == 1 and ':3:4: error: undefined variable' in p.stderr, p.stderr

# Imports are deduplicated, and cyclic imports are rejected rather than recursed.
tests.source('shared', 'fn square(x: i64) -> i64 { return x * x; }')
tests.source('left', 'import "shared.on1x";')
tests.source('right', 'import "shared.on1x";')
tests.valid('diamond', '49\n', text='''
import "left.on1x"; import "right.on1x";
fn main() { println(square(7)); }
''')
cycle = tests.source('cycle_a', 'import "cycle_b.on1x"; fn main() {}')
tests.source('cycle_b', 'import "cycle_a.on1x";')
p = run([tests.compiler, '--check', cycle])
assert p.returncode != 0 and 'import cycle' in p.stderr, p.stderr

# Every arithmetic trap is checked with the same strict flags and sanitizers
# as successful programs, not only with an unsanitized default C compiler.
traps = [
    ('9223372036854775807 + 1', 'addition overflow'),
    ('-9223372036854775808 - 1', 'subtraction overflow'),
    ('9223372036854775807 * 2', 'multiplication overflow'),
    ('1 / 0', 'invalid division'),
    ('-9223372036854775808 / -1', 'invalid division'),
    ('1 % 0', 'invalid modulo'),
    ('-9223372036854775808 % -1', 'invalid modulo'),
    ('-(-9223372036854775808)', 'negation overflow'),
]
for index, (expression, diagnostic) in enumerate(traps):
    exe = tests.build(f'arithmetic_error_{index}',
                      text=f'fn main() {{ println({expression}); }}')
    p = run([exe])
    assert p.returncode == 70 and diagnostic in p.stderr, (p.returncode, p.stderr)
print(f'On1x compiler: PASS ({len(cases)} negative cases, {len(traps)} arithmetic traps)')
