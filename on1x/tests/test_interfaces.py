#!/usr/bin/env python3
"""V1 .on1xi conformance through the real loader, checker and project C11 path."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ON1X = Path(__file__).resolve().parents[1]
OPTIONS = None


class InterfaceTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix='on1x-interface-tests-')
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)

    def write(self, filename, text):
        path = self.root / filename
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding='utf-8')
        return path

    def run_command(self, command, *, error=None):
        result = subprocess.run(command, cwd=self.root, text=True, capture_output=True, timeout=90)
        if error is None:
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertEqual(result.stdout, '')
            self.assertRegex(result.stderr, error)
        return result

    def compiler(self, *args, **kwargs):
        return self.run_command([OPTIONS.on1xc, '--stdlib', ON1X / 'stdlib', *args], **kwargs)

    def check(self, implementation, *interfaces, error=None):
        return self.compiler('--check-interfaces', implementation,
                             *(word for path in interfaces for word in ('--interface', path)),
                             error=error)

    def project(self, *args, root=None, error=None):
        driver = ON1X / 'tools/on1x-project.py'
        return self.run_command([sys.executable, driver, 'build', root or self.root,
                                 '--on1xc', OPTIONS.on1xc, '--cc', OPTIONS.cc,
                                 '--stdlib', ON1X / 'stdlib', *args], error=error)

    def application(self, extra='', kind='executable'):
        self.write('On1xfile', f'on1xfile 1\nkind {kind}\nname "app"\ndirectory "src"\n'
                   'target "app" "src/main.on1x"\n' + extra)
        self.write('src/On1xfile.m', 'on1xfile 1\nsource "main.on1x"\n')
        self.write('src/main.on1x', 'fn main() -> i64 { println(42); return 0; }\n')

    def test_project_c11_with_unsealed_exports(self):
        self.application()
        self.write('src/On1xfile.m', 'on1xfile 1\nsource "main.on1x"\nlibrary "messages.on1x"\n')
        self.write('src/On1xfile.i', 'on1xfile 1\ninterface "messages.on1xi"\ninterface "main.on1xi"\n')
        self.write('src/main.on1xi', 'on1xi 1;\nfn main() -> i64;\n')
        self.write('src/messages.on1xi', '''# Ordinary On1x scanner/layout
on1xi 1; # v1
fn pack(count: i64, word: string) -> (i64, (string, bool));
fn print_answer(values: (i64, i64)) -> void;
''')
        self.write('src/messages.on1x', '''
fn pack(n: i64, s: string) -> (i64, (string, bool)) { return (n, (s, true)); }
fn print_answer((n, extra): (i64, i64)) { println(n + extra); }
fn extra() -> i64 { return 2; }
''')
        self.write('src/main.on1x', '''
import messages as Message;
fn main() -> i64 {
    let (n, (word, ok)) = Message.pack(40, "on1xi");
    assert(ok); println(word);
    Message.print_answer((n, Message.extra()));
    return 0;
}
''')
        dest = Path(self.project().stdout.strip())
        self.assertEqual(self.run_command([dest / 'app']).stdout, 'on1xi\n42\n')
        marker = dest / '.on1x-build.json'
        before = marker.read_bytes()
        self.assertIn('project/src/messages.on1xi', json.loads(before)['inputs'])

        self.write('src/messages.on1xi', 'on1xi 1;\nfn print_answer(value: bool) -> void;\n')
        self.project(error=r'messages\.on1xi:2:4: error: interface signature mismatch.*found fn\(\(i64, i64\)\)')
        self.assertEqual(marker.read_bytes(), before)
        self.assertEqual(self.run_command([dest / 'app']).stdout, 'on1xi\n42\n')

    def test_identity_and_original_definition(self):
        left = self.write('left/value.on1x', 'fn value() -> i64 { return 42; }\n')
        left_i = self.write('left/value.on1xi', 'on1xi 1;\nfn value() -> i64;')
        right = self.write('right/value.on1x', 'fn value() -> bool { return true; }\n')
        right_i = self.write('right/value.on1xi', 'on1xi 1;\nfn value() -> bool;')
        app = self.write('main.on1x', '''
import left.value as L, right.value as R;
fn main() { assert(R.value()); println(L.value()); }
''')
        self.assertIn('2 interface members', self.check(app, left_i, right_i).stdout)
        self.check(app, left_i, left_i, error='duplicate interface file')
        self.check(right, left_i, error='matching implementation is not in the loaded graph')
        self.check(left, left_i)
        self.compiler('--check', left, error='missing main function')
        self.write('left/value.on1x', 'from right.value import value;')
        self.check(app, left_i, error='externs and re-exports do not satisfy interfaces')
        self.write('left/value.on1x', 'extern fn value() -> i64;')
        self.check(app, left_i, error='externs and re-exports do not satisfy interfaces')
        self.write('left/value.on1x', 'fn value() -> bool { return true; }')
        self.check(app, left_i, error=r'left/value\.on1xi:2:4: error: interface signature mismatch'
                                      r'.*expected fn\(\) -> i64, found fn\(\) -> bool')

    def test_missing_duplicate_and_inferred_implementation_members(self):
        interface = self.write('values.on1xi', 'on1xi 1;\nfn next(n: i64) -> i64;')
        for text, error in (
            ('fn other(n:i64) -> i64 { return n; }', "expected one local On1x definition of 'next'"),
            ('fn next(n:i64) -> i64 { return n; }\nfn next(n:i64) -> i64 { return n; }',
             "expected one local On1x definition of 'next'"),
            ('fn next(n) -> i64 { return n+1; }', 'requires a fully annotated signature'),
            ('fn next(n:i64) -> _ { return n+1; }', 'requires a fully annotated signature'),
            ('fn next(n:i64) -> i64 { return false; }', r'values\.on1x:1:\d+: error:.*bool'),
            ('fn next() -> i64 { return 1; }', r'interface signature mismatch.*found fn\(\)'),
        ):
            with self.subTest(text=text):
                file = self.write('values.on1x', text)
                self.check(file, interface, error=error)

    def test_unsupported_signature_syntax_fails_closed(self):
        implementation = self.write('library.on1x', 'fn answer() -> i64 { return 42; }\n')
        for text, error in (
            ('an opaque interface input', r'library\.on1xi:1:1: error: expected interface header'),
            ('on1xi 2;\nfn answer() -> i64;', r'library\.on1xi:1:7: error: unsupported interface version'),
            ('on1xi 1;', 'requires at least one function'),
            ('on1xi 1;\nimport dangerous;', 'accepts only'),
            ('on1xi 1;\nextern fn answer() -> i64;', 'accepts only'),
            ('on1xi 1;\n@ppx tool "/bin/false";', 'accepts only'),
            ('on1xi 1;\nfn answer() -> i64 { return 42; }', "expected ';'"),
            ('on1xi 1;\nfn answer() -> i64;\nfn answer() -> i64;',
             r'library\.on1xi:3:4: error: duplicate interface member'),
            ('on1xi 1;\nfn answer(a:i64, a:bool) -> i64;', 'duplicate interface parameter'),
            ('on1xi 1;\nfn answer(a:i64);', "expected '->'"),
            ('on1xi 1;\nfn answer(a) -> i64;', "expected ':'"),
            ('on1xi 1;\nfn answer(a:fn(i64) -> i64) -> i64;', 'interface v1 expects'),
            ('on1xi 1;\nfn answer(a:forall(t) fn(t) -> t) -> i64;', 'interface v1 expects'),
            ('on1xi 1;\nfn answer(a:void) -> i64;', 'interface v1 expects'),
            ('on1xi 1;\nfn answer() -> (i64, void);', 'interface v1 expects'),
            ('on1xi 1;\nfn answer(a:_) -> i64;', 'interface v1 expects'),
            ('on1xi 1;\nfn answer() -> pkg.Value;', 'interface v1 expects'),
            ('on1xi 1;\nfn answer(a:channel<i64>) -> i64;', 'interface v1 expects'),
            ('on1xi 1;\nfn answer() -> (i64,' + ','.join(['i64'] * 32) + ');',
             'interface tuple width limit exceeded'),
            ('on1xi 1;\nfn answer(' + ','.join(f'a{i}:i64' for i in range(33)) + ') -> i64;',
             'interface parameter limit exceeded'),
            ('on1xi 1;\n' + ''.join(f'fn f{i}() -> i64;\n' for i in range(257)),
             'interface member limit exceeded'),
        ):
            with self.subTest(text=text[:120]):
                interface = self.write('library.on1xi', text)
                self.check(implementation, interface, error=error)

    def test_declared_pair_must_be_in_same_staged_directory(self):
        self.application()
        self.write('src/other.on1x', 'fn number() -> i64 { return 42; }\n')
        self.write('src/On1xfile.i', 'on1xfile 1\ninterface "other.on1xi"\n')
        self.write('src/other.on1xi', 'on1xi 1;\nfn number() -> i64;')
        self.write('src/nested/On1xfile.m', 'on1xfile 1\nlibrary "other.on1x"\n')
        self.write('src/nested/other.on1x', 'fn number() -> i64 { return 42; }\n')
        self.project(error=r'project/src/On1xfile\.i:2:.*requires a declared same-directory implementation')
        self.assertFalse((self.root / 'target/dev').exists())

    def test_unreached_interface_uses_only_staged_modules(self):
        self.application()
        self.write('src/On1xfile.m', 'on1xfile 1\nsource "main.on1x"\nlibrary "unreached.on1x"\n')
        self.write('src/On1xfile.i', 'on1xfile 1\ninterface "unreached.on1xi"\n')
        self.write('src/unreached.on1xi', 'on1xi 1;\nfn answer() -> i64;\n')
        self.write('src/unreached.on1x', 'import helper as H;\nfn answer() -> i64 { return H.number(); }\n')
        self.write('src/helper.on1x', 'fn number() -> i64 { return 42; }\n')
        self.project(error='module not found: helper')
        with (self.root / 'src/On1xfile.m').open('a', encoding='utf-8') as manifest:
            manifest.write('library "helper.on1x"\n')
        self.project()

    def test_package_and_unused_dependencies_are_checked(self):
        self.application('dependency "toolkit" local "vendor/toolkit"\n')
        dep = self.root / 'vendor/toolkit'
        self.write('vendor/toolkit/On1xfile',
                   'on1xfile 1\nkind library\nname "toolkit"\ndirectory "modules"\n')
        self.write('vendor/toolkit/modules/On1xfile.m', 'on1xfile 1\nlibrary "reply.on1x"\n')
        self.write('vendor/toolkit/modules/On1xfile.i', 'on1xfile 1\ninterface "reply.on1xi"\n')
        self.write('vendor/toolkit/modules/reply.on1x', 'fn reply() -> bool { return true; }\n')
        self.write('vendor/toolkit/modules/reply.on1xi', 'on1xi 1;\nfn reply() -> i64;\n')
        self.project(root=dep, error='interface signature mismatch')
        self.project(error='interface signature mismatch')
        self.write('vendor/toolkit/modules/reply.on1xi', 'on1xi 1;\nfn reply() -> bool;\n')
        package = Path(self.project(root=dep).stdout.strip()) / 'library'
        interface = package / 'modules/reply.on1xi'
        self.assertEqual(interface.read_text(), 'on1xi 1;\nfn reply() -> bool;\n')
        self.write('On1xfile', (self.root / 'On1xfile').read_text().replace(
            'vendor/toolkit', 'vendor/toolkit/target/dev/library'))
        self.write('src/main.on1x', 'import toolkit.reply as T;\nfn main() { println(T.reply()); }\n')
        dest = Path(self.project().stdout.strip())
        self.assertEqual(self.run_command([dest / 'app']).stdout, 'true\n')
        interface.write_text('on1xi 1;\nfn reply() -> i64;\n')
        self.project(error='interface signature mismatch')

    def test_selected_target_does_not_skip_a_declared_interface(self):
        self.application('target "extra" "src/extra.on1x"\n', kind='complex')
        self.write('src/On1xfile.m', 'on1xfile 1\nsource "main.on1x"\nsource "extra.on1x"\n')
        self.write('src/extra.on1x', 'fn main() -> i64 { return 0; }\n')
        self.write('src/On1xfile.i', 'on1xfile 1\ninterface "extra.on1xi"\n')
        self.write('src/extra.on1xi', 'on1xi 1;\nfn main() -> bool;\n')
        self.project('--target', 'app', error=r'extra\.on1xi:2:4: error: interface signature mismatch')

    def test_check_mode_is_not_a_build_mode_and_never_runs_ppx(self):
        implementation = self.write('values.on1x', 'fn next(n:i64) -> i64 { return n+1; }')
        interface = self.write('values.on1xi', 'on1xi 1;\nfn next(n:i64) -> i64;')
        self.compiler('--check-interfaces', implementation, error='must be used together')
        self.compiler('--emit-c', implementation, '--interface', interface, error='must be used together')
        self.compiler('--check-interfaces', implementation, '--interface', interface, '-o', 'out.c',
                      error='without output')
        self.assertFalse((self.root / 'out.c').exists())
        self.write('values.on1x', '@ppx bad "/bin/false";\nfn next(n:i64) -> i64 { return n+1; }')
        self.check(implementation, interface, error='does not execute PPX')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--on1xc', type=lambda path: str(Path(path).resolve()), required=True)
    parser.add_argument('--cc', default='cc')
    OPTIONS, args = parser.parse_known_args()
    unittest.main(argv=[sys.argv[0], *args])
