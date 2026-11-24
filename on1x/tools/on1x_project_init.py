"""Create plain On1xfile manifests without evaluating project policy."""
from contextlib import contextmanager, nullcontext
from functools import partial
import json
import os
from pathlib import Path
import re
import stat


MAX_DISCOVERY = 32768
MAX_DEPTH = 32
TEMPLATE_NAME = re.compile(r'[A-Za-z_][A-Za-z0-9_-]{0,63}\Z')


def add_command(commands, *, root_manifest, local_roles, local, max_manifest, qmr_tag):
    parser = commands.add_parser('init', help='create an On1xfile without running project policy')
    parser.add_argument('directory', type=Path, nargs='?', default=Path('.'))
    parser.add_argument('--recursive', action='store_true', help='also generate directory-local manifests')
    parser.add_argument('--template', metavar='NAME', help='copy and validate an explicit NAME.On1xfile template')
    parser.add_argument('--templates-path', metavar='PATH_LIST',
                        help='search directories instead of $ON1XFILE_TEMPLATES_PATH')
    parser.set_defaults(init_project=partial(initialize, root_manifest=root_manifest, local_roles=local_roles,
                                             local=local, max_manifest=max_manifest, qmr_tag=qmr_tag))


def open_directory(path, parent=None):
    try:
        return os.open(path, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=parent)
    except NotADirectoryError as error:
        raise ValueError(f'{path}: expected a real directory (not a symlink)') from error


@contextmanager
def directory(path, parent=None):
    """Open each component without following symlinks."""
    if parent is None:
        fd = open_directory(path.anchor)
        parts = path.parts[1:]
    else:
        fd = os.dup(parent)
        parts = path.parts
    try:
        for part in parts:
            child = open_directory(part, fd)
            os.close(fd)
            fd = child
        yield fd
    finally:
        os.close(fd)


def entry_info(parent, name):
    try:
        return os.stat(name, dir_fd=parent, follow_symlinks=False)
    except FileNotFoundError:
        return None


def template(args, max_manifest):
    if args.template is None:
        if args.templates_path is not None:
            raise ValueError('--templates-path requires --template NAME')
        # Name normalization matches the existing on1xc scaffolder.
        name = re.sub(r'[^A-Za-z0-9_-]+', '-', args.directory.name).strip('-') or 'project'
        if not re.match(r'[A-Za-z_]', name):
            name = '_' + name
        return f'on1xfile 1\nkind library\nname "{name[:64]}"\ndirectory "."\n'.encode()
    if not TEMPLATE_NAME.fullmatch(args.template):
        raise ValueError(f'invalid template name: {args.template!r}')
    search = args.templates_path
    if search is None:
        search = os.environ.get('ON1XFILE_TEMPLATES_PATH', '')
    paths = search.split(os.pathsep)
    if not all(paths):
        raise ValueError('--template requires a nonempty template path list with no empty entries')
    candidate = args.template + '.On1xfile'
    for path in paths:
        try:
            with directory(Path(os.path.abspath(path))) as parent:
                fd = os.open(candidate, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=parent)
                with os.fdopen(fd, 'rb') as stream:
                    if not stat.S_ISREG(os.fstat(stream.fileno()).st_mode):
                        raise ValueError(f'{Path(path) / candidate}: template must be a regular file')
                    data = stream.read(max_manifest + 1)
                    if len(data) > max_manifest:
                        raise ValueError(f'{Path(path) / candidate}: template exceeds manifest byte budget')
                    return data
        except FileNotFoundError:
            continue
    raise ValueError(f'template not found: {candidate}')


