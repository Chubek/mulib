"""Project subcommands for on1xc and on1x-build.py."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import stat
import sys


TOOLS = Path(__file__).resolve().parent


def _mode(path):
    try:
        return path.lstat().st_mode
    except FileNotFoundError:
        return None


def _create_project(root, files):
    """Preflight all fixed paths and never overwrite existing content."""
    directories = {root, *(parent for path in files for parent in path.parents
                           if parent.is_relative_to(root))}
    directories = sorted(directories, key=lambda p: (len(p.parts), str(p)))
    for parent in directories:
        mode = _mode(parent)
        if mode is not None and not stat.S_ISDIR(mode):
            raise ValueError(f'project directory is not a real directory: {parent}')
    for path in files:
        if _mode(path) is not None:
            raise ValueError(f'project file already exists: {path}')
    created_dirs, created_files = [], []
    try:
        for parent in directories:
            if _mode(parent) is None:
                parent.mkdir()
                created_dirs.append(parent)
        for path, content in files.items():
            with path.open('x', encoding='utf-8', newline='\n') as stream:
                created_files.append(path)
                stream.write(content)
    except OSError:
        for path in reversed(created_files):
            path.unlink(missing_ok=True)
        for parent in reversed(created_dirs):
            try:
                parent.rmdir()
            except OSError:
                pass
        raise


def _project_name(root):
    # The directory itself may contain spaces or non-ASCII. Choose a portable
    # package/artifact name without putting the raw path into manifest syntax.
    name = re.sub(r'[^A-Za-z0-9_-]+', '-', root.name).strip('-')
    if not name:
        return 'project'
    if not re.match(r'[A-Za-z_]', name):
        name = '_' + name
    return name[:64]


def _exec(command, arguments):
    # Keep the engine contract here rather than in either driver entry point.
    if not re.fullmatch(r'[A-Za-z_][A-Za-z0-9_-]{0,63}', command):
        raise ValueError(f'invalid On1xfile command: {command!r}')
    engine = TOOLS / 'on1x-project.py'
    if not engine.is_file():
        raise ValueError(f'On1xfile engine is unavailable: {engine}')
    compiler = os.environ.get('ON1X_PROJECT_COMPILER')
    options = ['--on1xc', compiler] if compiler else []
    vm_commands = ('run', 'vm-rerun', 'vm-cache-clear')
    action = command if command in vm_commands else 'build'
    if command == 'release':
        options.append('--release')
    elif command not in ('build', *vm_commands):
        options.extend(('--project-command', command))
    completed = subprocess.run([sys.executable, engine, action, *options, *arguments])
    return 128 - completed.returncode if completed.returncode < 0 else completed.returncode


def _scaffold(kind, name):
    manifest = f'on1xfile 1\nkind {kind}\nname "{name}"\ndirectory "src"\n'
    library = 'fn greeting() -> string { return "Hello from On1x!"; }\n'
    if kind == 'library':
        return {'On1xfile': manifest, 'src/On1xfile.m': 'on1xfile 1\nlibrary "__init__.on1x"\n',
                'src/__init__.on1x': library}
    manifest += f'target "{name}" "src/main.on1x"\n'
    declarations = 'on1xfile 1\nsource "main.on1x"\n'
    program = 'fn main() { println("Hello from On1x!"); }\n'
    files = {}
    if kind == 'complex':
        second = 'demo' if name != 'demo' else 'example'
        manifest += f'target "{second}" "src/demo.on1x"\n'
        declarations += 'source "demo.on1x"\nlibrary "greeting.on1x"\n'
        program = 'import "greeting.on1x";\n\nfn main() { println(greeting()); }\n'
        files['src/greeting.on1x'] = library
        files['src/demo.on1x'] = 'fn main() { println("On1x demo"); }\n'
    files.update({'On1xfile': manifest, 'src/On1xfile.m': declarations, 'src/main.on1x': program})
    return files


def main(argv):
    driver = 'on1xc' if os.environ.get('ON1X_PROJECT_COMPILER') else Path(sys.argv[0]).name
    parser = argparse.ArgumentParser(prog=f'{driver} {argv[0]}')
    if argv[0] == 'init':
        kind = parser.add_mutually_exclusive_group(required=True)
        kind.add_argument('--bin', dest='kind', action='store_const', const='executable')
        kind.add_argument('--proj', dest='kind', action='store_const', const='complex')
        kind.add_argument('--lib', dest='kind', action='store_const', const='library')
        parser.add_argument('directory', nargs='?', type=Path, default=Path('.'))
        args = parser.parse_args(argv[1:])
        root = Path(os.path.abspath(args.directory))
        _create_project(root, {root / path: text for path, text in _scaffold(args.kind, _project_name(root)).items()})
        return 0
    parser.add_argument('command', help='build, release, run, vm-rerun, vm-cache-clear, or a root command/profile')
    parser.add_argument('arguments', nargs=argparse.REMAINDER)
    args = parser.parse_args(argv[1:])
    return _exec(args.command, args.arguments)
