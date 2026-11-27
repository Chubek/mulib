#!/usr/bin/env python3
"""Derive executable MuLIB recipes and diagnostic catalogues from C11 headers."""

import argparse
import fcntl
import json
import os
from pathlib import Path
import re
import sys
import tempfile
import tomllib

from compile_recipes import bound_document, prove_i64_body
from generate_portable_recipe import (
    CEPHYR_CONFIG, DOCUMENT_LIMIT, HERE, KERNEL, NODE_LIMIT, ROOT, Lower, Rejection,
    ast_body, atoms, declaration_sites, field, function_forms, identifier, index_ast_definitions,
    interface_definition, interface_model, interface_target, read_ast, read_interface,
    recipe_expression, render, require,
)
from kernel_inputs import INPUT_LIMIT, Inputs, digest, discover, implementation_macro, json_bytes, roots_for, source_path
from recipe_mir import directives, read_mir, verified_evidence
from recipe_totality import prove_totality


DEFAULT_CEPHYR_ROOT = CEPHYR_CONFIG.parent
MALFORMED = (Rejection, UnicodeError)
RECIPE_LIMIT = 65536
CATALOGUE_VIEW_LIMIT = 65536
STAGES = ("ast", "interface", "mir")
GENERATOR_FILES = ("ffispec-gen.py", "compile_recipes.py", "generate_portable_recipe.py",
                   "kernel_inputs.py", "recipe_mir.py", "recipe_totality.py")
PATH_PART = re.compile(r"[A-Za-z0-9_. +\-]{1,128}\Z")


def find_kernel(header):
    name = Path(header.relative).name
    match = re.fullmatch(r"mulib-(.+)\.h", name)
    if match and len(match[1]) <= 128 and KERNEL.fullmatch(match[1]) and header.path.name == name:
        return match[1]
    return None


def attempt(compiler, args, source, resource_root):
    views, documents, messages = {}, {}, {}
    stages = ((lambda argv, tu, **kw: compiler.parse_c11(argv, tu, format="sexp", **kw), read_ast),
              (compiler.emit_interface, read_interface), (compiler.emit_mir, read_mir))
    for stage, (invoke, parse) in zip(STAGES, stages):
        result = invoke(args, source, resource_root=resource_root)
        status = int(result.status)
        detail = result.diagnostics.decode("utf-8", "replace").strip()[:8192]
        if status not in (0, 1):
            raise RuntimeError(f"Cephyr {stage} infrastructure failure ({status}): {detail}")
        if not result.ok or not result.output.strip():
            views[stage] = {"status": "missing"}
            messages[stage] = detail or "Cephyr returned no document"
            continue
        try:
            require(len(result.output) <= DOCUMENT_LIMIT, "Cephyr view exceeds output budget")
            text = result.output.decode("utf-8")
            require("\0" not in text, "NUL in Cephyr view")
            parsed = parse(text)
        except MALFORMED as error:
            views[stage] = {"status": "unsupported"}
            messages[stage] = f"{error}\n{detail}".strip()
        else:
            views[stage] = {"status": "available", "sha256": digest(result.output)}
            documents[stage] = (parsed, text)
            if detail:
                messages[stage] = detail
    return views, documents, messages