def discover(root, start, config, local):
    targets = set(config['targets'].values())
    stores = {dep.source for dep in config['deps'].values() if dep.kind != 'git-remote'}
    found = set()
    inspected = 0

    def walk(fd, here):
        nonlocal inspected
        if len(here.parts) > MAX_DEPTH:
            raise ValueError('recursive initialization exceeds directory depth budget')
        entries = []
        with os.scandir(fd) as scan:
            for item in scan:
                inspected += 1
                if inspected > MAX_DISCOVERY:
                    raise ValueError('recursive initialization exceeds directory discovery budget')
                if not item.name.startswith('.'):
                    entries.append((item.name, item.is_dir(follow_symlinks=False),
                                    item.is_file(follow_symlinks=False)))
        if here.parts and any(name == 'On1xfile' for name, _, _ in entries):
            return
        sources, interfaces, children = set(), set(), []
        for name, is_dir, is_file in entries:
            if is_dir:
                child = here / name
                if name not in ('build', 'target') and not any(child.is_relative_to(p) for p in stores):
                    children.append(name)
            elif is_file:
                suffix = Path(name).suffix
                if suffix in ('.on1x', '.on1xi'):
                    local(name, single=True)
                    (sources if suffix == '.on1x' else interfaces).add(name)
        if sources:
            rows = []
            for name in sorted(sources):
                path = here / name
                role = 'source' if path in targets else 'library'
                if role == 'source':
                    found.add(path)
                rows.append(f'{role} {json.dumps(name)}\n')
            yield here / 'On1xfile.m', ('on1xfile 1\n' + ''.join(rows)).encode()
        if interfaces:
            rows = []
            for name in sorted(interfaces):
                impl = Path(name).with_suffix('.on1x').name
                if impl not in sources:
                    raise ValueError(f'{here / name}: interface requires same-directory implementation {impl!r}')
                rows.append(f'interface {json.dumps(name)}\n')
            yield here / 'On1xfile.i', ('on1xfile 1\n' + ''.join(rows)).encode()
        for child in sorted(children):
            with directory(Path(child), fd) as subdir:
                yield from walk(subdir, here / child)

    if any(start.is_relative_to(p) for p in stores):
        raise ValueError(f'{start}: project directory cannot be inside a dependency store')
    if root is not None:
        with directory(start, root) as start_fd:
            yield from walk(start_fd, start)
    elif start != Path('.'):
        raise ValueError(f'{start}: declared project directory is missing')
    if missing := targets - found:
        raise ValueError(f'template target was not discovered: {min(missing)}')


def available(root, plan):
    for path, _ in plan:
        with directory(path.parent, root) as fd:
            if entry_info(fd, path.name) is not None:
                raise ValueError(f'project file already exists: {path}')


def publish(root, plan):
    created = []
    try:
        for path, data in plan:
            with directory(path.parent, root) as parent:
                fd = os.open(path.name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                             0o644, dir_fd=parent)
                with os.fdopen(fd, 'wb') as stream:
                    info = os.fstat(stream.fileno())
                    created.append((path, info.st_dev, info.st_ino))
                    stream.write(data)
    except (OSError, ValueError):
        for path, device, inode in reversed(created):
            try:
                with directory(path.parent, root) as parent:
                    info = entry_info(parent, path.name)
                    if info is not None and (info.st_dev, info.st_ino) == (device, inode):
                        os.unlink(path.name, dir_fd=parent)
            except (OSError, ValueError):
                pass
        raise


def initialize(args, *, root_manifest, local_roles, local, max_manifest, qmr_tag):
    args.directory = Path(os.path.abspath(args.directory))
    data = template(args, max_manifest)
    if qmr_tag.search(data):
        raise ValueError('project templates must be plain On1xfile text; QMR is not evaluated')
    config = root_manifest(data, args.directory / 'On1xfile')
    plan = [(Path('On1xfile'), data)]
    name = args.directory.name or '.'
    with directory(args.directory.parent) as parent:
        present = entry_info(parent, name) is not None
        with (directory(Path(name), parent) if present else nullcontext()) as root:
            if root is not None:
                available(root, plan)
            if args.recursive:
                start = config.get('directory', Path('.'))
                for path, content in discover(root, start, config, local):
                    local(path.as_posix())
                    local_roles(content, path, path, config['kind'])
                    plan.append((path, content))
            if root is not None:
                available(root, plan)
                publish(root, plan)
        if not present:
            os.mkdir(name, dir_fd=parent)
            try:
                with directory(Path(name), parent) as root:
                    publish(root, plan)
            except (OSError, ValueError):
                try:
                    os.rmdir(name, dir_fd=parent)
                except OSError:
                    pass
                raise
