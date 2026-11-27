"""Standalone full-driver gate: python3 test_graph_driver.py /path/to/on1xc."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1]).resolve(strict=True)
    with tempfile.TemporaryDirectory(dir=Path.cwd()) as temp:
        root = Path(temp)
        entry = root / "μ graph.on1x"
        probe = root / "probe"
        marker = root / "executed"
        probe.write_text(f'#!/bin/sh\ntouch "{marker}"\ncat\n')
        probe.chmod(0o700)
        entry.write_text('import "dep.on1x"; fn main(){}')
        (root / 'dep.on1x').write_text('fn answer()->i64{return 42;}')

        def run(*args, ok=True):
            p = subprocess.run([compiler, '--no-prelude', *args, entry], cwd=root,
                               capture_output=True, timeout=10)
            assert (p.returncode == 0) == ok, (args, p.returncode, p.stderr)
            assert not marker.exists(), 'graph inspection executed PPX'
            return p.stdout

        for mode, schema in [('import', 'import'), ('module', 'module-dependency')]:
            flag = f'--emit-{mode}-graph'
            raw = run(flag)
            assert raw == run(flag, '--graph-format', 'json')
            graph = json.loads(raw)
            assert graph['schema'] == f'on1x.{schema}-graph/v1' and graph['status'] == 'resolved'
            for fmt in ('sexpr', 'dot'):
                assert run(flag, '--graph-format', fmt)
        assert run('--emit-ppx-graph-v1').startswith(b'(SourceModuleGraphMetadata 1 ')
        for target in (entry, root / 'dep.on1x'):
            before = target.read_bytes()
            run('--emit-module-graph', '-o', target, ok=False)
            assert target.read_bytes() == before
        # Atomic publication must not follow a symlink or block on a FIFO.
        alias_output = root / 'linked.json'
        alias_output.symlink_to(root / 'dep.on1x')
        before = (root / 'dep.on1x').read_bytes()
        run('--emit-module-graph', '-o', alias_output, ok=False)
        assert alias_output.is_symlink() and (root / 'dep.on1x').read_bytes() == before
        fifo = root / 'fifo.json'
        os.mkfifo(fifo)
        run('--emit-module-graph', '-o', fifo, ok=False)
        assert not list(root.glob('.on1x-graph-*'))
        for args in [('--emit-module-graph', '--deps'), ('--graph-format', 'json'),
                      ('--emit-import-graph', '--ppx', 'probe', probe),
                      ('--emit-module-graph', '--allow-source-ppx'),
                      ('--emit-module-graph', '--ir-input'),
                      ('--emit-import-graph', '--memory-check'),
                      ('--emit-module-graph', '--verify-types')]:
            run(*args, ok=False)
        # The project projection must use compiler quotient edges even when
        # several fragment requests witness the same logical dependency.
        wrapper = [root / 'api.on1xm', root / 'body.on1xm']
        dep = root / 'dep.on1xm'
        def member(name, paths):
            return dict(package=str(root), declared_name=name, fragments=list(map(str, paths)), interfaces=[])
        for i, path in enumerate(wrapper):
            path.write_text(f'module A.Wrapper; import A.Value as V; public fn f{i}()->i64{{return V.value();}}')
        dep.write_text('module A.Value; public fn value()->i64{return 42;}')
        entry.write_text('import A.Wrapper; fn main(){}')
        index = root / 'membership.json'
        index.write_text(json.dumps(dict(schema='on1x.project-modules/v1', project=str(root), programs={},
            members={'A.Wrapper': member('A.Wrapper', wrapper), 'A.Value': member('A.Value', [dep])})))
        args = ['--project-module-index', index]
        payload = run('--project-module-graph-v3', *args)
        packet = json.loads(payload)
        assert packet['physical'] == json.loads(run('--project-module-graph', *args))
        by_name = {n['name']: n['id'] for n in packet['nodes']}
        witness, = [e for e in packet['edges'] if (e['source'], e['target']) ==
                    (by_name['A.Wrapper'], by_name['A.Value'])]
        assert len(witness['witnesses']) == 2
        spec = importlib.util.spec_from_file_location('projection_driver',
            Path(__file__).resolve().parents[1] / 'tools/on1x-project.py')
        project = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = project
        spec.loader.exec_module(project)
        owners = {p: ('pkg', 'A.Wrapper') for p in wrapper}
        owners[dep] = ('pkg', 'A.Value')
        projection = project.ProjectProjection(owners)
        class Runner:
            def run(self, cmd, **kwargs):
                return run(cmd[1], *cmd[3:])
        project.inspect_module_graph(Runner(), compiler, entry, args, {*owners, entry}, (), projection)
        assert projection.report() == [dict(package='pkg', name='A.Value', dependencies=[]),
            dict(package='pkg', name='A.Wrapper', dependencies=[dict(package='pkg', name='A.Value')])]
        entry.write_text('@ppx probe "./probe"; fn main(){}')
        run('--emit-module-graph', '--graph-partial')
        entry.write_text('import "absent.on1x"; fn main(){}')
        before = entry.read_bytes()
        run('--emit-module-graph', '--graph-partial', '-o', entry, ok=False)
        assert entry.read_bytes() == before
        # A failed observational transaction must not overwrite a provider
        # discovered before the missing import, even though no graph is returned.
        entry.write_text('import "dep.on1x"; import "absent.on1x"; fn main(){}')
        before = (root / 'dep.on1x').read_bytes()
        run('--emit-module-graph', '--graph-partial', '-o', root / 'dep.on1x', ok=False)
        assert (root / 'dep.on1x').read_bytes() == before
        output = root / 'previous.json'
        output.write_text('previous')
        run('--emit-module-graph', '-o', output, ok=False)
        assert output.read_text() == 'previous'
        failure = json.loads(run('--emit-module-graph', '--graph-partial'))
        assert failure['status'] == 'partial' and failure['schema'] == 'on1x.graph-failure/v1'
        assert failure['diagnostics'] and 'nodes' not in failure


if __name__ == '__main__':
    main()
