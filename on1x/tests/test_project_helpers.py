"""Exercise the installed-shape build-time Mune and optional QMR adapters."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def run(*args, **kwargs):
    return subprocess.run(args, text=True, capture_output=True, timeout=15, **kwargs)


mune, stdlib, *qmr = map(Path, sys.argv[1:])
with tempfile.TemporaryDirectory() as directory:
    policy = Path(directory) / 'policy.lua'
    policy.write_text('return on1xfile.profile { optimize = 2, debug = true }\n')
    command = (mune, 'profile', stdlib / 'profiles.lua', policy)
    result = run(*command)
    assert result.returncode == 0 and result.stdout == '2 1\n', result

    for script in (
        'return on1xfile.profile { optimize = -1, debug = true }\n',
        'return { optimize = 2, debug = true, extra = true }\n',
        'return os.getenv("PATH")\n',
        'while true do end\n',
    ):
        policy.write_text(script)
        result = run(*command)
        assert result.returncode != 0 and result.stdout == '', result

    if qmr:
        result = run(qmr[0], 'fixture.On1xfile.i',
                     input='role = {{ data.role }}\n')
        assert result.returncode == 0, result.stderr
        policy.write_text(result.stdout)
        value = 'sources"\\\n# not a second field'
        result = run(mune, 'render', stdlib / 'render.lua', policy, 'role', value)
        assert result.returncode == 0, result.stderr
        assert json.loads(result.stdout.removeprefix('role = ')) == value, result
        result = run(mune, 'render', stdlib / 'render.lua', policy)
        assert result.returncode != 0 and result.stdout == '', result
        result = run(qmr[0], 'broken.On1xfile.i', input='{{ data.role')
        assert result.returncode != 0 and result.stdout == '', result
