"""Public project driver, safe scaffolding, and existing single-file compilation."""
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
from support import NativeTests, run


t = NativeTests()
helper = [sys.executable, t.root / 'tools/on1x-build.py']


def ok(command, *args, cwd, env=None):
    result = run([*command, *args], cwd=cwd, env=env)
    assert result.returncode == 0, (command, args, result)
    return result.stdout


def reject(command, *args, cwd, message=None):
    result = run([*command, *args], cwd=cwd)
    assert result.returncode != 0, (command, args, result)
    if message is not None:
        assert message in result.stderr, result


with tempfile.TemporaryDirectory(dir=t.work, prefix='projects-') as tmp:
    work = Path(tmp)
    driver = [t.compiler] if os.name == 'posix' else helper
    assert 'init' in ok(driver, '--help', cwd=work)
    assert '--proj' in ok(driver, 'init', '--help', cwd=work)
    reject(driver, 'init', cwd=work)
    reject(driver, 'init', '--bin', '--lib', cwd=work)
    reject(driver, 'exec', cwd=work)
    assert list(work.iterdir()) == []

    # No generated file may replace an existing file or follow a symlink.
    target = work / 'a new bin'
    target.mkdir()
    (target / 'keep.txt').write_text('keep me')
    ok(driver, 'init', '--bin', target, cwd=work)
    manifest = target / 'On1xfile'
    manifest_text = manifest.read_text()
    assert not manifest_text.lstrip().startswith('(')
    reject(driver, 'init', '--bin', target, cwd=work)
    assert manifest.read_text() == manifest_text
    assert (target / 'keep.txt').read_text() == 'keep me'

    collision = work / 'source collision'
    collision.mkdir()
    preserved = next(target.rglob('*.on1x')).relative_to(target)
    (collision / preserved).parent.mkdir(parents=True, exist_ok=True)
    (collision / preserved).write_text('existing project source')
    reject(driver, 'init', '--bin', collision, cwd=work)
    assert not (collision / 'On1xfile').exists()
    assert (collision / preserved).read_text() == 'existing project source'

    if os.name == 'posix':
        symlink = work / 'link target'
        symlink.symlink_to(target, target_is_directory=True)
        reject(driver, 'init', '--lib', symlink, cwd=work)
        assert manifest.read_text() == manifest_text
        unrelated = work / 'unrelated'
        unrelated.mkdir()
        project = work / 'linked contents'
        project.mkdir()
        (project / 'src').symlink_to(unrelated, target_is_directory=True)
        reject(driver, 'init', '--bin', project, cwd=work)
        assert not (project / 'On1xfile').exists() and list(unrelated.iterdir()) == []

    projects = {'--bin': target}
    for kind in ('--proj', '--lib'):
        directory = work / (kind[2:] + ' project')
        directory.mkdir()
        ok(helper, 'init', kind, cwd=directory)
        assert (directory / 'On1xfile').is_file()
        projects[kind] = directory
    for kind, directory in projects.items():
        source_files = sorted(directory.rglob('*.on1x'))
        assert source_files, kind
        assert list(directory.rglob('On1xfile.m')), kind
        if kind != '--lib':
            ok([t.compiler], '--check', directory / 'src/main.on1x', cwd=directory)
        else:
            client = directory / 'client.on1x'
            client.write_text('import "src/__init__.on1x"; fn main() { println(greeting()); }\n')
            ok([t.compiler], '--check', client, cwd=directory)
        for local in (*directory.rglob('On1xfile.m'), *directory.rglob('On1xfile.i')):
            assert not local.read_text().lstrip().startswith('('), local

    source = target / 'standalone.on1x'
    source.write_text('fn main() { println(42); }\n')
    binary = work / 'single file'
    ok(helper, source, '--on1xc', t.compiler, '--cc', t.cc, '-o', binary,
       *['--cflag=' + flag for flag in t.flags], cwd=work)
    assert ok([binary], cwd=work) == '42\n'

    # The driver is transport, not a second evaluator. Assert exact argument,
    # cwd, and status propagation using a test engine installed beside it.
    fixture = work / 'fixture driver'
    tools = fixture / 'tools'
    tools.mkdir(parents=True)
    for name in ('on1x-build.py', 'on1x_project.py'):
        shutil.copy2(t.root / 'tools' / name, tools)
    engine = tools / 'on1x-project.py'
    engine.write_text(
        'import json, os, sys\n'
        'print(json.dumps([os.getcwd(), *sys.argv[1:]]))\n'
        'raise SystemExit(41)\n'
    )
    forwarded = ['exec', 'release', '--', 'a b', '$(echo not-a-shell)']
    result = run([sys.executable, tools / 'on1x-build.py', *forwarded], cwd=target)
    assert result.returncode == 41, result
    assert json.loads(result.stdout) == [str(target), 'build', '--release', *forwarded[3:]], result

    if sys.platform.startswith('linux'):
        engine_flags = ['--on1xc', t.compiler, '--cc', t.cc]
        for subcommand in ('build', 'release'):
            executable_root = projects['--bin']
            command = driver if subcommand == 'build' else helper
            output = Path(ok(command, 'exec', subcommand, executable_root, *engine_flags, cwd=work).strip())
            assert (output / '.on1x-build.json').is_file()
            assert ok([output / 'a-new-bin'], cwd=work) == 'Hello from On1x!\n'
        complex_root = projects['--proj']
        output = Path(ok(driver, 'exec', 'build', *engine_flags, cwd=complex_root).strip())
        assert ok([output / 'proj-project'], cwd=work) == 'Hello from On1x!\n'
        assert ok([output / 'demo'], cwd=work) == 'On1x demo\n'
        library_root = projects['--lib']
        output = Path(ok(driver, 'exec', 'build', *engine_flags, cwd=library_root).strip()) / 'library'
        assert (output / 'src/__init__.on1x').is_file() and (output / 'On1xfile').is_file()
        if (t.compiler.parent / 'on1x-mune-runner').is_file():
            (target / 'preview.lua').write_text('return on1xfile.profile { optimize = 1, debug = true }\n')
            manifest.write_text(manifest_text + 'profile "preview" "preview.lua"\n')
            output = Path(ok(driver, 'exec', 'preview', *engine_flags, cwd=target).strip())
            report = json.loads((output / '.on1x-build.json').read_text())
            assert report['profile'] == 'preview' and '-O1' in report['cflags'], report
        policy = manifest_text + 'define ignored shell "exit 9"\n'
        manifest.write_text(policy + ''.join(
            f'command "listing_{mode}" format {mode} "src/main.on1x"\n'
            for mode in ('plain', 'latex', 'troff')))
        (target / '.on1xfmt').write_text('tabstop = 4\n')
        for mode in ('plain', 'latex', 'troff'):
            actual = ok(driver, 'exec', 'listing_' + mode, '--on1xc', t.compiler, cwd=target)
            expected = ok([t.compiler], 'fmt', 'src/main.on1x', '--target', mode, cwd=target)
            assert actual == expected and actual
        manifest.write_text(policy + 'command "escape" format plain "../outside.on1x"\n')
        reject(driver, 'exec', 'escape', cwd=target, message='normalized local path')
        manifest.write_text(manifest_text)
        reject(driver, 'exec', 'not_declared', *engine_flags, cwd=target, message='unknown profile')
        reject(driver, 'exec', 'build', *engine_flags, cwd=work, message='On1xfile')

print('Project driver, scaffold protection and single-file compatibility: PASS')
