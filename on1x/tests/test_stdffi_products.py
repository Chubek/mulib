"""Compare portable foreign calls in independently selected products with the C kernel."""
import itertools
import re
import shutil
import sys

from support import NativeTests, run

t = NativeTests()


def ok(*args):
    result = run(args)
    assert result.returncode == 0, (args, result.returncode, result.stderr)
    return result.stdout


def native(name, source, *options):
    binary = t.work / name
    ok(t.cc, '-std=c11', '-I', t.root.parent / 'include', '-I', t.root / 'runtime',
       source, *options, *t.flags, '-o', binary)
    return ok(binary)


source = t.work / 'oracle.c'
source.write_text('''\
#define MULIB_IMPL_CLAMP
#include "mulib-clamp.h"
#include <inttypes.h>
#include <stdio.h>
int main(void) {
    int64_t v[] = {INT64_MIN, -1, 0, 1, INT64_MAX};
    for (unsigned i = 0; i < 5; ++i) {
        for (unsigned j = 0; j < 5; ++j) {
            printf("%" PRId64 "\\n", mulib_clamp_add_i64(v[i], v[j]));
            printf("%" PRId64 "\\n", mulib_clamp_sub_i64(v[i], v[j]));
            printf("%" PRId64 "\\n", mulib_clamp_i64(v[i], v[j], INT64_MAX));
        }
    }
    return 0;
}
''', encoding='ascii')
expected = native('oracle', source)
points = ['-9223372036854775808', '-1', '0', '1', '9223372036854775807']
lines = ['import foreign stdffi.clamp as K;', 'fn main() {']
for a, b in itertools.product(points, repeat=2):
    lines += [f'println(K.add_i64({a}, {b}));', f'println(K.sub_i64({a}, {b}));',
              f'println(K.i64({a}, {b}, {points[-1]}));']
source = t.source('product', '\n'.join([*lines, '}']))
ir = t.work / 'portable.mir'
ok(t.compiler, '--emit-ir', source, '-o', ir)
text = ir.read_text()
assert 'kernel-hot' in text and 'fold-identity' in text, text[:300]
for mode in ['--run-ir', '--run-vm']:
    for flags in [[], ['--optimize-ir']]:
        assert ok(t.compiler, mode, '--ir-input', ir, *flags) == expected, (mode, flags)
generated = t.work / 'product.c'
ok(t.compiler, '--emit-ir-c', '--ir-input', ir, '--mir-target', 'aarch64-unknown-linux-gnu',
   '-o', generated)
assert native('product', generated, t.root / 'runtime/on1x_runtime.c', '-lm') == expected
def hot_prototypes(code):
    return re.findall(r'^ON1X_MIR_HOT (?:ON1X_MIR_PRIVATE )?int64_t (on1x_mir_f\d+)\(', code, re.M)

assert not hot_prototypes(generated.read_text())
base = [t.compiler, '--emit-ir-c', '--ir-input', ir, '--mir-target', 'x86_64-unknown-linux-gnu']
assert not hot_prototypes(ok(*base)), 'speculative placement enabled without optimization'
assert len(hot_prototypes(ok(*base, '--optimize-ir'))) == 3
if sys.platform.startswith('linux') and shutil.which('nm'):
    # Link the generated functions into an actual ELF product: the placement
    # directive must change dynamic visibility, not just MIR diagnostics.
    private = re.findall(r'^.*ON1X_MIR_PRIVATE int64_t (on1x_mir_f\d+)\(', generated.read_text(), re.M)
    assert private, 'expected private kernel helpers in the C product'
    dso = t.work / 'product.so'
    ok(t.cc, '-std=c11', '-shared', '-fPIC', '-I', t.root / 'runtime', generated,
       t.root / 'runtime/on1x_runtime.c', '-lm', *t.flags, '-o', dso)
    exported = {line.split()[-1] for line in ok('nm', '-D', '--defined-only', dso).splitlines()}
    assert 'main' in exported and not exported.intersection(private), exported

# Effects in arguments happen once, in source order, before a failing guard.
bad = t.source('bad_interval', '''\
import foreign stdffi.clamp;
fn mark(x:i64)->i64 { println(x); return x; }
fn main() { println(clamp.i64(mark(1), mark(3), mark(2))); }
''')
for mode in ['--run-ir', '--run-vm']:
    result = run([t.compiler, mode, '--optimize-ir', bad])
    assert result.returncode == 70 and result.stdout == '1\n3\n2\n', (mode, result)
