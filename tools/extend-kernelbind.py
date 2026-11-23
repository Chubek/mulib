#!/usr/bin/env python3
"""Project MANIFEST.yaml into the On1x discovery-only kernel coverage catalog.

This is an inventory, not a declaration extractor, implementation recipe,
sidecar, or callable profile. It copies exact MANIFEST names (including legacy
spellings), never source aliases. Ambiguous source aliases and unregistered
headers are rejected. Reserved headers are not exported and cannot participate
in dependencies. Dependencies retain their MANIFEST names and order.
"""

import argparse
import importlib.util
import io
import json
import os
from pathlib import Path
import re
import stat
import sys
import tempfile


ROOT = Path(__file__).resolve().parent.parent
CATALOG = Path("on1x/contracts-exotract/kernel-auto.json")
SCHEMA = "on1x.kernel-auto/v1"
MAX_MANIFEST_BYTES = 8 * 1024 * 1024
MAX_CATALOG_BYTES = 2 * 1024 * 1024  # On1x's automatic catalog reader limit.
MAX_MODULES = 4096
NAME = r"[a-z0-9][a-z0-9_+-]*"  # Matches the shared MANIFEST reader.
ROOT_KEY = re.compile(r"([a-zA-Z_][a-zA-Z0-9_-]*): *(.*)")
FIELD = re.compile(r"  ([a-z][a-z0-9_]*):(?: +(.*))?")
PROJECTED = {"header", "status", "implementation", "macro", "dependencies", "extra_macros", "optional_dependency"}


def contract_tools():
    # Always use the trusted sibling script, never Python from --root.
    spec = importlib.util.spec_from_file_location(
        "kernel_auto_contracts", ROOT / "on1x/tools/on1x-contracts.py")
    module = importlib.util.module_from_spec(spec)
    # A read-only --check should not create __pycache__ in the source tree.
    saved = sys.dont_write_bytecode
    sys.dont_write_bytecode = True
    try:
        spec.loader.exec_module(module)
    finally:
        sys.dont_write_bytecode = saved
    return module


def uncomment(value):
    # The relevant MANIFEST values use plain scalars, not YAML quotes/anchors.
    return re.split(r"(?:^|[ \t]+)#", value, maxsplit=1)[0].rstrip()


def entries(text):
    """Frame the repository's MANIFEST subset without skipping bad name rows.

    The existing inventory reader validates complete buildable entries; a
    coverage projection also needs statuses and must accept reserved entries
    without an implementation. Reuse its dependency-list parser below.
    """
    found = inside = False
    current = None
    for number, raw in enumerate(io.StringIO(text, newline=None), 1):
        line = raw.rstrip()
        if not line or line.lstrip().startswith("#"):
            continue
        key = ROOT_KEY.fullmatch(line)
        if key:
            if inside and key[1] == "name":
                raise ValueError(f"MANIFEST.yaml:{number}: malformed module name")
            if current is not None:
                yield current
                current = None
            inside = key[1] == "modules"
            if inside:
                if found or uncomment(key[2]):
                    raise ValueError(f"MANIFEST.yaml:{number}: invalid/duplicate modules section")
                found = True
        elif inside:
            if not line.startswith(" "):
                if current is not None:
                    yield current
                name = re.fullmatch(r"- name: (" + NAME + r")", uncomment(line))
                if not name:
                    raise ValueError(f"MANIFEST.yaml:{number}: malformed module name")
                current = (number, name[1], [])
            elif current is None:
                raise ValueError(f"MANIFEST.yaml:{number}: module field before name")
            else:
                current[2].append((number, line))
    if current is not None:
        yield current
    if not found:
        raise ValueError("MANIFEST.yaml: missing modules section")


def project_entry(contracts, name, lines):
    fields = {}
    previous = None
    lists = {"dependencies": [], "extra_macros": []}
    for number, line in lines:
        field = FIELD.fullmatch(line)
        if field:
            previous, value = field[1], uncomment(field[2] or "")
            if previous in fields or previous == "name":
                raise ValueError(f"line {number}: duplicate field: {previous}")
            fields[previous] = value
            if previous in lists:
                lists[previous].append("  " + previous + ":" + (" " + value if value else ""))
        elif previous in lists and re.fullmatch(r"(?:  |    )- .+", line):
            lists[previous].append(uncomment(line))
        elif previous not in PROJECTED and previous and (
                line.startswith("    ") or re.fullmatch(r"  - .+", line)):
            # Ignore continuation lines belonging to non-projected fields
            # (for example, folded purpose prose).
            continue
        else:
            raise ValueError(f"line {number}: unsupported module field layout")

    header = fields.get("header")
    if header != f"include/mulib-{name}.h":
        raise ValueError("missing or noncanonical module header")
    status = fields.get("status")
    if status not in {"implemented", "bounded", "reserved"}:
        raise ValueError("missing or unsupported status")
    item = {
        "header": header,
        "status": status,
        "dependencies": contracts.manifest_list("\n".join(lists["dependencies"]), "dependencies", NAME),
    }
    macro, implementation = fields.get("macro"), fields.get("implementation")
    if status == "implemented" and implementation is None and re.fullmatch(
            r"MULIB_IMPL_[A-Z0-9_]+", macro or ""):
        item["macro"] = macro
    elif status == "bounded" and macro is None and implementation in {"static-inline", "static-data"}:
        item["implementation"] = implementation
    elif status != "reserved" or macro is not None or implementation is not None:
        raise ValueError("implementation metadata does not match status")
    if status == "reserved" and item["dependencies"]:
        raise ValueError("reserved MANIFEST module cannot declare dependencies")
    extra = contracts.manifest_list("\n".join(lists["extra_macros"]), "extra_macros",
                                    r"MULIB_IMPL_[A-Z0-9_]+", optional=True)
    if extra:
        if "macro" not in item:
            raise ValueError("non-external MANIFEST module cannot declare extra implementation switches")
        item["extra_macros"] = extra
    if "optional_dependency" in fields:
        value = fields["optional_dependency"]
        if not re.fullmatch(r"[A-Za-z0-9][ -~]{0,511}", value) or re.search(r":(?:\s|$)", value):
            raise ValueError("unsupported optional_dependency scalar")
        item["optional_dependency"] = value
    return item


