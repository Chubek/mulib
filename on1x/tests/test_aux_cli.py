"""Exercise the on1xc aux transport with real and controlled project drivers."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


ON1X = Path(__file__).resolve().parents[1]


def run(command, cwd, env):
    return subprocess.run(command, cwd=cwd, env=env, capture_output=True, text=True, timeout=60)


def exercise(compiler, engine, cc, project, env):
    aux = [compiler, 'aux']

    def invoke(*args, status=0):
        result = run([*aux, *args], project.parent, env)
        assert result.returncode == status, result
        return result

    def compare(*args, status=0):
        routed = invoke(*args, status=status)
        direct = run([engine, *args], project.parent, env)
        assert (routed.returncode, routed.stdout, routed.stderr) == (
            direct.returncode, direct.stdout, direct.stderr), (routed, direct)
        return routed.stdout

    assert 'aux COMMAND' in run([compiler, '--help'], project.parent, env).stdout
    help_text = compare('--help')
    assert compare('-h') == help_text
    choices = set(re.search(r'\{([\w-]+(?:,[\w-]+)+)\}', help_text)[1].split(','))
    assert {'plan', 'lock', 'fetch', 'backends', 'format', 'build'} <= choices, help_text
    for command in sorted(choices):
        compare(command, '--help')
    compare(status=2)
    compare('not-a-project-driver-command', status=2)
    compare('plan', '--not-a-project-driver-option', status=2)
    compare('format', '--check', '--', project / 'On1xfile')
    compare('fetch', 'not_declared', project, '--frozen', status=1)

    lock = Path(invoke('lock', project, '--offline').stdout.strip())
    assert lock == project / 'On1xfile.lock' and lock.is_file(), lock
    inventory = json.loads(invoke('backends', '--on1xc', compiler, '--cc', cc).stdout)
    assert any(row['name'] == 'c11' and row['available'] for row in inventory['backends']), inventory

    output = project.parent / (project.name + ' output')
    args = [project, '--on1xc', compiler, '--cc', cc, '--release', '--frozen',
            '--target', 'sample', '--output-dir', output]
    plan = json.loads(invoke('plan', *args).stdout)
    assert plan['profile'] == 'release' and not output.exists(), plan
    assert Path(invoke('build', *args).stdout.strip()) == output
    program = run([output / 'sample'], project.parent, env)
    assert (program.returncode, program.stdout) == (0, '42\n'), program


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--on1xc', type=lambda path: Path(path).resolve(), required=True)
    parser.add_argument('--installed-on1xc', type=lambda path: Path(path).resolve())
    parser.add_argument('--cc', default='cc')
    args = parser.parse_args()
    env = dict(os.environ, LC_ALL='C', NO_COLOR='1', PYTHONDONTWRITEBYTECODE='1')

    with tempfile.TemporaryDirectory(prefix='on1x aux ') as temporary:
        work = Path(temporary).resolve()
        drivers = [(args.on1xc, ON1X / 'tools/on1x-project.py', work / 'source project')]
        if args.installed_on1xc:
            installed_tools = args.installed_on1xc.parent.parent / 'share/on1x/tools'
            drivers.append((args.installed_on1xc, installed_tools / 'on1x-project.py', work / 'installed project'))
        # Transport resolves the tool independently of the current project.
        (work / 'On1xfile').write_text('this is not an On1xfile\n')
        for compiler, engine, project in drivers:
            (project / 'src').mkdir(parents=True)
            (project / 'On1xfile').write_text('on1xfile 1\nkind executable\nname "sample"\n'
                                             'directory "src"\ntarget "sample" "src/main.on1x"\n')
            (project / 'src/On1xfile.m').write_text('on1xfile 1\nsource "main.on1x"\n')
            (project / 'src/main.on1x').write_text('fn main()->i64{println(42);return 0;}\n')
            exercise(compiler, engine, args.cc, project, env)

        prefix = work / 'fixture prefix'
        compiler = prefix / 'bin/on1xc'
        compiler.parent.mkdir(parents=True)
        shutil.copy2(args.on1xc, compiler)
        alias = work / 'on1xc via symlink'
        alias.symlink_to(compiler)
        engine = prefix / 'share/on1x/tools/on1x-project.py'
        engine.parent.mkdir(parents=True)
        decoy = work / engine.name
        decoy.write_text('#!/usr/bin/env python3\nraise SystemExit(99)\n')
        decoy.chmod(0o755)
        env['PATH'] = str(work) + os.pathsep + os.environ.get('PATH', os.defpath)
        env['ON1X_PROJECT_COMPILER'] = str(decoy)

        engine.write_text('#!/usr/bin/env python3\nimport json, os, sys\n'
                          'print(json.dumps([os.getcwd(), os.environ["ON1X_PROJECT_COMPILER"], *sys.argv[1:]]))\n'
                          'print("fixture diagnostic", file=sys.stderr)\n'
                          'raise SystemExit(41)\n')
        engine.chmod(0o755)
        forwarded = ['future-vm-command', '--some-option=a b', '', '--', '-h', '$(touch aux-expanded)']
        result = run([alias, 'aux', *forwarded], work, env)
        assert (result.returncode, result.stderr) == (41, 'fixture diagnostic\n'), result
        assert json.loads(result.stdout) == [str(work), str(compiler), *forwarded], result
        assert not (work / 'aux-expanded').exists()

        # An executable with no shebang must not be retried through a shell.
        engine.write_text('touch aux-expanded\n')
        for present in (True, False):
            if not present:
                engine.unlink()
            result = run([compiler, 'aux', '--help'], work, env)
            assert result.returncode == 2 and not result.stdout, result
            assert 'cannot start project driver' in result.stderr, result
            assert 'share/on1x/tools/on1x-project.py' in result.stderr, result
            assert not (work / 'aux-expanded').exists()

    print('on1xc aux project driver transport: PASS')


if __name__ == '__main__':
    main()