def executable(header, kernel, source_hash, documents, provenance):
    ast, interface, mir = (documents[stage][0] for stage in STAGES)
    for site in ast["source"]["line_map"]:
        path = site["file"]
        if path not in ("<stdin>", "<built-in>"):
            source = Path(path)
            require(source.is_absolute() and source.resolve() in provenance,
                    f"untracked Cephyr AST source file: {path}")
    target, convention = interface_target(interface), interface_model(interface)
    definitions = index_ast_definitions(ast)
    inventory, seen_symbols = [], set()
    prefix = "mulib_" + kernel.replace("-", "_") + "_"
    for _, symbol, *fn in sorted(function_forms(interface), key=lambda form: form[1]):
        require(symbol not in seen_symbols, f"duplicate interface symbol {symbol!r}")
        seen_symbols.add(symbol)
        sites = tuple(declaration_sites(fn))
        if not any(path == str(header) for path, _, _ in sites):
            continue
        if not atoms(field(fn, "linkage"), "external"):
            continue
        inventory.append((str(symbol), fn))
    require(inventory, "no header-local external function definitions")
    # The profile describes an entire library. A partial public API is
    # deliberately not installed if any local export cannot be represented.
    require(len(inventory) <= 125, "too many portable exports")
    members, exports = set(), []
    lines = [f"(ffispec 1 (kernel {kernel}) (header {header.name})", f"  (sha256 {source_hash})"]
    for symbol, fn in inventory:
        try:
            identifier(symbol)
            member = identifier(symbol.removeprefix(prefix))
            require(member not in members, f"duplicate portable export member {member}")
            members.add(member)
            definition = interface_definition(fn, header, convention)
            line, params, _ = definition
            original = ast_body(definitions.get(symbol, []), header, symbol, definition)
            lower = Lower(params).statement(original)
            checked = recipe_expression(lower, len(params), [NODE_LIMIT - 1], tail=True)
            require(checked.kind == "i64", "C body does not return i64")
            prove_totality(checked.value, len(params))
            # Only publish an executable recipe if FFIHandler's independent
            # admission check also accepts it. One unsupported header must
            # not prevent unrelated kernels from building.
            prove_i64_body(checked.value)
            require(symbol in mir, "definition is absent from corroborating MIR")
            evidence = verified_evidence(mir[symbol], len(params))
            requests = directives(target, evidence)
        except (Rejection, ValueError) as error:
            raise Rejection(f"{symbol}: {error}") from error
        lines += [f"  (export {member} {symbol} ({' '.join(('params', *params))})",
                  "    (guard true)", f"    (body {render(checked.value, params)})",
                  *("    " + request for request in requests)]
        lines[-1] += ")"
        exports.append({"member": member, "symbol": symbol, "line": line, "arity": len(params),
                        "proof": "bounded-affine-i64-totality-v1", "mir_evidence": evidence,
                        "directives": requests})
    text = "\n".join([*lines, ")", ""])
    require(len(text) <= RECIPE_LIMIT and len(re.findall(r"[^ \t\r\n()]+", text)) <= 16384,
            "portable recipe exceeds document budget")
    try:
        bound_document(text)
    except ValueError as error:
        raise Rejection(str(error)) from error
    return text.encode("ascii"), target, exports


def quote(text):
    return json.dumps(text, ensure_ascii=True)


def catalogue_path(relative):
    parts = Path(relative).with_suffix(".ffi").parts
    # Use an escaped, flat namespace if a filename is unrepresentable, or if a
    # directory could collide with an ordinary output file.
    if any(not PATH_PART.fullmatch(part) or part in (".", "..") or part.endswith(".ffi")
           for part in parts[:-1]) or not PATH_PART.fullmatch(parts[-1]):
        return "catalogue/_opaque/" + digest(os.fsencode(relative)) + ".ffi"
    if parts[0] == "_opaque":
        return "catalogue/_opaque/" + digest(os.fsencode(relative)) + ".ffi"
    return "catalogue/" + "/".join(parts)


def catalogue(entry, details):
    lines = ["(ffi-catalogue 1", f"  (header {quote(entry['header'])})",
             f"  (relative-header {quote(entry['relative'])})", f"  (status {entry['status']})"]
    if "sha256" in entry:
        lines.append(f"  (sha256 {quote(entry['sha256'])})")
    for diagnostic in entry.get("diagnostics", []):
        lines.append(f"  (diagnostic {diagnostic['code']} {quote(diagnostic['detail'])})")
    for profile, macro, documents, messages in details:
        lines.append(f"  ({profile}")
        if macro is not None:
            lines.append(f"    (implementation-macro {quote(macro)})")
        for stage in STAGES:
            if stage in documents:
                text = documents[stage][1].strip()
                if len(text) <= CATALOGUE_VIEW_LIMIT:
                    lines.append(f"    ({stage}\n{text if stage != 'mir' else quote(text)}\n    )")
                else:
                    # The original document was validated instead of being
                    # replaced by a truncated (and unparseable) S-expression.
                    view = entry["attempts"][profile][stage]
                    lines.append(f"    ({stage}-omitted (sha256 {quote(view['sha256'])}) (reason catalogue-budget))")
            if stage in messages:
                lines.append(f"    ({stage}-diagnostics {quote(messages[stage])})")
        lines.append("  )")
    return "\n".join([*lines, ")", ""]).encode("utf-8")