def generate(root, *, raw_manifest=None):
    contracts = contract_tools()
    manifest = contracts.local(root, "MANIFEST.yaml")
    if raw_manifest is None:
        raw_manifest = contracts.read(manifest, MAX_MANIFEST_BYTES)
    if len(raw_manifest) > MAX_MANIFEST_BYTES:
        raise ValueError("MANIFEST.yaml: input byte limit exceeded")
    text = raw_manifest.decode("utf-8")
    if "\0" in text:
        raise ValueError("MANIFEST.yaml: unexpected NUL byte")
    modules = {}
    switches = {}
    for number, name, lines in entries(text):
        location = f"MANIFEST.yaml:{number}: {name}"
        if name in modules:
            raise ValueError(f"{location}: duplicate module name")
        if len(modules) >= MAX_MODULES:
            raise ValueError(f"{location}: module count limit exceeded")
        try:
            item = project_entry(contracts, name, lines)
            if item["status"] != "reserved":
                header = contracts.local(root, item["header"])
                if header != root / item["header"]:
                    raise ValueError("symlinked module header")
            if "macro" in item:
                macro = item["macro"]
                if macro in switches:
                    raise ValueError(f"implementation switch {macro} is also registered by {switches[macro]}")
                switches[macro] = name
        except ValueError as error:
            raise ValueError(f"{location}: {error}") from error
        modules[name] = item
    if not modules:
        raise ValueError("MANIFEST.yaml: no module entries")
    for name, item in modules.items():
        for dependency in item["dependencies"]:
            if dependency not in modules:
                raise ValueError(f"MANIFEST.yaml: {name}: unknown dependency: {dependency}")
            if modules[dependency]["status"] == "reserved":
                raise ValueError(f"MANIFEST.yaml: {name}: reserved MANIFEST dependency: {dependency}")
    eligible = {name: item for name, item in modules.items() if item["status"] != "reserved"}
    for alias, names in sorted(contracts.manifest_aliases(eligible).items()):
        if len(names) > 1:
            raise ValueError(f"ambiguous manifest alias: mulib.{alias} ({', '.join(sorted(names))})")
    registered = {item["header"] for item in modules.values()}
    headers = {path.relative_to(root).as_posix() for path in root.glob("include/mulib-*.h")}
    missing = sorted(headers - registered)
    if missing:
        raise ValueError("unregistered kernel header: " + missing[0] + "; register in MANIFEST.yaml")
    # Publication must also pass the shared consumer's registration checks,
    # including the manual and extra switch metadata not copied to the catalog.
    contracts.manifest_inventory(root, raw=raw_manifest, registration=True)
    data = {"schema": SCHEMA, "safe_export": False, "modules": eligible}
    output = (json.dumps(data, sort_keys=True, indent=2, ensure_ascii=True) + "\n").encode("ascii")
    if len(output) > MAX_CATALOG_BYTES:
        raise ValueError("automatic catalog byte limit exceeded")
    return output


def update(path, contents, *, check):
    if path.parent.resolve() != path.parent:
        raise ValueError(f"symlinked catalog directory: {path.parent}")
    try:
        mode = path.lstat().st_mode
    except FileNotFoundError:
        mode = None
    if mode is not None:
        if not stat.S_ISREG(mode):
            raise ValueError(f"not a regular catalog file: {path}")
        with path.open("rb") as stream:
            if stream.read(len(contents) + 1) == contents:
                return
    if check:
        raise ValueError(f"missing or stale catalog: {path}; regenerate with tools/extend-kernelbind.py")

    temporary = None
    try:
        with tempfile.NamedTemporaryFile(
                dir=path.parent, prefix=f".{path.name}.", delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(contents)
            os.chmod(temporary, 0o644 if mode is None else stat.S_IMODE(mode))
        os.replace(temporary, path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT,
                        help="MuLIB checkout to read and update (default: script's checkout)")
    parser.add_argument("--check", action="store_true",
                        help="check exact catalog bytes without writing")
    args = parser.parse_args()
    try:
        root = args.root.resolve()
        update(root / CATALOG, generate(root), check=args.check)
    except (OSError, RuntimeError, ValueError) as error:
        parser.exit(1, f"extend-kernelbind: {error}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
