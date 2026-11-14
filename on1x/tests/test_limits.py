#!/usr/bin/env python3
"""Bounded source loading, syntactic recursion and HIR traversal regressions."""
from support import NativeTests, run

tests = NativeTests()
limit = 2 * 1024 * 1024
source = tests.source('at_source_limit', '#' * limit)
p = run([tests.compiler, '--tokens', source])
assert p.returncode == 0 and p.stdout == f'1:{limit + 1}  <eof>\n', (p.returncode, p.stderr)
for mode in ('--tokens', '--check', '--emit-c', '--types'):
    source = tests.source('over_source_limit', '#' * (limit + 1))
    p = run([tests.compiler, mode, source])
    assert p.returncode == 1 and ':1:1: error: source exceeds 2 MiB limit' in p.stderr, p.stderr

source = tests.source('nul_after_comment', '# comment\nfn main() {\n \x00\n}')
p = run([tests.compiler, '--check', source])
assert p.returncode == 1 and ':3:2: error: NUL in source' in p.stderr, p.stderr

# Exercise both parser recursion and flat left-associative HIR chains. These
# used to reach recursive parser/checker/codegen/destruction stack overflows
# despite fitting easily inside the source-size limit.
deep_cases = {
    'blocks': '{' * 20000 + '}' * 20000,
    'parens': 'println(' + '(' * 20000 + '1' + ')' * 20000 + ');',
    'unary': 'println(' + '!' * 20000 + 'true);',
    'binary': 'println(' + '1+' * 20000 + '1);',
    'calls': 'println(' + 'id(' * 20000 + '1' + ')' * 20000 + ');',
    'postfix_calls': 'id' + '()' * 20000 + ';',
    'lambdas': 'fn() -> i64 { return ' * 20000 + '1' + '; }()' * 20000 + ';',
}
for name, body in deep_cases.items():
    source = tests.source('deep_' + name, 'fn id(x: i64) -> i64 { return x; }\nfn main() { ' + body + ' }')
    p = run([tests.compiler, '--emit-c', source])
    assert p.returncode == 1 and '256-level' in p.stderr, (name, p.returncode, p.stderr)
    assert not p.stdout, (name, 'partial code emitted on failure')

source = tests.source('deep_function_type', 'fn reject(f: ' + 'fn() -> ' * 20000 + 'i64) {} fn main() {}')
p = run([tests.compiler, '--check', source])
assert p.returncode == 1 and 'syntax nesting exceeds 256-level' in p.stderr, (p.returncode, p.stderr)

source = tests.source('bounded_function_type', 'fn accept(f: ' + 'fn() -> ' * 64 + 'i64) {} fn main() {}')
tests.valid('bounded_function_type_native', '', source=source)
source = tests.source('at_type_limit', 'fn accept(f: ' + 'fn() -> ' * 255 + 'i64) {} fn main() {}')
p = run([tests.compiler, '--check', source])
assert p.returncode == 0, p.stderr
source = tests.source('over_type_limit', 'fn accept(f: ' + 'fn() -> ' * 256 + 'i64) {} fn main() {}')
p = run([tests.compiler, '--check', source])
assert p.returncode == 1 and 'syntax nesting exceeds 256-level' in p.stderr, p.stderr

# The expression-tree limit is inclusive and is not just a token-count limit.
source = tests.source('at_tree_limit', 'fn main() { ' + '+'.join(['1'] * 256) + '; }')
p = run([tests.compiler, '--check', source])
assert p.returncode == 0, p.stderr
tests.valid('at_tree_limit_native', '', source=source)
source = tests.source('over_tree_limit', 'fn main() { ' + '+'.join(['1'] * 257) + '; }')
p = run([tests.compiler, '--check', source])
assert p.returncode == 1 and 'expression exceeds 256-level' in p.stderr, p.stderr

source = tests.source('at_block_limit', 'fn main() {' + '{' * 255 + '}' * 255 + '}')
p = run([tests.compiler, '--check', source])
assert p.returncode == 0, p.stderr
source = tests.source('over_block_limit', 'fn main() {' + '{' * 256 + '}' * 256 + '}')
p = run([tests.compiler, '--check', source])
assert p.returncode == 1 and 'syntax nesting exceeds 256-level' in p.stderr, p.stderr

# Successfully check, emit, compile and run nontrivial trees below the limits.
tests.valid('bounded_trees', '64\n1\n', text='fn main() { ' + '{' * 64 +
            'println(' + '+'.join(['1'] * 64) + ');' +
             'println(' + '(' * 64 + '1' + ')' * 64 + ');' + '}' * 64 + ' }')

# Anonymous-function bodies contribute to the enclosing expression-tree bound,
# even when a long outer left-associative chain uses shallow parser recursion.
for terms, accepted in ((255, True), (256, False)):
    source = tests.source(f'lambda_tree_{terms}', 'fn main() { let f = fn() -> i64 { return ' +
                          '+'.join(['1'] * terms) + '; }; }')
    p = run([tests.compiler, '--check', source])
    assert p.returncode == (0 if accepted else 1), (terms, p.returncode, p.stderr)
    if not accepted:
        assert 'expression exceeds 256-level' in p.stderr, p.stderr
tests.valid('bounded_lambda_tree', '64\n', text='fn main() { println(fn() -> i64 { return ' +
            '+'.join(['1'] * 64) + '; }()); }')
source = tests.source('combined_lambda_tree', 'fn main() { fn() -> i64 { return ' +
                      '+'.join(['1'] * 200) + '; }()' + '+1' * 100 + '; }')
p = run([tests.compiler, '--check', source])
assert p.returncode == 1 and 'expression exceeds 256-level' in p.stderr, p.stderr

for count, accepted in ((2048, True), (2049, False)):
    source = tests.source(f'lambda_count_{count}', 'fn main() { ' + 'fn() {};' * count + ' }')
    p = run([tests.compiler, '--check', source])
    assert p.returncode == (0 if accepted else 1), (count, p.returncode, p.stderr)
    if not accepted:
        assert 'too many anonymous functions' in p.stderr, p.stderr

declarations = ''.join(f'let v{i} = {i};' for i in range(128))
expression = '+'.join(f'v{i}' for i in range(128))
for count, accepted in ((128, True), (129, False)):
    source = tests.source(f'capture_count_{count}', 'fn main() {' + declarations +
                          ('fn() -> i64 { return ' + expression + '; };') * count + '}')
    p = run([tests.compiler, '--check', source])
    assert p.returncode == (0 if accepted else 1), (count, p.returncode, p.stderr)
    if not accepted:
        assert 'closure capture metadata limit exceeded' in p.stderr, p.stderr

# Transitive captures can amplify a single long source identifier; bound the
# copied names independently of capture count and avoid echoing the huge name.
name = 'x' * 32768
source = tests.source('capture_name_budget', 'fn main() { let ' + name + ' = 1;' +
                      'fn() -> i64 { return ' * 80 + name + '; }()' * 80 + '; }')
p = run([tests.compiler, '--check', source])
assert p.returncode == 1 and 'closure capture metadata limit exceeded' in p.stderr, (p.returncode, p.stderr[:512])
assert len(p.stderr) < 512, len(p.stderr)
print('Source and syntax limits: PASS')
