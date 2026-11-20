"""Verify that container sources and native companions survive packaging."""
import json
from pathlib import Path
import sys
import tempfile

from support import NativeTests, run


t = NativeTests()

with tempfile.TemporaryDirectory(dir=t.work, prefix='container-package-') as path:
    work = Path(path)
    output = work / 'package'
    published = run([sys.executable, t.root/'tools/on1x-project.py', 'build',
                     t.root/'stdlib', '--on1xc', t.compiler, '--cc', t.cc,
                     '--output-dir', output])
    assert published.returncode == 0, published
    stdlib = output/'library'
    declarations = (t.root/'stdlib/container/On1xfile.m').read_text().splitlines()
    contents = ['tty/runtime.c', 'tty/runtime.h']
    for declaration in declarations:
        role, _, name = declaration.partition(' ')
        if role in ('library', 'resource'):
            contents.append('container/'+json.loads(name))
    assert 'container/Box.on1x' in contents and 'container/runtime.c' in contents
    for name in contents:
        assert (stdlib/name).read_bytes() == (t.root/'stdlib'/name).read_bytes(), name

    app = work/'main.on1x'
    app.write_text('''import std.container.Box as B;
fn main() {
  let owner = B.basis_box_new(42);
  println(B.basis_box_into_inner(move(owner)));
}
''', encoding='ascii')
    cmd = [sys.executable, t.root/'tools/on1x-build.py', app, '--on1xc', t.compiler,
           '--cc', t.cc, '--stdlib', stdlib, '--mulib-root', t.root.parent,
           *['--cflag='+flag for flag in t.flags]]
    binary = work/'boxed'
    result = run([*cmd, '-o', binary])
    assert result.returncode == 0, result
    result = run([binary])
    assert result.returncode == 0 and result.stdout == '42\n', result

    if sys.platform.startswith('linux'):
        blob = work/'bytes.bin'
        blob.write_bytes(b'\x00\xff')
        app.write_text(f'''import std.container.Tls as T;
import std.container.Foreign as F;
fn main() {{
  let result = T.basis_tls_i64_with(42, fn() -> i64 {{
    return match T.basis_tls_i64_current() with {{ None() => -1, Some(n) => n }};
  }});
  println(match result with {{ None() => -2, Some(n) => n }});
  println(match T.basis_tls_i64_current() with {{ None() => true, Some(_) => false }});
  let byte = F.basis_foreign_file_byte_at({json.dumps(str(blob))}, 1);
  println(match byte with {{ Ok(Some(b)) => b == 255u8, _ => false }});
}}
''', encoding='ascii')
        binary, plan = work/'native', work/'link.json'
        result = run([*cmd, '-o', binary, '--mulib-plan', plan])
        assert result.returncode == 0, result
        result = run([binary])
        assert result.returncode == 0 and result.stdout == '42\ntrue\ntrue\n', result
        report = json.loads(plan.read_text())
        assert report['safe_export'] is False
        assert report['container_bridge_groups'] == ['FOREIGN', 'TLS']
        assert report['plan']['implementation_translation_units'] == 1
        assert report['plan']['implementation_macros'] == ['MULIB_IMPL_XTHREADS']
        assert str(stdlib/'container/runtime.c') in report['inputs']
        assert str(stdlib/'container/runtime.h') in report['inputs']

        app.write_text('''extern fn on1x_container_file_byte_at(path: i64, offset: i64) -> i64;
fn main() {
  println(on1x_container_file_byte_at(42, 0));
}
''', encoding='ascii')
        failed = work/'bad-abi'
        result = run([*cmd, '-o', failed])
        assert result.returncode != 0 and 'conflicting types' in result.stderr, result
        assert not failed.exists()

        app.write_text('''import std.container.Arc as A;
import std.container.RefCell as C;
fn main() {
  let bytes = A.basis_arc_new(array[255u8, 0u8, 255u8]);
  let alias = A.basis_arc_clone(bytes);
  drop(bytes);
  assert(A.basis_arc_parallel_count(alias, 255u8, 2) == 2);
  let cell = C.basis_refcell_new(42);
  match C.basis_refcell_try_borrow(cell) with {
    Granted(reader) => println(C.basis_refcell_read(reader)), Busy() => assert(false)
  };
}
''', encoding='ascii')
        result = run([*cmd, '-o', binary, '--mulib-plan', plan])
        assert result.returncode == 0, result
        result = run([binary])
        assert result.returncode == 0 and result.stdout == '42\n', result
        report = json.loads(plan.read_text())
        assert report['container_bridge_groups'] == ['SHARED']
        assert report['plan']['implementation_translation_units'] == 1
        assert set(report['plan']['implementation_macros']) == {'MULIB_IMPL_XATOMICS', 'MULIB_IMPL_XTHREADS'}
        for name in ('shared.c', 'on1x_container_shared.h'):
            assert str(stdlib/'container'/name) in report['inputs']
