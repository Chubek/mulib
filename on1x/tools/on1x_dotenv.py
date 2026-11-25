"""Literal, project-confined dotenv loading for On1xfile.

Each UTF-8 line contains a comment or NAME=VALUE (with optional export).
Unquoted values have leading/trailing space and tabs trimmed, and a # begins
a comment at the start of the value or after whitespace. Quotes preserve
whitespace and #; within double quotes only backslash and quote can be
escaped. A quote must close on the same line.
"""
import errno
import os
from pathlib import Path
import re
import stat


MAX_DOTENV = 64 * 1024
_CONTROL = re.compile(r'[\x00-\x08\x0a-\x1f\x7f]')
_ASSIGNMENT = re.compile(r'(?:export[ \t]+)?([A-Za-z_][A-Za-z0-9_]*)[ \t]*=')
_COMMENT = re.compile(r'[ \t]#')


def local_path(value):
    """Validate a raw manifest path before Path can normalize it."""
    value = os.fspath(value)
    if (not isinstance(value, str) or len(value) > 1024 or '\\' in value or ':' in value
            or '\t' in value or _CONTROL.search(value)):
        raise ValueError('dotenv requires a normalized project-relative path')
    parts = value.split('/')
    if len(parts) > 32 or any(part in ('', '.', '..') for part in parts):
        raise ValueError('dotenv requires a normalized project-relative path')
    return Path(*parts)


def _read(project, relative, origin):
    # Open each component relative to the already opened directory. Checking
    # paths before opening would allow a concurrent symlink substitution.
    directory_flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW
    try:
        parent = os.open(project, directory_flags)
        try:
            for part in relative.parts[:-1]:
                child = os.open(part, directory_flags, dir_fd=parent)
                os.close(parent)
                parent = child
            fd = os.open(relative.name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK,
                         dir_fd=parent)
            try:
                info = os.fstat(fd)
                if not stat.S_ISREG(info.st_mode):
                    raise ValueError(f'{origin}: expected a regular file')
                if info.st_size <= MAX_DOTENV:
                    with os.fdopen(fd, 'rb', closefd=False) as stream:
                        payload = stream.read(MAX_DOTENV + 1)
                    if len(payload) <= MAX_DOTENV:
                        return payload
                raise ValueError(f'{origin}: file exceeds {MAX_DOTENV} bytes')
            finally:
                os.close(fd)
        finally:
            os.close(parent)
    except FileNotFoundError:
        raise ValueError(f'{origin}: declared file does not exist') from None
    except OSError as error:
        if error.errno in (errno.ELOOP, errno.ENOTDIR):
            raise ValueError(f'{origin}: expected real directories and file; symlinks are not allowed') from None
        raise ValueError(f'{origin}: cannot read declared file') from None


def _value(text, at):
    text = text.lstrip(' \t')
    if not text or text.startswith('#'):
        return ''
    quote = text[0]
    if quote not in ('"', "'"):
        comment = _COMMENT.search(text)
        return (text[:comment.start()] if comment else text).rstrip(' \t')
    value, i = [], 1
    while i < len(text):
        ch = text[i]
        if ch == quote:
            tail = text[i + 1:].lstrip(' \t')
            if tail and not tail.startswith('#'):
                raise ValueError(f'{at}: unexpected text after quoted value')
            return ''.join(value)
        if quote == '"' and ch == '\\' and i + 1 < len(text) and text[i + 1] in ('\\', '"'):
            i += 1
            ch = text[i]
        value.append(ch)
        i += 1
    raise ValueError(f'{at}: unterminated quoted value')


def _parse(payload, origin):
    invalid_line = None
    try:
        text = payload.decode('utf-8')
    except UnicodeDecodeError as error:
        invalid_line = 1 + payload.count(b'\n', 0, error.start)
    if invalid_line is not None:
        raise ValueError(f'{origin}:{invalid_line}: invalid UTF-8')
    values = {}
    for number, line in enumerate(text.split('\n'), 1):
        at = f'{origin}:{number}'
        line = line.removesuffix('\r').lstrip(' \t')
        if _CONTROL.search(line):
            raise ValueError(f'{at}: unexpected control character')
        if not line or line.startswith('#'):
            continue
        match = _ASSIGNMENT.match(line)
        if not match:
            raise ValueError(f'{at}: expected NAME=VALUE assignment')
        values[match[1]] = _value(line[match.end():], at)
    return values


def load(project, relative, ambient, *, preserve=False):
    """Return a new environment using an explicit base mapping.

    The declared UTF-8 file must be a regular file under the project, with
    no symlinks in the relative path. Last assignment wins within the file.
    The file wins over the base by default; preserve=True keeps every base
    key that is already present, including keys with empty values. Values
    are literal: no expansion, continuation or shell evaluation takes place.
    """
    relative = local_path(relative)
    origin = f'dotenv {ascii(relative.as_posix())}'
    values = _parse(_read(project, relative, origin), origin)
    return {**values, **ambient} if preserve else {**ambient, **values}
