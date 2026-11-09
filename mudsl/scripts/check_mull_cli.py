#!/usr/bin/env python3
"""CLI, native ABI, stage stopping, linking, and C++/D differential regressions."""
import argparse
from pathlib import Path
import os
import random
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mull', required=True)
    parser.add_argument('--cc', default='cc')
    parser.add_argument('--native', default='ON')
    parser.add_argument('--mulib', action='store_true')
    args = parser.parse_args()
    binary = Path(args.mull).resolve()
    native = args.native.upper() in ('ON', 'TRUE', '1')

    def run(*options, ok=True, input=None, env=None):
        result = subprocess.run([str(binary), *map(str, options)], input=input,
                                capture_output=True, text=True, timeout=40, env=env)
        assert (result.returncode == 0) == ok, (options, result.returncode, result.stdout, result.stderr)
        return result

    assert 'MuLL ' in run('--version').stdout
    assert '--stop-after' in run('--help').stdout
    available = run('--list-engines').stdout
    engines = ['cpp'] + (['d'] if 'd: available' in available else [])
    native_engines = {line.split(':')[0] for line in available.splitlines()
                      if '; x86-64 runtime: available' in line}
    with tempfile.TemporaryDirectory(prefix='mull cli ; spaces-') as directory:
        tmp = Path(directory)
        compiled = 0
        ir_outputs = []
        for engine in engines:
            flags = ['--engine=' + engine]
            assert run(*flags, '-p', '-e', 'return 6*7;').stdout == '42\n'
            assert run(*flags, '-p', '-', input='return 9;').stdout == '9\n'
            assert run(*flags, '-p', '-e', 'return Args;', '--', '-x', 'a b').stdout == '[-x, a b]\n'
            assert run(*flags, '--check', '-e', 'G:GetStdout()("EFFECT");').stdout == ''
            assert 'EFFECT' not in run(*flags, '--emit=mu3ac', '-e', 'G:GetStdout()("EFFECT");').stderr
            for stage, marker in [('ast', 'program'), ('mullier', 'program'), ('mu3ac', 'mu3ac')]:
                result = run(*flags, '--emit=' + stage, '-e', 'return 7+3;').stdout
                assert marker in result
                if stage == 'mu3ac': ir_outputs.append(result)
            assert 'immutable' in run(*flags, '--check', '-e', 'val x <- 1; x <- 2;', ok=False).stderr
            assert 'source size' in run(*flags, '--max-source-bytes=3', '-e', 'return 42;', ok=False).stderr
            repl = run(*flags, '--repl', input='var x <- 1;\nx <- x+1;\nx;\n:quit\n')
            assert '=> 2' in repl.stdout
            if not native or engine not in native_engines:
                continue
            for optimize in ['-O0', '-O1']:
                # Existing language fixtures exercise all value kinds, closures,
                # package callbacks, failures, imports, signed zero, and budgets.
                sources = list((ROOT / 'tests/mull').glob('*.mull'))
                if args.mulib:
                    sources += list((ROOT / 'tests/mull/mulib').glob('*.mull'))
                for source in sorted(sources):
                    base = [*flags, optimize, '-p', '--no-default-paths', '-I', ROOT / 'mull/stdlib',
                            '-I', ROOT / 'tests/mull/packages', source]
                    expected = source.with_suffix('.expected')
                    interpreted = run(*base, ok=expected.exists())
                    generated = run('--backend=x86-64', *base, ok=expected.exists())
                    assert generated.stdout == interpreted.stdout, (engine, optimize, source, generated.stdout, interpreted.stdout)
                    if not expected.exists():
                        error = source.with_suffix('.error').read_text().strip()
                        assert error in generated.stderr, (source, error, generated.stderr)
                    compiled += 1
                # Target checks happen before evaluating an assignment's RHS.
                source = 'val f <- fn(){x <- G:GetStdout()("BAD");}; val x <- 0; f();'
                failure = run(*flags, optimize, '--backend=x86-64', '-e', source, ok=False)
                assert not failure.stdout and 'immutable' in failure.stderr
                source = 'var n <- 0; while n < 8 { n <- n+1; { if n > 2 { break; } } } return n;'
                assert run(*flags, optimize, '--backend=x86-64', '-p', '-e', source).stdout == '3\n'
                # Deterministic expression generation and semantic corner cases.
                rng = random.Random(1729)
                def expression(depth):
                    if not depth:
                        return str(rng.randrange(1, 10))
                    return '(' + expression(depth-1) + rng.choice(['+', '-', '*']) + expression(depth-1) + ')'
                arithmetic = 'return [' + ','.join(expression(3) for _ in range(12)) + '];'
                cases = [arithmetic,
                    'val make <- fn(){return fn(){return 1;};}; val f <- make(); return [f==f, make()==make(), (fn(){})==(fn(){})];',
                    'var i <- 0; while i < 4 { i <- i+1; if i == 3 {break;} i*10; }',
                    'return [G:Str(-0), G:Str(-(-0)), G:Str(0+(-0)), G:Str((-0)*1)];',
                    'var trace <- ""; val mark <- fn(s,x){trace <- trace+s; return x;}; val x <- mark("a",1)+mark("b",2); val xs <- [mark("c",3),mark("d",4)]; return [trace,x,xs];',
                    'val f <- fn(){return later;}; val later <- 19; return f();',
                    'val obj <- {"a\\0b": 23}; return obj["a\\0b"];']
                for source in cases:
                    reference = run(*flags, optimize, '-p', '-e', source)
                    generated = run(*flags, optimize, '--backend=x86-64', '-p', '-e', source)
                    assert generated.stdout == reference.stdout, (engine, optimize, source, generated.stdout, reference.stdout)
                    compiled += 1
            for limit, source, error in [('--max-steps=20', 'while true {}', 'execution limit'),
                                         ('--max-call-depth=4', 'val f <- fn(){return f();}; f();', 'call depth'),
                                         ('--max-frames=2', '{} {}', 'environment limit'),
                                         ('--max-values=2', 'return 1+2;', 'value limit')]:
                assert error in run(*flags, '--backend=x86-64', limit, '-e', source, ok=False).stderr
            asm = tmp / (engine + '.s')
            obj = tmp / (engine + '.o')
            exe = tmp / (engine + ' executable')
            run(*flags, '-S', '-o', asm, '-e', 'return 17;')
            assert 'mu3ac_entry' in asm.read_text()
            run(*flags, '-c', '-o', obj, '-e', 'return 17;')
            assert obj.read_bytes().startswith(b'\x7fELF')
            run(*flags, '--emit=exe', '-p', '-o', exe, '-e', 'return Args;')
            result = subprocess.run([exe, 'hello', '-option'], capture_output=True, text=True, timeout=10)
            assert result.returncode == 0 and result.stdout == '[hello, -option]\n'
            # Actually resolve and execute a third-party symbol through the C ABI.
            libsource = tmp / 'foreign.c'
            libsource.write_text('double custom_square(double x) { return x*x; }\n')
            library = tmp / 'libcustom.a'
            subprocess.run([args.cc, '-x', 'c', '-c', libsource, '-o', tmp / 'foreign.o'], check=True)
            subprocess.run(['ar', 'rcs', library, tmp / 'foreign.o'], check=True)
            linked = run(*flags, '--backend=x86-64', '-p', '-L', tmp, '-lcustom',
                         '--extern=Square=custom_square:1', '-e', 'return Square(7);')
            assert linked.stdout == '49\n'
            shared = tmp / 'libsharedcustom.so'
            subprocess.run([args.cc, '-x', 'c', '-shared', '-fPIC', libsource, '-o', shared], check=True)
            linked = run(*flags, '--backend=x86-64', '-p', '-L', tmp, '-lsharedcustom', '--rpath', tmp,
                         '--extern=Square=custom_square:1', '-e', 'return Square(8);')
            assert linked.stdout == '64\n'
            assert 'invalid number' in run(*flags, '--backend=x86-64', '-e', 'return 1e999;', ok=False).stderr
        assert len(set(ir_outputs)) == 1, 'C++ and D Mu3AC formats differ'
        run('--emit=exe', '-o', '-', '-e', 'return 1;', ok=False)
        run('--engine=nope', '-e', 'return 1;', ok=False)
        run('--backend=interpreter', '-lcustom', '-e', 'return 1;', ok=False)
        run('--extern=Bad=evil;symbol:1', '--backend=x86-64', '-e', 'return 1;', ok=False)
        run('--max-steps=0', '-e', 'return 1;', ok=False)
        run('--backend=x86-64', '-o', '-', '-e', 'return 1;', ok=False)
        run('--check', '-o', tmp / 'unused', '-e', 'return 1;', ok=False)
        original = tmp / 'keep.mull'
        original.write_text('return 1;')
        run('--emit=asm', '-o', original, original, ok=False)
        assert original.read_text() == 'return 1;'
        print(f'MuLL CLI checks passed: {len(engines)} hosts, {compiled} native differential fixture runs')


if __name__ == '__main__':
    main()
