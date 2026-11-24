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

    extended = (mune, 'profile-context', stdlib / 'profiles.lua', policy)
    policy.write_text('''
assert(on1xfile.argument("n") == math.mininteger)
assert(on1xfile.argument("empty") == "")
assert(on1xfile.argument("label") == 'false\\n"\\\\é')
assert(on1xfile.argument("debug") == false and on1xfile.argument("absent") == nil)
assert(__on1x_arguments == nil and __on1x_context == nil and __on1x_targets == nil)
assert(on1xfile.context().backend == "c11")
return on1xfile.profile { optimize = 1, debug = false }
''', encoding='utf-8')
    result = run(*extended, 'integer', 'n', '-9223372036854775808', 'string', 'empty', '',
                 'string', 'label', 'false\n"\\é', 'boolean', 'debug', 'false',
                 'context', 'backend', 'c11')
    assert result.returncode == 0 and result.stdout == '1 0\n', result
    policy.write_text('return { optimize = 0, debug = false }\n')
    for records in [
        ('unknown', 'value', 'true'),
        ('integer', 'value', '9223372036854775808'),
        ('integer', 'value', '1more'),
        ('boolean', 'value', '0'),
        ('string', 'missing_value'),
    ]:
        result = run(*extended, *records)
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