def extract(header, compiler, args, data, sha, trusted_origin, provenance, resource_root):
    entry = {"header": str(header.path), "relative": header.relative,
             "shadowed": [str(path) for path in header.aliases], "attempts": {},
             "status": "malformed-input"}
    details = []

    def diagnose(code, detail):
        entry.setdefault("diagnostics", []).append({"code": code, "detail": str(detail)[:256]})

    kernel = find_kernel(header)
    if kernel is not None:
        entry["kernel"] = kernel
    if sha is not None:
        entry["sha256"] = sha
    try:
        require(not header.error, header.error)
        include = source_path(header.path)
        require(len(data) <= INPUT_LIMIT and b"\0" not in data, "header exceeds input budget or contains NUL")
    except (Rejection, ValueError) as error:
        diagnose("input", error)
    else:
        macro, ambiguous = implementation_macro(header.relative, data)
        if macro is not None:
            entry["implementation_macro"] = macro
        if ambiguous:
            diagnose("ambiguous-implementation", ", ".join(ambiguous))
        candidates = [("implementation", macro), ("declarations", None)] if macro else [("declarations", None)]
        first_documents = {}
        for profile, switch in candidates:
            source = (f"#define {switch} 1\n" if switch else "") + f'#include "{include}"\n'
            views, documents, messages = attempt(compiler, args, source, resource_root)
            entry["attempts"][profile] = views
            details.append((profile, switch, documents, messages))
            if profile == candidates[0][0]:
                first_documents = documents
            if documents.keys() == set(STAGES):
                entry["complete_profile"] = profile
                break
        if first_documents.keys() != set(STAGES):
            entry["status"] = "missing-views"
            missing = set(STAGES) - first_documents.keys()
            diagnose("missing-cephyr-views", ", ".join(sorted(missing)))
        else:
            entry["status"] = "unsupported"
            try:
                require(kernel is not None, "not a representable mulib-<kernel>.h recipe name")
                require(not ambiguous, "implementation switch is ambiguous")
                require(trusted_origin, "cannot verify canonical source origin across preprocessing dependencies")
                payload, target, exports = executable(header.path, kernel, sha, first_documents, provenance)
            except Rejection as error:
                diagnose("portable-profile", error)
            else:
                entry.update(file=f"recipes/{kernel}.ffi", status="generated", format="ffispec 1",
                             cephyr_target=target, exports=exports, output_sha256=digest(payload))
                return entry, payload
    entry.update(file=catalogue_path(header.relative), format="ffi-catalogue 1")
    payload = catalogue(entry, details)
    entry["output_sha256"] = digest(payload)
    return entry, payload


def file_id(path):
    path = path.resolve(strict=True)
    return [str(path), *file_stamp(path)]


def file_stamp(path):
    stat = path.lstat()
    return [stat.st_dev, stat.st_ino, stat.st_size, stat.st_mtime_ns, stat.st_ctime_ns]


def compiler_inputs(config, resource_root, library, compiler, roots):
    config_bytes = config.read_bytes()
    settings = tomllib.loads(config_bytes.decode("utf-8"))

    def table(name):
        value = settings.get(name, {})
        if not isinstance(value, dict):
            raise ValueError(f"expected Cephyr configuration [{name}] to be a table")
        return value

    preprocess = table("preprocess")

    def paths(key):
        values = preprocess.get(key, [])
        if not isinstance(values, list) or any(not isinstance(path, str) for path in values):
            raise ValueError(f"expected Cephyr preprocess.{key} to be an array of paths")
        return [config.parent / path for path in values]

    includes = [*paths("include_dirs"), *paths("system_include_dirs"), resource_root / "stdlib/include"]
    forced = paths("forced_includes")
    triple_script = table("paths").get("triple_script")
    tracked = [*(HERE / name for name in GENERATOR_FILES),
               *(resource_root / "python/cephyr").rglob("*.py"), *(resource_root / "triples").rglob("*.lua")]
    if triple_script is not None:
        if not isinstance(triple_script, str):
            raise ValueError("expected Cephyr paths.triple_script to be a path")
        tracked.append(config.parent / triple_script)
    # Native extensions, Lua hooks, and volatile preprocessor inputs can change
    # a TU without changing any discovered header. Keep such inputs diagnostic.
    extensions = table("extensions")
    reproducible = not (extensions.get("lua") or extensions.get("native"))
    external = library or os.environ.get("CEPHYR_LIBRARY")
    if not external:
        # A loader-search fallback may resolve a different library after any
        # invocation; don't reuse its results based on version text alone.
        return None, includes, forced, reproducible
    ids = [file_id(path) for path in sorted(tracked)]
    key = [str(config), digest(config_bytes), str(resource_root), file_id(Path(external)), compiler.version,
           [str(root) for root in roots], ids, sys.version_info[:3]]
    return (digest(json_bytes(key)) if reproducible else None), includes, forced, reproducible


