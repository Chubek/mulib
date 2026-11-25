"""Discover ordered MuLIB headers and fingerprint their literal dependency closure."""

from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import re
import stat

from generate_portable_recipe import KERNEL


INPUT_LIMIT = 8 * 1024 * 1024
IMPL_MACRO = re.compile(rb"\bMULIB_IMPL_[A-Z0-9_]+\b")
CONDITIONAL = re.compile(rb"^[ \t\v\f]*(?:#|%:)[ \t\v\f]*(?:if|ifdef|ifndef|elif)\b([^\\\r\n]*)", re.MULTILINE)
DIRECTIVE = re.compile(rb"^[ \t\v\f]*(?:#|%:)[ \t\v\f]*([A-Za-z_0-9]+)(.*)", re.MULTILINE)
INCLUDE = re.compile(rb'\s*(?:"([^"\r\n]+)"|<([^>\r\n]+)>)\s*$')
COMMENTS = re.compile(rb'"(?:[^"\\\n]|\\.)*"|\'(?:[^\'\\\n]|\\.)*\'|/\*[\s\S]*?\*/|//[^\r\n]*')
VOLATILE = re.compile(rb"\b(?:__has_include(?:_next)?|__DATE__|__TIME__|__TIMESTAMP__)\b")
TRIGRAPHS = dict(zip(b"=/'()!<>-", b"#\\^[]|{}~"))


def digest(contents):
    return hashlib.sha256(contents).hexdigest()


def json_bytes(document):
    return json.dumps(document, ensure_ascii=True, indent=2).encode("ascii") + b"\n"


def roots_for(value, default):
    selected = [Path(part) for part in (value or "").split(":") if part]
    roots = []
    for path in selected or [default]:
        path = path.resolve(strict=True)
        if not path.is_dir():
            raise ValueError(f"MuLIB kernel root is not a directory: {path}")
        if path not in roots:
            roots.append(path)
    return roots


@dataclass(frozen=True)
class Header:
    relative: str
    path: Path
    aliases: tuple
    error: str = ""


def file_kind(path):
    try:
        return path.stat().st_mode
    except (FileNotFoundError, NotADirectoryError):
        return 0


def resolve_header(path):
    try:
        return path.resolve(strict=True), ""
    except (FileNotFoundError, RuntimeError) as error:
        return path, f"cannot resolve header: {error}"


def discover(roots):
    """Shadow identical public kernel IDs or nonkernel paths, in root order."""
    matches = {}

    def fail(error):
        raise error

    for root in roots:
        for current, dirs, files in os.walk(root, onerror=fail):
            # Do not recurse through directory aliases: the user can explicitly
            # put such a directory (canonicalized once above) on their path.
            dirs.sort()
            for name in sorted((*files, *(name for name in dirs if name.endswith(".h")))):
                if name.endswith(".h"):
                    path = Path(current) / name
                    relative = path.relative_to(root).as_posix()
                    kernel = name.removeprefix("mulib-").removesuffix(".h")
                    key = name if name.startswith("mulib-") and KERNEL.fullmatch(kernel) else relative
                    matches.setdefault(key, []).append((relative, path))
    for records in matches.values():
        (relative, selected), *rest = records
        aliases = tuple(resolve_header(path)[0] for _, path in rest)
        canonical, error = resolve_header(selected)
        if not error and not stat.S_ISREG(file_kind(canonical)):
            error = "header is not a regular file"
        yield Header(relative, canonical, aliases, error)


def source_path(path):
    """Cephyr's quoted header-name grammar is not that of a C string literal."""
    value = str(path)
    value.encode("utf-8")
    if any(ord(char) < 32 or ord(char) == 127 or char in '"\\' for char in value) or "??" in value:
        raise ValueError(f"cannot represent header path in a C include: {value!r}")
    return value


