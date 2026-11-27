"""Standalone full-driver gate: python3 test_graph_driver.py /path/to/on1xc."""
import json
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
        for args in [('--emit-module-graph', '--deps'), ('--graph-format', 'json'),
                     ('--emit-import-graph', '--ppx', 'probe', probe),
                     ('--emit-module-graph', '--allow-source-ppx')]:
            run(*args, ok=False)
        entry.write_text('@ppx probe "./probe"; fn main(){}')
        run('--emit-module-graph', '--graph-partial')
        entry.write_text('import "absent.on1x"; fn main(){}')
        output = root / 'previous.json'
        output.write_text('previous')
        run('--emit-module-graph', '-o', output, ok=False)
        assert output.read_text() == 'previous'
        failure = json.loads(run('--emit-module-graph', '--graph-partial'))
        assert failure['status'] == 'partial' and failure['schema'] == 'on1x.graph-failure/v1'
        assert failure['diagnostics'] and 'nodes' not in failure


if __name__ == '__main__':
    main()
