"""Exercise default, recursive, and optional project scaffolding."""
import argparse
from hashlib import sha256
import os
from pathlib import Path
import subprocess
import tempfile


def exercise(compiler, work):
    work.mkdir()
    env = dict(os.environ, LC_ALL='C', NO_COLOR='1', PYTHONDONTWRITEBYTECODE='1')
    env.pop('ON1XFILE_TEMPLATES_PATH', None)

    def aux(*args, cwd=work, error=None, environment=env):
        result = subprocess.run([compiler, 'aux', *args], cwd=cwd, env=environment,
                                capture_output=True, text=True, timeout=60)
        if error is None:
            assert result.returncode == 0, result
        else:
            assert result.returncode == 1 and error in result.stderr, result
        return result.stdout

    help_text = aux('init', '--help')
    assert '--recursive' in help_text and '--with-gitignore' in help_text

    def write(relative, data):
        path = work / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(data, encoding='utf-8')
        return path

    def mkdir(relative):
        path = work / relative
        path.mkdir()
        return path

    write('plain/sub/orphan.on1xi', 'not parsed')
    aux('init', cwd=work / 'plain', environment=dict(env, ON1XFILE_TEMPLATES_PATH='::'))
    root = work / 'plain/On1xfile'
    expected = 'on1xfile 1\nkind library\nname "plain"\ndirectory "."\n'
    assert root.read_text() == expected
    assert not list((work / 'plain').rglob('On1xfile.?'))
    assert not (work / 'plain/.gitignore').exists()
    aux('init', 'plain', '--recursive', error='project file already exists')
    assert root.read_text() == expected
    aux('init', 'new project')
    assert 'name "new-project"' in (work / 'new project/On1xfile').read_text()
    aux('init', 'no-template', '--templates-path', '.', error='requires --template')
    assert not (work / 'no-template').exists()

    def check_gitignore(project):
        # Pin the supplied UTF-8 template, including all comments and the final LF.
        data = (project / '.gitignore').read_bytes()
        assert sha256(data).hexdigest() == '630418bdd0fccfbd427b2e3fb1a65edf30dd105db0d239b6fdcc3df6c69552f5'

    aux('init', 'new ignored project', '--with-gitignore')
    assert 'name "new-ignored-project"' in (work / 'new ignored project/On1xfile').read_text()
    check_gitignore(work / 'new ignored project')

    impl = 'fn value()->i64{return 42;}\n'
    interface = 'on1xi 1;\nfn value()->i64;\n'
    write('mixed/top.on1x', impl)
    write('mixed/sub/z space.on1x', impl)
    write('mixed/sub/a.on1x', impl)
    write('mixed/sub/a.on1xi', interface)
    write('mixed/sub/.hidden.on1x', impl)
    write('mixed/sub/policy.lua', 'os.execute("touch policy-ran")')
    write('mixed/.hidden/deep.on1x', impl)
    write('mixed/build/generated.on1x', impl)
    write('mixed/target/generated.on1x', impl)
    write('mixed/independent/On1xfile', 'preserve this nested project')
    write('mixed/independent/module.on1x', impl)
    external = write('external/foreign.on1x', impl)
    (work / 'mixed/redirection').symlink_to(external.parent, target_is_directory=True)
    (work / 'mixed/sub/linked.on1x').symlink_to(external)
    aux('init', '--recursive', 'mixed')
    assert (work / 'mixed/On1xfile.m').read_text() == 'on1xfile 1\nlibrary "top.on1x"\n'
    assert (work / 'mixed/sub/On1xfile.m').read_text() == (
        'on1xfile 1\nlibrary "a.on1x"\nlibrary "z space.on1x"\n')
    assert (work / 'mixed/sub/On1xfile.i').read_text() == 'on1xfile 1\ninterface "a.on1xi"\n'
    for name in ('On1xfile', 'On1xfile.m', 'sub/On1xfile.m', 'sub/On1xfile.i'):
        aux('format', work / 'mixed' / name, '--check')
    for directory in ('mixed/.hidden', 'mixed/build', 'mixed/target', 'mixed/independent', 'external'):
        assert not (work / directory / 'On1xfile.m').exists(), directory
    assert not (work / 'policy-ran').exists()

    write('gitignore-collision/sub/mod.on1x', impl)
    existing_ignore = work / 'gitignore-collision/.gitignore'
    existing_bytes = b'# keep this exactly\r\n/local/\r\n'
    existing_ignore.write_bytes(existing_bytes)
    aux('init', '--recursive', '--with-gitignore', cwd=existing_ignore.parent,
        error='project file already exists: .gitignore')
    assert existing_ignore.read_bytes() == existing_bytes
    assert not list(existing_ignore.parent.rglob('On1xfile*'))
    aux('init', cwd=existing_ignore.parent)
    assert (existing_ignore.parent / 'On1xfile').is_file()
    assert existing_ignore.read_bytes() == existing_bytes

    linked_ignore = mkdir('linked-ignore') / '.gitignore'
    missing_target = work / 'missing-ignore'
    linked_ignore.symlink_to(missing_target)
    aux('init', 'linked-ignore', '--with-gitignore', error='project file already exists: .gitignore')
    assert linked_ignore.readlink() == missing_target and not missing_target.exists()
    assert not (linked_ignore.parent / 'On1xfile').exists()

    write('buildable/src/module.on1x', impl)
    write('buildable/src/module.on1xi', interface)
    aux('init', '--recursive', 'buildable')
    built = Path(aux('build', 'buildable', '--on1xc', compiler).strip())
    assert (built / 'library/src/module.on1xi').read_text() == interface

    write('orphan/mod.on1xi', interface)
    aux('init', '--recursive', 'orphan', error='requires same-directory implementation')
    assert not (work / 'orphan/On1xfile').exists()
    write('collision/a/ok.on1x', impl)
    write('collision/z/existing.on1x', impl)
    kept = write('collision/z/On1xfile.m', 'preserve this file')
    aux('init', '--recursive', '--with-gitignore', 'collision', error='project file already exists')
    assert kept.read_text() == 'preserve this file'
    assert not (work / 'collision/a/On1xfile.m').exists() and not (work / 'collision/On1xfile').exists()
    assert not (work / 'collision/.gitignore').exists()
    linked_output = mkdir('linked-output')
    (linked_output / 'On1xfile').symlink_to(external)
    aux('init', linked_output, error='project file already exists')
    assert external.read_text() == impl
    (work / 'redirected-root').symlink_to(external.parent, target_is_directory=True)
    aux('init', 'redirected-root/escape', error='real directory')
    assert not (external.parent / 'escape').exists()

    executable = ('on1xfile 1\nkind executable\nname "from-env"\ndirectory "app"\n'
                  'target "run" "app/main.on1x"\ndefine marker shell "touch template-ran"\n'
                  'dependency "dep" local "app/vendors/dep"\n')
    preferred = 'on1xfile 1\nkind library\nname "from-flag"\ndirectory "."\n'
    write('env templates/demo.On1xfile', executable)
    write('flag templates/demo.On1xfile', preferred)
    write('app project/app/main.on1x', 'fn main()->i64{return 0;}\n')
    write('app project/app/util.on1x', impl)
    write('app project/app/vendors/dep/lib.on1x', impl)
    write('app project/no-scan/other.on1x', impl)
    mkdir('empty templates')
    templates = dict(env, ON1XFILE_TEMPLATES_PATH=os.pathsep.join(('empty templates', 'env templates')))
    aux('init', 'app project', '--recursive', '--with-gitignore', '--template', 'demo', environment=templates)
    assert (work / 'app project/On1xfile').read_text() == executable
    check_gitignore(work / 'app project')
    assert not (work / 'app project/app/.gitignore').exists()
    assert (work / 'app project/app/On1xfile.m').read_text() == (
        'on1xfile 1\nsource "main.on1x"\nlibrary "util.on1x"\n')
    assert not (work / 'app project/no-scan/On1xfile.m').exists()
    assert not (work / 'app project/app/vendors/dep/On1xfile.m').exists()
    assert not (work / 'template-ran').exists()
    paths = os.pathsep.join(('flag templates', 'env templates'))
    aux('init', 'flag project', '--template', 'demo', '--templates-path', paths, environment=templates)
    assert (work / 'flag project/On1xfile').read_text() == preferred
    aux('init', 'missing-target', '--recursive', '--template', 'demo', environment=templates,
        error='declared project directory is missing')
    assert not (work / 'missing-target').exists()
    write('bad-target/app/util.on1x', impl)
    aux('init', 'bad-target', '--recursive', '--template', 'demo', environment=templates,
        error='template target was not discovered')
    assert not (work / 'bad-target/On1xfile').exists()
    write('env templates/qmr.On1xfile', '{{ touch template-ran }}')
    aux('init', 'qmr', '--template', 'qmr', environment=templates, error='QMR is not evaluated')
    aux('init', 'escape', '--template', '../demo', environment=templates, error='invalid template name')
    aux('init', 'unset', '--template', 'demo', error='requires a nonempty template path')
    aux('init', 'absent', '--template', 'not_present', environment=templates, error='template not found')
    (work / 'empty templates/demo.On1xfile').symlink_to(work / 'env templates/demo.On1xfile')
    aux('init', 'linked-template', '--template', 'demo', environment=templates, error='demo.On1xfile')
    assert not (work / 'linked-template').exists()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--on1xc', type=lambda path: Path(path).resolve(), required=True)
    parser.add_argument('--installed-on1xc', type=lambda path: Path(path).resolve())
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='on1x aux init ') as temporary:
        work = Path(temporary)
        exercise(args.on1xc, work / 'source')
        if args.installed_on1xc:
            exercise(args.installed_on1xc, work / 'installed')
    print('on1xc aux init: PASS')


if __name__ == '__main__':
    main()
