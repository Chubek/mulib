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

from stdlib_module_inventory import collect

ON1X = Path(__file__).resolve().parents[1]
STDLIB = ON1X / 'stdlib'
RESOURCES = {
    'runtime/basis_native.c', 'runtime/basis_native.h', 'runtime/basis_real.c',
    'environ/runtime.c',
    'cephyr/runtime.c',
    'daemon/supervisor.c', 'daemon/supervisor.h', 'daemon/lifecycle_kernels.c',
    'elvi/runtime.c', 'elvi/runtime.h', 'elvi/bridge.c', 'elvi/bridge.h',
    'elvi/capture.c', 'elvi/capture.h', 'elvi/kernels.c',
    'elvi/script.c', 'elvi/script.h', 'elvi/live.c', 'elvi/live.h',
    'html/runtime.c', 'html/runtime.h', 'html/kernels.c',
    'http/runtime.c', 'http/runtime.h', 'http/kernels.c',
    'http/probe/context.c', 'http/probe/context.h',
    'http/probe/supervised.c', 'http/probe/supervised.h',
    'http/probe/artifact.c', 'http/probe/artifact.h',
    'kron/runtime.c', 'kron/runtime.h', 'kron/kernels.c',
    'prettyprint/runtime.c', 'prettyprint/runtime.h',
    'tty/runtime.c', 'tty/runtime.h',
    'container/runtime.c', 'container/runtime.h',
    'container/shared.c', 'container/on1x_container_shared.h',
    'container/pod_kernel.h', 'container/pod_runtime.c', 'container/pod_runtime.h',
    'container/pod_worker.c',
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

    def test_package_entry_points(self):
        for roles in sorted(STDLIB.rglob('On1xfile.m')):
            directory = roles.parent.relative_to(STDLIB)
            declarations = roles.read_text(encoding='utf-8').splitlines()
            if not directory.parts or not any(line.startswith('library "') for line in declarations):
                continue
            with self.subTest(package=directory):
                # The source bundle deliberately exposes only the daemon's
                # pure lifecycle types, not its legacy KernelBind initializer.
                if directory == Path('daemon'):
                    self.assertNotIn('library "__init__.on1x"', declarations)
                    entry = 'lifecycle.on1x'
                    module = 'daemon.lifecycle'
                else:
                    entry = '__init__.on1x'
                    module = '.'.join(directory.parts)
                self.assertIn(f'library "{entry}"', declarations)
                source = self.write('entry.on1x', f'import std.{module} as Package;\nfn main() {{}}\n')
                graph = self.execute(OPTIONS.on1xc, '--deps', source, '--stdlib', STDLIB).splitlines()
                self.assertIn(str(roles.parent / entry), graph)
                self.execute(OPTIONS.on1xc, '--check', source, '--stdlib', STDLIB)

    def test_package_exports(self):
        source = self.write('exports.on1x', '''
import std.base as Base;
import std.numeric as Numeric;
import std.parsec.parsec as Original;
from std.parsec import basis_parsec_int;
from std import sexpr;
fn main() {
    let (ok, value, end) = basis_parsec_int("-42!", 0);
    assert(ok && value == -42 && end == 3);
    let (bad, _, rollback) = Original.basis_parsec_int("!", 0);
    assert(!bad && rollback == 0);
    assert(Numeric.Int.basis_int_div(-7, 3) == -3);
    assert(Base.option.base_is_some(Base.option.base_some(sx_string("ok"))));
    assert(Base.base_identity_i64(42) == 42);
    assert(sexpr.sexpr_size(sexpr.sexpr_parse("(one two)")) == 2);
}
''')
        exe = self.work / 'exports'
        self.execute(sys.executable, ON1X / 'tools/on1x-build.py', source,
                     '--on1xc', OPTIONS.on1xc, '--cc', OPTIONS.cc, '--stdlib', STDLIB, '-o', exe)
        self.execute(exe)
        child = self.write('child.on1x', 'import std.parsec.parsec as Parse;\nfn main() {}\n')
        graph = self.execute(OPTIONS.on1xc, '--deps', child, '--stdlib', STDLIB).splitlines()
        self.assertNotIn(str(STDLIB / 'parsec/__init__.on1x'), graph)
        private = self.write('private.on1x', 'from std.ini import on1x_mulib_ini_get;\nfn main() {}\n')
        self.execute(OPTIONS.on1xc, '--check', private, '--stdlib', STDLIB,
                     negative="no export or submodule 'on1x_mulib_ini_get'")

    def test_legacy_parsec_paths_share_one_implementation(self):
        implementation = (STDLIB / 'parsec/parsec.on1x').as_posix()
        initializer = (STDLIB / 'parsec/__init__.on1x').as_posix()
        source = self.write('mixed.on1x', f'''
import "{implementation}";
import std.parsec as Entry;
import std.parsec.parsec as Body;
from std.parsec import basis_parsec_int as scan;
fn main() {{
    let (ok, value, end) = basis_parsec_int("-9223372036854775808!", 0);
    assert(ok && value == -9223372036854775808 && end == 20);
    let (overflow, _, reset) = scan("9223372036854775808", 0);
    assert(!overflow && reset == 0);
    assert(Entry.parsec_literal("on1x", 0, "on"));
    assert(!Body.parsec_literal("on1x", 0, "one"));
}}
''')
        graph = self.execute(OPTIONS.on1xc, '--deps', source, '--stdlib', STDLIB).splitlines()
        self.assertEqual(graph.count(implementation), 1)
        self.assertEqual(graph.count(initializer), 1)
        exe = self.work / 'mixed'
        self.execute(sys.executable, ON1X / 'tools/on1x-build.py', source,
                     '--on1xc', OPTIONS.on1xc, '--cc', OPTIONS.cc, '--stdlib', STDLIB, '-o', exe)
        self.execute(exe)

    def test_initializer_survives_packaging(self):
        root = self.work / 'parsec-stdlib'
        self.write('parsec-stdlib/On1xfile', 'on1xfile 1\nkind library\nname "std"\n')
        self.write('parsec-stdlib/On1xfile.m', 'on1xfile 1\nlibrary "pervasive.on1x"\n')
        self.write('parsec-stdlib/pervasive.on1x', 'fn unused() {}\n')
        shutil.copytree(STDLIB / 'parsec', root / 'parsec')
        package = (self.build(root) / 'library').rename(self.work / 'relocated parsec stdlib')
        source = self.write('consumer.on1x', '''
import std.parsec as Parse;
fn main() { assert(Parse.parsec_literal("on1x", 0, "on")); }
''')
        graph = self.execute(OPTIONS.on1xc, '--deps', source, '--stdlib', package).splitlines()
        self.assertIn(str(package / 'parsec/__init__.on1x'), graph)
        self.assertIn(str(package / 'parsec/parsec.on1x'), graph)
        self.execute(OPTIONS.on1xc, '--check', source, '--stdlib', package)

    def test_whole_stdlib_and_downstream(self):
        dest = self.build(STDLIB)
        package = (dest / 'library').rename(self.work / 'relocated stdlib')
        report = json.loads((dest / '.on1x-build.json').read_bytes())
        self.assertEqual(report['outputs'], {
            'library/' + path.relative_to(package).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in package.rglob('*') if path.is_file()
        })
        # The restricted source package does not distribute the legacy
        # KernelBind daemon entry point, the separately hosted SDK source
        # layer, or test fixtures.
        modules = {
            entry['path'] for entry in collect(STDLIB)
            if entry['bundle_role'] in ('library', 'interface')
        }
        # The empty MCP initializer is a source-tree placeholder, not a
        # declared library module. Keep checking all other modules.
        mcp_placeholder = 'mcp/__init__.on1x'
        self.assertEqual(modules, {
            p.relative_to(package).as_posix() for p in package.rglob('*')
            if p.is_file() and p.suffix in ('.on1x', '.on1xm', '.on1xi')
        })
        self.assertNotIn('project/' + mcp_placeholder, report['inputs'])
        mcp_client = self.write('mcp-client.on1x', 'import std.mcp;\nfn main() {}\n')
        self.execute(OPTIONS.on1xc, '--check', mcp_client, '--stdlib', package,
                     negative='standard module not found: std.mcp')
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
import std.ini as I;
import std.http as Http;
from std import mulib;
fn main() {
    assert(Http.success(200) && !Http.success(404));
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
        self.assertIn(package / 'http/__init__.on1x', graph)
        self.assertNotIn(package / 'http/client.on1x', graph)
        separate = self.write('standalone.on1x', 'import mulib as M;\nfn main(){assert(M.fits_size(42));}\n')
        self.assertIn(str(package / 'mulib/__init__.on1x'), self.execute(
            OPTIONS.on1xc, '--deps', separate, '--no-prelude', '--stdlib', self.work / 'absent',
            '-I', package / 'mulib').splitlines())
        executable = self.build(client.parent, 'bin', '--stdlib', package) / 'consumer'
        self.assertEqual(self.execute(executable), '1\n1\npackaged prettyprint\n42\n1\n')
        if sys.platform.startswith('linux'):
            self.execute(sys.executable, '-B', ON1X / 'tests/test_http_probe.py',
                         '--on1xc', OPTIONS.on1xc, '--cc', OPTIONS.cc,
                         '--stdlib', package)
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

    def test_environ_resource_in_relocated_package(self):
        root = self.work / 'process-stdlib'
        self.write('process-stdlib/On1xfile', 'on1xfile 1\nkind library\nname "std"\n')
        self.write('process-stdlib/On1xfile.m', 'on1xfile 1\nlibrary "pervasive.on1x"\n')
        self.write('process-stdlib/pervasive.on1x', 'fn unused() {}\n')
        shutil.copytree(STDLIB / 'environ', root / 'environ')
        package = (self.build(root) / 'library').rename(self.work / 'relocated process stdlib')
        self.assertEqual((package / 'environ/runtime.c').read_bytes(),
                         (STDLIB / 'environ/runtime.c').read_bytes())
        self.write('process-client/On1xfile', 'on1xfile 1\nkind executable\nname "consumer"\n'
                   'target "consumer" "consumer.on1x"\n')
        self.write('process-client/On1xfile.m', 'on1xfile 1\nsource "consumer.on1x"\n')
        client = self.write('process-client/consumer.on1x',
                            'import std.environ as E;\nfn main(){println(E.argc());}\n')
        executable = self.build(client.parent, 'bin', '--stdlib', package) / 'consumer'
        self.assertEqual(self.execute(executable, 'argument'), '2\n')

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