def write_changed(path, payload):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.is_file() and not path.is_symlink() and path.read_bytes() == payload:
        return
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as stream:
        temporary = Path(stream.name)
        try:
            stream.write(payload)
            stream.close()
            temporary.replace(path)
        finally:
            temporary.unlink(missing_ok=True)


def read_cache(path):
    if not path.is_file() or path.is_symlink():
        return {}
    try:
        content = json.loads(path.read_bytes())
        return content if isinstance(content, dict) and content.get("version") == 1 else {}
    except ValueError:
        return {}


def managed_path(output, name):
    relative = Path(name)
    if (relative.is_absolute() or len(relative.parts) < 2 or relative.parts[0] not in {"catalogue", "recipes"}
            or ".." in relative.parts or relative.suffix != ".ffi"):
        raise ValueError(f"invalid generator output path: {name!r}")
    path = output / relative
    if not path.parent.resolve().is_relative_to(output) or path.parent.resolve() != path.parent:
        raise ValueError(f"generator output directory contains an alias: {path.parent}")
    return path


def generate(compiler, config, resource_root, roots, output, library):
    global_key, includes, forced, reproducible = compiler_inputs(config, resource_root, library, compiler, roots)
    args = ["--config", config, "--stdlib=cephyr", "--no-factir", "--no-blackstone", "--no-mir-opt"]
    for root in roots:
        args.extend(("-I", root))
    args.append("-")
    previous = read_cache(output / "index.json")
    state = read_cache(output / ".state.json")
    old_headers = {item["relative"]: item for item in previous.get("headers", [])
                   if isinstance(item, dict) and all(isinstance(item.get(field), str)
                                                     for field in ("relative", "file", "output_sha256"))}
    old_keys = state.get("headers", {}) if global_key and state.get("fingerprint") == global_key else {}
    scan = Inputs(roots, includes)
    forced_inputs = [scan.fingerprint(path.resolve(strict=True)) for path in forced]
    forced_keys = [key for key, _, _ in forced_inputs]
    forced_sources = frozenset().union(*(files for _, _, files in forced_inputs))
    cacheable = reproducible and all(key and trusted for key, trusted, _ in forced_inputs)
    entries, keys = [], {}
    preflight = False

    def probe():
        views, docs, messages = attempt(compiler, args, "long mulib_generator_probe(void) { return 0L; }\n",
                                        resource_root)
        if docs.keys() != set(STAGES):
            raise RuntimeError(f"Cephyr preflight failed: {views}, {messages}")

    for header in discover(roots):
        data = sha = None
        if header.error:
            fingerprint, trusted = digest(header.error.encode("utf-8", "backslashreplace")), False
            provenance = frozenset()
        else:
            data, sha = scan.read(header.path)
            fingerprint, trusted, provenance = scan.fingerprint(header.path)
        if not cacheable or not fingerprint:
            key = None
        else:
            key = digest(json_bytes([str(header.path), fingerprint, forced_keys,
                                     [str(path) for path in header.aliases]]))
        prior = old_headers.get(header.relative)
        remembered = old_keys.get(header.relative)
        if key and prior is not None and isinstance(remembered, dict) and remembered.get("input") == key:
            path = managed_path(output, prior["file"])
            if path.is_file() and not path.is_symlink():
                stamp = file_stamp(path)
                if stamp == remembered.get("output") or digest(path.read_bytes()) == prior["output_sha256"]:
                    entries.append(prior)
                    keys[header.relative] = {"input": key, "output": stamp}
                    continue
        if not preflight:
            probe()
            preflight = True
        entry, payload = extract(header, compiler, args, data, sha, trusted and cacheable,
                                 provenance | forced_sources, resource_root)
        if not header.error:
            after = Inputs(roots, includes)
            if (after.read(header.path)[1] != sha or after.fingerprint(header.path) != (fingerprint, trusted, provenance)
                    or [after.fingerprint(path.resolve(strict=True)) for path in forced] != forced_inputs):
                raise RuntimeError(f"header or dependency changed while extracting {header.path}")
        path = managed_path(output, entry["file"])
        write_changed(path, payload)
        entries.append(entry)
        if key:
            keys[header.relative] = {"input": key, "output": file_stamp(path)}
    entries.sort(key=lambda item: item["relative"])
    if not preflight and not entries and not (global_key and state.get("fingerprint") == global_key):
        probe()
    recipes = [entry for entry in entries if entry["status"] == "generated"]
    index = {"version": 1, "cephyr_version": compiler.version, "config": str(config),
             "kernel_roots": [str(root) for root in roots], "recipes": recipes, "headers": entries}
    active = {entry["file"] for entry in entries}
    old_files = {entry["file"] for entry in old_headers.values()}
    # recipes/ is owned by this generator and installed as a directory.
    # Drop even a validly named orphan left by an interrupted generation.
    recipe_dir = output / "recipes"
    if recipe_dir.is_dir() and not recipe_dir.is_symlink():
        old_files.update(f"recipes/{path.name}" for path in recipe_dir.glob("*.ffi")
                         if KERNEL.fullmatch(path.stem) and (path.is_file() or path.is_symlink()))
    for name in sorted(old_files - active):
        managed_path(output, name).unlink(missing_ok=True)
    write_changed(output / "index.json", json_bytes(index))
    write_changed(output / ".state.json", json_bytes({"version": 1, "fingerprint": global_key, "headers": keys}))
    counts = {status: sum(entry["status"] == status for entry in entries)
              for status in ("unsupported", "missing-views", "malformed-input")}
    print(f"MuLIB ffispec: {len(recipes)} recipes, {sum(counts.values())} catalogue entries "
          f"({counts['unsupported']} unsupported, {counts['missing-views']} missing views, "
          f"{counts['malformed-input']} malformed); see {output / 'index.json'}")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--cephyr-library", type=Path, help="Cephyr shared library (or set CEPHYR_LIBRARY)")
    parser.add_argument("--cephyr-resource-root", type=Path, default=DEFAULT_CEPHYR_ROOT,
                        help="Cephyr checkout with Python bindings, target scripts and stdlib (default: vendored)")
    parser.add_argument("--config", type=Path, help="Cephyr driver configuration (default: Cephyr.toml in resource root)")
    options = parser.parse_args(argv)
    try:
        resource_root = options.cephyr_resource_root.resolve()
        binding = resource_root / "python/cephyr/__init__.py"
        if not binding.is_file() or not all((resource_root / name).is_dir() for name in ("triples", "stdlib/include")):
            raise ValueError(f"invalid Cephyr resource root (expected python/cephyr, triples and stdlib/include): "
                             f"{resource_root}")
        config = (options.config or resource_root / "Cephyr.toml").resolve(strict=True)
        roots = roots_for(os.environ.get("MULIB_KERNELS_PATH"), ROOT / "kernels")
        output = options.output_dir.resolve()
        if output == ROOT or output.is_relative_to((ROOT / "on1x/stdffi/mulib").resolve()) or any(
                output.is_relative_to(root) for root in roots):
            raise ValueError("generate into a build directory outside kernel and stdffi source directories")
        sys.path.insert(0, str(resource_root / "python"))
        import cephyr

        loaded = getattr(cephyr, "__file__", None)
        if loaded is None or Path(loaded).resolve() != binding.resolve():
            raise ImportError(f"Cephyr Python binding is not from the selected resource root: {resource_root}")
        compiler = cephyr.Compiler(options.cephyr_library)
        output.mkdir(parents=True, exist_ok=True)
        with (output / ".lock").open("ab") as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            generate(compiler, config, resource_root, roots, output, options.cephyr_library)
    except (ImportError, OSError, RuntimeError, ValueError) as error:
        parser.exit(1, f"ffispec-gen: {error}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
