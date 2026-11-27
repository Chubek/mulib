"""Independent On1xfile v1 membership, staging and graph tests (no compiler)."""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    'project_module_tests_driver', Path(__file__).resolve().parents[1] / 'tools/on1x-project.py')
project = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = project
spec.loader.exec_module(project)


class ModuleProjects(unittest.TestCase):
    def manifest(self, body):
        return project.root_manifest(
            ('on1xfile 1\nkind library\nname "example"\ndirectory "src"\n' + body).encode(),
            'On1xfile')

    def test_v1_membership_order(self):
        config = self.manifest('module "Math.Checked" "src/z.on1xm"\n'
                               'module-interface "Math.Checked" "src/y.on1xi"\n'
                               'module "Math.Checked" "src/a.on1xm"\n'
                               'module-interface "Math.Checked" "src/b.on1xi"\n'
                               'module-interface-policy "Math.Checked" required\n')
        member = config['modules']['Math.Checked']
        self.assertEqual(member.sources, (Path('src/z.on1xm'), Path('src/a.on1xm')))
        self.assertEqual(member.interfaces, (Path('src/y.on1xi'), Path('src/b.on1xi')))
        self.assertEqual(self.manifest(project.module_certificate(config['modules'])), config)

    def test_invalid_membership(self):
        cases = (
            ('module "X" "src/x.on1x"\n', 'requires a .on1xm path'),
            ('module "X" "src/x.on1xm"\nmodule "Y" "src/x.on1xm"\n', 'duplicate module membership'),
            ('module "X..Y" "src/x.on1xm"\n', 'invalid name'),
            ('module "X" "src/../x.on1xm"\n', 'normalized local path'),
            ('module "X" "other/x.on1xm"\n', 'outside the declared directory'),
            ('module "X" "src/build/x.on1xm"\n', 'inside a pruned directory'),
            ('module-interface "X" "src/x.on1xi"\n', 'requires at least one implementation'),
            ('module "X" "src/x.on1xm"\nmodule-interface-policy "X" required\n',
             'requires at least one interface'),
            ('module "X" "src/x.on1xm"\nmodule-interface "X" "src/a.on1xi"\n'
             'module-interface-policy "X" forbidden\n', 'forbids interface fragments'),
        )
        for text, message in cases:
            with self.subTest(text=text), self.assertRaisesRegex(ValueError, message):
                self.manifest(text)

    def test_stage_and_index_dependency_paths(self):
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)
            source, staged = root / 'source', root / 'staged'
            (source / 'src').mkdir(parents=True)
            manifest = ('on1xfile 1\nkind library\nname "example"\ndirectory "src"\n'
                        'module "X" "src/z.on1xm"\nmodule "X" "src/a.on1xm"\n'
                        'module-interface "X" "src/part.on1xi"\n')
            (source / 'On1xfile').write_text(manifest)
            for file in ('z.on1xm', 'a.on1xm', 'part.on1xi'):
                (source / 'src' / file).write_text(file)
            membership, snapshot = {}, project.Snapshot()
            roles = project.dependency_layout(project.Disk(source), 'example', staged, snapshot,
                                              membership=membership)
            self.assertEqual(roles, {Path('z.on1xm'): 'module', Path('a.on1xm'): 'module',
                                     Path('part.on1xi'): 'module-interface'})
            executor = type('Runner', (), {'path': staticmethod(lambda path: path)})
            index = root / 'index.json'
            flags, owners = project.module_index(
                executor, index, staged, {'example': (staged, membership, 'example.')})
            self.assertEqual(flags, ['--project-module-index', str(index)])
            self.assertEqual(owners[staged / 'a.on1xm'], ('example', 'X'))
            payload = json.loads(index.read_text())
            self.assertEqual(payload['project'], str(staged))
            record = payload['members']['example.X']
            self.assertEqual(record['fragments'], [str(staged / 'z.on1xm'), str(staged / 'a.on1xm')])
            self.assertEqual(record['interfaces'], [str(staged / 'part.on1xi')])
            self.assertIn('dependency/example/src/part.on1xi', snapshot.inputs)
            self.assertEqual((staged / 'part.on1xi').read_text(), 'part.on1xi')
            (source / 'src/a.on1xm').unlink()
            (source / 'src/a.on1xm').symlink_to('z.on1xm')
            with self.assertRaisesRegex(ValueError, 'symlinks are not allowed'):
                project.dependency_layout(project.Disk(source), 'example', root / 'retry', project.Snapshot())

    def test_program_members_require_local_library_role(self):
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)
            config = project.root_manifest(
                b'on1xfile 1\nkind executable\nname "app"\ntarget "app" "main.on1x"\n'
                b'program "Helpers.Value" "helper.on1x"\n', 'On1xfile')
            (root / 'On1xfile.m').write_text('on1xfile 1\nsource "main.on1x"\n')
            (root / 'main.on1x').touch()
            with self.assertRaisesRegex(ValueError, 'locally declared .on1x library input'):
                project.layouts(project.Disk(root), config, 'project', root / 'bad', project.Snapshot())
            (root / 'On1xfile.m').write_text('on1xfile 1\nsource "main.on1x"\nlibrary "helper.on1x"\n')
            (root / 'helper.on1x').touch()
            staged = root / 'stage'
            project.layouts(project.Disk(root), config, 'project', staged, project.Snapshot())
            executor = type('Runner', (), {'path': staticmethod(lambda path: path)})
            index = root / 'index.json'
            project.module_index(executor, index, staged, {}, config['programs'])
            self.assertEqual(json.loads(index.read_text())['programs'],
                             {'Helpers.Value': str(staged / 'helper.on1x')})

    def test_graph_uses_logical_identity_and_deterministic_order(self):
        members = {Path(k): value for k, value in {
            '/one': ('app', 'A'), '/two': ('app', 'A'), '/other': ('dep', 'B')}.items()}
        e = [(Path('/one'), Path('/other')), (Path('/two'), Path('/other'))]
        expected = [
            {'package': 'dep', 'name': 'B', 'dependencies': []},
            {'package': 'app', 'name': 'A', 'dependencies': [{'package': 'dep', 'name': 'B'}]},
        ]
        self.assertEqual(project.module_graph(members, e), expected)
        self.assertEqual(project.module_graph(dict(reversed(list(members.items()))), reversed(e)), expected)
        with self.assertRaisesRegex(ValueError, 'app:A -> dep:B -> app:A'):
            project.module_graph(members, [*e, (Path('/other'), Path('/two'))])

    def test_graph_report_must_include_all_declared_fragments(self):
        entry, module, missing = map(Path, ('/input/test.on1x', '/input/a.on1xm', '/input/z.on1xm'))
        answer = {'schema': 'on1x.project-module-graph/v1', 'sources': [str(entry), str(module)],
                  'edges': [[str(entry), str(module)]]}
        runner = type('Runner', (), {'run': staticmethod(lambda _, **__: json.dumps(answer).encode())})
        edges = set()
        with self.assertRaisesRegex(ValueError, 'did not load the declared fragment'):
            project.inspect_module_graph(runner, 'cc', entry, [], {entry, module, missing}, (), edges,
                                         (module, missing))
        with self.assertRaisesRegex(ValueError, 'outside the declared module graph'):
            project.inspect_module_graph(runner, 'cc', entry, [], {entry}, (), edges)
        project.inspect_module_graph(runner, 'cc', entry, [], {entry, module}, (), edges, (module,))
        self.assertEqual(edges, {(entry, module)})

    def test_module_header_agreement_uses_compiler_tokens(self):
        tokens = ('1:1  module\n1:8  Math\n1:12  .\n1:13  Checked\n1:20  ;\n').encode()
        runner = type('Runner', (), {'run': staticmethod(lambda _, **__: tokens),
                                    'path': staticmethod(lambda path: path)})
        project.source_policy(runner, 'cc', Path('/input/a.on1xm'), Path('/input'),
                              set(), declared_module='Math.Checked')
        with self.assertRaisesRegex(ValueError, 'source module header does not match'):
            project.source_policy(runner, 'cc', Path('/input/a.on1xm'), Path('/input'),
                                  set(), declared_module='Math.Other')

    def test_project_program_admission_has_no_no_index_fallback(self):
        tokens = [b'import', b'program', b'Helpers', b'.', b'Value', b':', b'value', b';']
        runner = type('Runner', (), {
            'run': staticmethod(lambda _, **__: b''.join(b'1:1  ' + token + b'\n' for token in tokens)),
            'path': staticmethod(lambda path: path)})
        entry, root = Path('/input/main.on1x'), Path('/input')
        with self.assertRaisesRegex(ValueError, 'program is not in the project index: Helpers.Value'):
            project.source_policy(runner, 'cc', entry, root, set())
        project.source_policy(runner, 'cc', entry, root, set(), programs={'Helpers.Value'})
        tokens[2:] = [b'"tool.on1x"', b':', b'value', b';']
        with self.assertRaisesRegex(ValueError, 'require an admitted program name'):
            project.source_policy(runner, 'cc', entry, root, {root / 'tool.on1x'})
        tokens[2:] = [b'as', b'p', b';']
        project.source_policy(runner, 'cc', entry, root, set())


if __name__ == '__main__':
    unittest.main()
