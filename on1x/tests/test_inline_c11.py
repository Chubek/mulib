"""Trusted native expression -> checked raw call -> separate C11 TU -> executable."""
import os
import sys
from support import NativeTests, run

t = NativeTests()
plugin = t.compiler.parent / 'stdppx/on1x-ppx-InlineC11'
key = 'ON1X_TRUSTED_INLINE_C11_OUTPUT'
env = dict(os.environ)
env.pop(key, None)


def build(name, source, *flags, error=None, expected=None, environment=env):
    binary = t.work / name
    binary.unlink(missing_ok=True)
    p = run([sys.executable, t.root / 'tools/on1x-build.py', t.source(name, source),
             '--on1xc', t.compiler, '--cc', t.cc, '--stdppx-path', plugin.parent,
             '-o', binary, *flags, *('--cflag=' + f for f in t.flags)], env=environment)
    if error:
        assert p.returncode and error in p.stderr and not binary.exists(), (name, p)
        return p.stderr
    else:
        assert p.returncode == 0, (name, p)
        result = run([binary])
        assert (result.returncode, result.stdout, result.stderr) == (0, expected, ''), result


trust = '--trust-inline-c11'
example = (t.root / 'stdppx/examples/inline_c11.on1x').read_text()
build('example', example, trust, expected='10\n')
build('order', '''
fn on1x_inline_c11_0_1() -> i64 { return 17; }
fn trace(x: i64) -> i64 { println(x); return x; }
fn main(){
  println(@InlineC11("unsafe_i64", "a0 + a0 + a1",
    @InlineC11("unsafe_i64", "a0 + 1", trace(2)), trace(3)));
  println(@InlineC11("unsafe_i64", "INT64_C(42)"));
  println(@InlineC11("unsafe_i64", "a0 - a1 + a2 * a3", 12, 2, 5, 3));
  println(on1x_inline_c11_0_1());
}''', trust, expected='2\n3\n9\n42\n25\n17\n')
build('no_processor', example, error='unhandled metaprogramming syntax: InlineC11')
sentinel = t.work / 'unauthorized.c'
sentinel.unlink(missing_ok=True)
build('no_trust', example, '--use-ppx', 'InlineC11', error='--trust-inline-c11',
      environment={**env, key: str(sentinel)})
assert not sentinel.exists()
for name, expr, message in [
        ('operation', '"safe","42"', 'expected unsafe_i64'),
        ('nonliteral', '"unsafe_i64",42', 'literal'),
        ('arity', '"unsafe_i64","1",0,1,2,3,4', '0..4'),
        ('long', '"unsafe_i64","' + '1' * 4097 + '"', 'limit exceeded'),
        ('type', '"unsafe_i64","a0",true', 'i64')]:
    build(name, 'fn main(){println(@InlineC11(' + expr + '));}', trust, error=message)
diagnostic = build('invalid_c', '''fn main() {
  println(@InlineC11("unsafe_i64",
    "unknown_c_identifier"));
}''', trust, error='unknown_c_identifier')
assert 'invalid_c.on1x:3:' in diagnostic, diagnostic
build('site_limit', 'fn main(){' + 'println(@InlineC11("unsafe_i64","1"));' * 33 + '}',
      trust, error='site limit')
build('mir', example, trust, '--via-mir', error='direct C11')
build('emit', example, trust, '--emit-only', error='direct C11')
build('memory', example, trust, '--memory-check', error='without FFI admission')

# Exercise protocol framing and exclusive publication independently of discovery.
out = t.work / 'protocol.c'
out.unlink(missing_ok=True)
protocol = {**env, key: str(out), 'ON1X_PPX_SCHEMA': str(t.root / 'grammar/PPX.schema.json'),
            'ON1X_PPX_PROTOCOL': '1', 'ON1X_PPX_LABEL': 'InlineC11'}
for wire in ('(Unit 1 ()) trailing', '(Unit 99 ())'):
    p = run([plugin], input=wire, env=protocol)
    assert p.returncode and not p.stdout and not out.exists(), p
p = run([plugin], input='(Unit 1 ())', env=protocol)
assert p.returncode == 0 and p.stdout == '(Unit 1 ())\n', p
out.write_text('sentinel')
p = run([plugin], input='(Unit 1 ())', env=protocol)
assert p.returncode and not p.stdout and out.read_text() == 'sentinel', p
