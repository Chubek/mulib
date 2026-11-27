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
        packet = {'nodes': [dict(id='b', kind='named', name='B', fragments=['/other']),
                            dict(id='a', kind='named', name='A', fragments=['/one', '/two'])],
                  'edges': [dict(source='a', target='b', witnesses=['r1', 'r2'])]}
        expected = [
            {'package': 'dep', 'name': 'B', 'dependencies': []},
            {'package': 'app', 'name': 'A', 'dependencies': [{'package': 'dep', 'name': 'B'}]},
        ]
        projection = project.ProjectProjection(members)
        projection.accept(packet, list(members))
        self.assertEqual(projection.report(), expected)
        projection.accept(packet, reversed(list(members)))
        self.assertEqual(projection.report(), expected)
        for mutation in (
            lambda p: p['nodes'].reverse(),
            lambda p: p['nodes'][0].update(id='a'),
            lambda p: p['nodes'][1]['fragments'].pop(),
            lambda p: p['nodes'][1].update(kind='legacy'),
            lambda p: p['edges'][0].update(target='absent'),
            lambda p: p['edges'][0].update(target='a'),
            lambda p: p['edges'][0].update(witnesses=[]),
            lambda p: p['edges'][0].update(witnesses=['r', 'r']),
            lambda p: p['edges'].clear(),
        ):
            import copy
            invalid = copy.deepcopy(packet)
            mutation(invalid)
            with self.assertRaises(ValueError):
                projection.accept(invalid, list(members))
            self.assertEqual(projection.report(), expected)

    def test_project_mode_requires_v3_without_fallback(self):
        path = Path('/staged/μ file')
        physical = dict(schema='on1x.project-module-graph/v2', sources=[str(path)],
                        edges=[], auxiliary=[], cacheable=True)
        packet = dict(schema='on1x.project-module-graph/v3', snapshot='sha256:'+'0'*64,
                      physical=physical, nodes=[dict(id='x', kind='named', name='X', fragments=[str(path)])], edges=[])
        def run(cmd, **_):
            self.assertEqual(cmd[1], '--project-module-graph-v3')
            return json.dumps(packet).encode()
        runner = type('Runner', (), {'run': staticmethod(run)})
        projection = project.ProjectProjection({path: ('pkg', 'X')})
        result = project.inspect_module_graph(runner, 'cc', path, [], {path}, (), projection)
        self.assertEqual(result.sources, (path,))
        self.assertEqual(projection.report(), [dict(package='pkg', name='X', dependencies=[])])
        packet['schema'] = 'on1x.project-module-graph/v4'
        with self.assertRaisesRegex(ValueError, 'invalid compiler project projection'):
            project.inspect_module_graph(runner, 'cc', path, [], {path}, (), projection)

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
        graph = project.inspect_module_graph(runner, 'cc', entry, [], {entry, module}, (), edges, (module,))
        self.assertEqual(edges, {(entry, module)})
        self.assertFalse(graph.cacheable)

    def test_graph_v2_keeps_signature_inputs_out_of_computational_edges(self):
        entry, module, signature, other = map(Path, (
            '/input/test.on1x', '/input/a.on1xm', '/input/a.on1xi', '/input/unlisted.on1xi'))
        graph = {'schema': 'on1x.project-module-graph/v2', 'sources': [str(entry), str(module)],
                 'edges': [[str(entry), str(module)]], 'auxiliary': [str(signature)], 'cacheable': True}
        runner = type('Runner', (), {'run': staticmethod(lambda _, **__: json.dumps(graph).encode())})
        edges = set()
        validated = project.inspect_module_graph(
            runner, 'cc', entry, [], {entry, module}, (), edges, (module,),
            auxiliary_permitted={signature}, expected_auxiliary=(signature,))
        self.assertTrue(validated.cacheable)
        self.assertEqual(validated.auxiliary, (signature,))
        self.assertEqual(edges, {(entry, module)})
        with self.assertRaisesRegex(ValueError, 'did not load the declared interface'):
            project.inspect_module_graph(
                runner, 'cc', entry, [], {entry, module}, (), set(),
                auxiliary_permitted={signature, other}, expected_auxiliary=(other,))
        graph['auxiliary'] = [str(other)]
        with self.assertRaisesRegex(ValueError, 'auxiliary input is outside'):
            project.inspect_module_graph(runner, 'cc', entry, [], {entry, module}, (), set(),
                                         auxiliary_permitted={signature})

    def test_std_module_is_owned_for_admission_but_not_project_index_transport(self):
        with tempfile.TemporaryDirectory() as work:
            staged = Path(work)
            member = project.ModuleMembership((Path('math.on1xm'),), (Path('math.on1xi'),), 'required')
            packages = {'std': (staged, {'std.math': member}, '')}
            executor = type('Runner', (), {'path': staticmethod(lambda path: path)})
            index = staged / 'index.json'
            with self.assertRaisesRegex(ValueError, 'namespace is reserved'):
                project.module_index(executor, index, staged, packages)
            flags, owners = project.module_index(executor, index, staged, packages, trusted_standard=True)
            self.assertEqual(flags, [])
            self.assertEqual(owners, {staged / 'math.on1xm': ('std', 'std.math')})
            self.assertFalse(index.exists())
            self.assertEqual(project.quoted_import(staged / 'check.on1x', 'math.on1x', staged, (),
                                                   standard_aliases={staged / 'math.on1x'}),
                             staged / 'math.on1x')
            with self.assertRaisesRegex(ValueError, 'must name a declared library'):
                project.quoted_import(staged / 'check.on1x', 'math.on1x', staged, ())

    def test_module_cache_transitive_policy_profile_and_failed_check(self):
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)
            staged, dependency = root / 'project', root / 'dependency'
            staged.mkdir()
            dependency.mkdir()
            carrier, module, unrelated = (staged / name for name in ('carrier.on1x', 'app.on1xm', 'other.on1xm'))
            imported, signature = (dependency / name for name in ('dep.on1xm', 'dep.on1xi'))
            prelude = root / 'pervasive.on1x'
            for path in (carrier, module, imported, signature, prelude, unrelated):
                path.write_text(path.name)
            graph = project.ModuleGraphInputs((prelude, imported, module, carrier),
                                             ((carrier, module), (module, imported)), (signature,), True)
            roots = [('project', staged), ('dependency/dep', dependency)]
            rule = {'policy': 'optional'}
            membership = {imported: ('dependency/dep:Inner', rule)}
            versions = {'dependency/dep': {'commit': '0000'}}
            context = {'project': str(staged), 'backend': 'c11', 'tools': {'on1xc': 'A'}}
            checks = []
            with project.module_cache_directory(root / 'cache', staged, None) as fd:
                cache = project.ModuleCheckCache(fd, context, roots, membership, versions)
                def check():
                    cache.check('', 'App', graph, lambda: checks.append('verified'))

                check()
                check()
                unrelated.write_text('not imported')
                check()
                self.assertEqual(len(checks), 1)
                for count, path in enumerate((module, imported, signature, prelude), 2):
                    path.write_text('changed')
                    check()
                    self.assertEqual(len(checks), count)
                for count, (record, key, value) in enumerate((
                        (rule, 'policy', 'required'), (versions['dependency/dep'], 'commit', '0001'),
                        (context, 'backend', 'mir-c11'), (context['tools'], 'on1xc', 'B')), 6):
                    record[key] = value
                    check()
                    self.assertEqual(len(checks), count)
                context['backend'] = 'mir-vm'
                with self.assertRaisesRegex(ValueError, 'invalid body'):
                    cache.check('', 'App', graph, lambda: project.fail('invalid body'))
                check()
                self.assertEqual(len(checks), 10)
                foreign = project.ModuleGraphInputs(graph.sources, graph.edges, graph.auxiliary, False)
                cache.check('', 'App', foreign, lambda: checks.append('unchecked profile'))
                cache.check('', 'App', foreign, lambda: checks.append('unchecked profile'))
                self.assertEqual(len(checks), 12)

    def test_cache_directory_rejects_untrusted_locations(self):
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)
            staged = root / 'project'
            staged.mkdir()
            with self.assertRaisesRegex(ValueError, 'must be separate'):
                with project.module_cache_directory(staged / '.on1x-cache', staged, None):
                    self.fail('accepted cache content from the project checkout')
            cache = root / 'cache'
            cache.mkdir()
            cache.chmod(0o755)
            with self.assertRaisesRegex(ValueError, 'must be private'):
                with project.module_cache_directory(cache, staged, None):
                    self.fail('accepted a public cache')
            link = root / 'linked'
            link.symlink_to(cache, target_is_directory=True)
            with self.assertRaises(OSError):
                with project.module_cache_directory(link, staged, None):
                    self.fail('accepted a cache symlink')

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
        with self.assertRaisesRegex(ValueError, 'quoted program is not in the project index'):
            project.source_policy(runner, 'cc', entry, root, {root / 'tool.on1x'})
        project.source_policy(runner, 'cc', entry, root, {root / 'tool.on1x'},
                              program_paths={root / 'tool.on1x'})
        tokens[2:] = [b'as', b'p', b';']
        project.source_policy(runner, 'cc', entry, root, set())


if __name__ == '__main__':
    unittest.main()