def directive_source(data):
    data = re.sub(rb"\?\?([=/'()!<>-])", lambda m: bytes([TRIGRAPHS[m[1][0]]]), data)
    data = re.sub(rb"\\(?:\r\n|\n|\r)", b"", data)
    data = data.replace(b"\r\n", b"\n").replace(b"\r", b"\n")
    return COMMENTS.sub(lambda m: b" " + b"\n" * m[0].count(b"\n") if m[0].startswith(b"/") else m[0],
                        data)


def implementation_macro(relative, data):
    observed = {m.decode("ascii") for line in CONDITIONAL.findall(directive_source(data))
                for m in IMPL_MACRO.findall(line)}
    stem = Path(relative).stem.removeprefix("mulib-")
    conventional = "MULIB_IMPL_" + stem.upper().replace("-", "_").replace("+", "P")
    if conventional in observed:
        return conventional, ()
    if len(observed) == 1:
        return next(iter(observed)), ()
    return None, tuple(sorted(observed))


class Inputs:
    def __init__(self, roots, include_dirs=()):
        self.roots = (*roots, *include_dirs)
        self.loaded = {}
        self.parsed = {}
        self.candidates = {}

    def read(self, path):
        if path not in self.loaded:
            with path.open("rb") as stream:
                payload = stream.read(INPUT_LIMIT + 1)
            self.loaded[path] = payload, digest(payload)
        return self.loaded[path]

    def scan(self, path):
        if path not in self.parsed:
            data, sha = self.read(path)
            text = directive_source(data)
            cacheable = len(data) <= INPUT_LIMIT and not VOLATILE.search(text)
            origin = cacheable and b"\0" not in data
            imports = []
            for directive, argument in DIRECTIVE.findall(text):
                if directive == b"line" or directive[:1].isdigit():
                    origin = False
                if directive in (b"include", b"include_next", b"import", b"embed"):
                    match = INCLUDE.fullmatch(argument) if directive == b"include" else None
                    if match is None:
                        cacheable = False
                        # Macro-generated include names cannot justify a
                        # header-exclusive definition or a sound delta.
                        origin = False
                    else:
                        try:
                            imports.append((bool(match[1]), (match[1] or match[2]).decode("utf-8")))
                        except UnicodeError:
                            cacheable = origin = False
            self.parsed[path] = (sha, tuple(imports), cacheable, origin)
        return self.parsed[path]

    def find(self, candidate):
        if candidate not in self.candidates:
            try:
                resolved = candidate.resolve(strict=True)
                self.candidates[candidate] = resolved if stat.S_ISREG(file_kind(resolved)) else None
            except (FileNotFoundError, NotADirectoryError, RuntimeError):
                self.candidates[candidate] = None
        return self.candidates[candidate]

    def fingerprint(self, header):
        todo = [(header, header)]
        visited, files = set(), {}
        cacheable = origin = True
        while todo:
            spelling, path = todo.pop()
            key = (spelling.parent, path)
            if key in visited:
                continue
            visited.add(key)
            if len(visited) > 1024:
                return None, False, frozenset()
            sha, imports, delta, trustworthy = self.scan(path)
            cacheable &= delta
            origin &= trustworthy
            files[(str(spelling.parent), str(path))] = sha
            for quoted, name in imports:
                # Probe all potential resolutions, not only today's winner.
                # A newly added left-hand candidate invalidates the old key.
                # Quote and angle semantics can differ across input resources.
                locations = [spelling.parent, path.parent, *self.roots] if quoted else self.roots
                if Path(name).is_absolute():
                    locations = [Path("/")]
                for parent in locations:
                    candidate = Path(os.path.abspath(parent / name))
                    resolved = self.find(candidate)
                    if resolved is not None:
                        todo.append((candidate, resolved))
        provenance = frozenset(path for _, path in visited)
        return (digest(json_bytes(sorted((*name, sha) for name, sha in files.items()))) if cacheable else None,
                origin, provenance)
