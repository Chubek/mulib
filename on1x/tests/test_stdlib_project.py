#!/usr/bin/env python3
"""Build real source packages and consume the relocated standard library."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ON1X = Path(__file__).resolve().parents[1]
STDLIB = ON1X / 'stdlib'
RESOURCES = {
    'runtime/basis_native.c', 'runtime/basis_native.h', 'runtime/basis_real.c',
    'prettyprint/runtime.c', 'prettyprint/runtime.h',
}


class StdlibProjectTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='on1x-stdlib-test-')
        self.addCleanup(temporary.cleanup)
        self.work = Path(temporary.name)
        self.env = {key: value for key, value in os.environ.items()
                    if not key.startswith(('ON1X_', 'MULIB_'))}

    def write(self, name, text):
        path = self.work / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding='utf-8')
        return path

    def execute(self, *args, negative=None):
        result = subprocess.run(args, cwd=self.work, env=self.env, text=True, capture_output=True, timeout=120)
        if negative is None:
            self.assertEqual(result.returncode, 0, result.stderr)
            return result.stdout
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn(negative, result.stderr)

    def build(self, root, output='output', *options, negative=None):
        dest = self.work / output
        result = self.execute(sys.executable, OPTIONS.driver, 'build', root,
                              '--on1xc', OPTIONS.on1xc, '--cc', OPTIONS.cc,
                              '--mulib-root', ON1X.parent, '--output-dir', dest, *options, negative=negative)
        if negative is None:
            self.assertEqual(Path(result.strip()), dest)
        return dest

    def test_whole_stdlib_and_downstream(self):
        dest = self.build(STDLIB)
        package = (dest / 'library').rename(self.work / 'relocated stdlib')
        report = json.loads((dest / '.on1x-build.json').read_bytes())
        modules = {p.relative_to(STDLIB).as_posix() for p in STDLIB.rglob('*.on1x')
                   if 'tests' not in p.relative_to(STDLIB).parts}
        self.assertEqual(modules, {p.relative_to(package).as_posix() for p in package.rglob('*.on1x')})
        self.assertEqual(RESOURCES, {p.relative_to(package).as_posix() for p in package.rglob('*')
                                     if p.suffix in ('.c', '.h')})
        for name in sorted(modules | RESOURCES | {'mulib/On1xfile'}):
            data = (STDLIB / name).read_bytes()
            self.assertEqual((package / name).read_bytes(), data)
            self.assertEqual(report['inputs']['project/' + name], hashlib.sha256(data).hexdigest())
        for name in sorted(RESOURCES):
            path = self.execute(OPTIONS.on1xc, '--stdlib-resource', name, '--stdlib', package)
            self.assertEqual(Path(path.strip()), package / name)
        self.assertEqual(self.execute(OPTIONS.on1xc, '--print-stdlib', '--stdlib-path', package).strip(),
                         str(package))
        self.write('client/On1xfile', 'on1xfile 1\nkind executable\nname "consumer"\n'
                   'target "consumer" "consumer.on1x"\n')
        self.write('client/On1xfile.m', 'on1xfile 1\nsource "consumer.on1x"\n')
        client = self.write('client/consumer.on1x', '''
import std.List as L;
import std.base.list as B;
import std.numeric.Real as R;
import std.prettyprint as P;
import std.ini.ini as I;
from std import mulib;
fn main() {
    println(B.base_list_length(L.cons(sx_string("one"),L.nil())));
    println(R.basis_real_add("0.25","0.75"));
    let(status,formatted)=P.pp_format(P.pp_text("packaged prettyprint"),30);
    assert(status==0);
    println(formatted);
    let(parsed,value)=I.basis_ini_get("answer=42\\n","","answer");
    assert(parsed==0);
    println(value);
    println(mulib.abi_version());
}
''')
        graph = {Path(path) for path in self.execute(
            OPTIONS.on1xc, '--deps', client, '--stdlib', package).splitlines()}
        self.assertTrue(graph)
        self.assertTrue(all(p == client or p.is_relative_to(package) for p in graph), graph)
        self.assertIn(package / 'collections/List.on1x', graph)
        self.assertIn(package / 'core/Pervasive.on1x', graph)
        self.assertIn(package / 'mulib/Native.on1x', graph)
        separate = self.write('standalone.on1x', 'import mulib as M;\nfn main(){assert(M.fits_size(42));}\n')
        self.assertIn(str(package / 'mulib/__init__.on1x'), self.execute(
            OPTIONS.on1xc, '--deps', separate, '--no-prelude', '--stdlib', self.work / 'absent',
            '-I', package / 'mulib').splitlines())
        executable = self.build(client.parent, 'bin', '--stdlib', package) / 'consumer'
        self.assertEqual(self.execute(executable), '1\n1\npackaged prettyprint\n42\n1\n')
        rebuilt = self.build(package, 'rebuilt') / 'library'
        self.assertEqual({p.relative_to(package): p.read_bytes() for p in package.rglob('*') if p.is_file()},
                         {p.relative_to(rebuilt): p.read_bytes() for p in rebuilt.rglob('*') if p.is_file()})
        # A pervasive module in the input tree or in the compiler's installed
        # stdlib cannot fill a hole in the staged declarations.
        manifest = package / 'On1xfile.m'
        manifest.write_text(manifest.read_text().replace('library "pervasive.on1x"\n', ''))
        self.build(package, 'rebuilt', '--stdlib', STDLIB, negative='pervasive.on1x not found')
        self.assertTrue((rebuilt / 'pervasive.on1x').is_file())

    def test_intra_package_boundary(self):
        root = self.work / 'project'
        self.write('project/On1xfile', 'on1xfile 1\nkind library\nname "example"\ndirectory "lib"\n')
        self.write('project/lib/sub/On1xfile.m', 'on1xfile 1\nlibrary "uses.on1x"\n')
        self.write('project/lib/On1xfile.m', 'on1xfile 1\nlibrary "value.on1x"\n')
        original = self.write('project/lib/value.on1x', 'fn value() -> i64 { return 42; }\n')
        self.write('project/lib/private.on1x', original.read_text())
        self.write('project/value.on1x', original.read_text())
        subject = 'project/lib/sub/uses.on1x'
        self.write(subject, 'import "../value.on1x";\nfn answer() -> i64 { return value(); }\n')
        package = self.build(root) / 'library'
        before = (package / 'lib/sub/uses.on1x').read_bytes()
        for path, diagnostic in [
            ('../private.on1x', 'must name a declared library in the same package'),
            ('../../value.on1x', 'relative import escapes the declared package'),
            ('../sub/../value.on1x', 'expected a normalized local path'),
            ('..//value.on1x', 'expected a normalized local path'),
            ('.././value.on1x', 'expected a normalized local path'),
            (str(original), 'expected a normalized local path'),
        ]:
            with self.subTest(path=path):
                self.write(subject, f'import "{path}";\n')
                self.build(root, negative=diagnostic)
                self.assertEqual((package / 'lib/sub/uses.on1x').read_bytes(), before)
        self.write(subject, 'from ... import value;\n')
        self.build(root, negative='relative import escapes the declared package')
        self.write(subject, before.decode())
        original.unlink()
        original.symlink_to(root / 'value.on1x')
        self.build(root, negative='symlinks are not allowed')

    def test_resources_are_stdlib_only_and_cannot_be_symlinks(self):
        root = self.work / 'project'
        manifest = self.write('project/On1xfile', 'on1xfile 1\nkind library\nname "arbitrary"\n')
        self.write('project/On1xfile.m', 'on1xfile 1\nlibrary "pervasive.on1x"\n')
        self.write('project/pervasive.on1x', 'fn helper() {}')
        roles = self.write('project/runtime/On1xfile.m', 'on1xfile 1\nresource "basis_native.c"\n')
        self.build(root, negative='only registered standard-library resources')
        manifest.write_text(manifest.read_text().replace('"arbitrary"', '"std"'))
        roles.write_text('on1xfile 1\nresource "extra.c"\n')
        self.build(root, negative='only registered standard-library resources')
        roles.write_text('on1xfile 1\nresource "basis_native.c"\n')
        (roles.parent / 'basis_native.c').symlink_to(STDLIB / 'runtime/basis_native.c')
        self.build(root, negative='symlinks are not allowed')
        self.assertFalse((self.work / 'output').exists())

    def test_dependency_relative_import_stays_in_own_package(self):
        root = self.work / 'project'
        self.write('project/On1xfile', 'on1xfile 1\nkind executable\nname "sample"\n'
                   'directory "src"\ntarget "sample" "src/main.on1x"\n'
                   'dependency "toolkit" local "vendor/toolkit"\n'
                   'dependency "outside" local "vendor/outside"\n')
        self.write('project/src/On1xfile.m', 'on1xfile 1\nsource "main.on1x"\n')
        self.write('project/src/main.on1x', 'import toolkit.sub.uses as T;\nfn main(){println(T.answer());}\n')
        for name in ('toolkit', 'outside'):
            prefix = 'project/vendor/' + name
            self.write(prefix + '/On1xfile', f'on1xfile 1\nkind library\nname "{name}"\ndirectory "lib"\n')
            self.write(prefix + '/lib/On1xfile.m', 'on1xfile 1\nlibrary "value.on1x"\n')
            self.write(prefix + '/lib/value.on1x', 'fn value() -> i64 { return 42; }\n')
        self.write('project/vendor/toolkit/lib/sub/On1xfile.m', 'on1xfile 1\nlibrary "uses.on1x"\n')
        uses = 'project/vendor/toolkit/lib/sub/uses.on1x'
        self.write(uses, 'import "../value.on1x";\nfn answer() -> i64 { return value(); }\n')
        dest = self.build(root)
        self.assertEqual(self.execute(dest / 'sample'), '42\n')
        self.write(uses, 'import "../../outside/value.on1x";\nfn answer() -> i64 { return value(); }\n')
        self.build(root, negative='relative import escapes the declared package')
        self.assertEqual(self.execute(dest / 'sample'), '42\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--on1xc', type=lambda p: Path(p).resolve(), required=True)
    parser.add_argument('--cc', default='cc')
    parser.add_argument('--driver', type=lambda p: Path(p).resolve(), default=ON1X / 'tools/on1x-project.py')
    OPTIONS, rest = parser.parse_known_args()
    unittest.main(argv=[sys.argv[0], *rest], verbosity=2)
