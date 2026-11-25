#!/usr/bin/env python3
"""Build or inspect a checked On1x kernel package without loading its payload."""

import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import stat
import struct
import subprocess
import sys
import tempfile


HERE = Path(__file__).resolve().parent
SOURCE_FFI = HERE.parent / "hir/FFIHandler"
INSTALLED_FFI = HERE.parent / "share/on1x/kernel-package"
FFI = SOURCE_FFI if (SOURCE_FFI / "compile_recipes.py").is_file() else INSTALLED_FFI
sys.path.insert(0, str(FFI))

import compile_recipes as checks
from generate_portable_recipe import (
    CEPHYR_CONFIG, Rejection, atoms, declaration_sites, field, function_forms,
    i64_type, interface_model, interface_target, signature,
)
from kernel_inputs import Inputs, implementation_macro, source_path
from portable_recipe_contract import load_portable_recipe_contract


MAGIC = b"ON1XKP\r\n"
RECIPE_MAX = 64 * 1024
IMAGE_MAX = 4 * 1024 * 1024
PACKAGE_MAX = 8192 + RECIPE_MAX + IMAGE_MAX
MODULE = "stdffi.clamp"
HEADER = "mulib-clamp.h"
ARITIES = {"add_i64": 2, "i64": 3, "sub_i64": 2}
OPS = {values[0]: key for key, values in checks.OPS.items()}
MODULE_NAME = re.compile(r"[a-z][a-z0-9_]*(?:\.[a-z][a-z0-9_]*)+")
EXPORT_NAME = re.compile(r"[a-z][a-z0-9_]*")
require = checks.require
sha256 = checks.sha256


def load_generator():
    spec = importlib.util.spec_from_file_location("ffispec_gen", FFI / "ffispec-gen.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def recipe_document(data, parser):
    text = data.decode("ascii")
    require(0 < len(data) <= RECIPE_MAX and b"\0" not in data, "invalid recipe size or NUL")
    checks.bound_document(text)
    try:
        parsed, _ = parser.parse(text).children
    except parser.LanguageError as error:
        raise ValueError(f"invalid FFI recipe: {error}") from error
    _, tag, version, clauses, _ = parsed.children
    return [checks.spelling(tag), checks.spelling(version),
            *map(checks.field_tree, checks.list_items(clauses))]


def expression(table, index):
    op, _, a, b, c, value = table.nodes[index]
    if op == "Bool":
        return "true" if value else "false"
    if op == "Param":
        return f"${value}"
    if op == "Integer":
        return value
    return (OPS[op], *(expression(table, i) for i in ((a, b, c) if op == "If" else (a, b))))


def render(tree, params):
    if isinstance(tree, (tuple, list)):
        return "(" + " ".join(render(item, params) for item in tree) + ")"
    if isinstance(tree, str) and tree.startswith("$") and params is not None:
        return params[int(tree[1:])]
    return str(tree)


def interface_hash(module, name, arity):
    params = ",".join(["i64"] * arity)
    return sha256(f"on1x.kernel.interface/v1\nmodule={module}\nexport={name}\n"
                  f"signature=({params})->i64\n".encode("ascii"))


def behavior_hash(guard, body):
    return sha256(f"(guard {render(guard, None)})\n(body {render(body, None)})\n".encode("ascii"))


def verify_recipe(doc, module, source_sha):
    kernel = module.removeprefix("stdffi.")
    table = checks.RecipeTable()
    table.document(doc, kernel, f"mulib-{kernel}.h", source_sha)
    return {member: (symbol, arity, expression(table, guard), expression(table, body))
            for (_, member, symbol, _, _, _, arity), (guard, body, _, _)
            in zip(table.exports, table.recipes)}


def metadata(module, exports):
    return [{"name": name, "arity": arity,
             "interface_sha256": interface_hash(module, name, arity),
             "behavior_sha256": behavior_hash(guard, body)}
            for name, (_, arity, guard, body) in sorted(exports.items())]


def declarations_for(forms, header, expected, *, with_definitions):
    convention = interface_model(forms)
    seen = {}
    for _, symbol, *fn in function_forms(forms):
        sites = tuple(declaration_sites(fn))
        if not any(site[0] == str(header) for site in sites):
            continue
        declarations = [site for site in sites if not site[2]]
        require(symbol in expected and symbol not in seen
                and len(sites) == (2 if with_definitions else 1) and len(declarations) == 1
                and all(site[0] == str(header) for site in sites)
                and atoms(field(fn, "definition"), "defined" if with_definitions else "declaration")
                and atoms(field(fn, "linkage"), "external")
                and atoms(field(fn, "calling-convention"), convention)
                and field(fn, "storage")
                and all(item in ("none", "extern") for item in field(fn, "storage"))
                and not field(fn, "specifiers"),
                f"unsupported declaration provenance for {symbol}")
        result, params = signature(fn)
        rank = i64_type(result)
        require(len(params) == expected[symbol] and all(i64_type(param) == rank for param in params),
                f"type mismatch for {symbol}")
        seen[symbol] = (rank, declarations[0][1])
    require(seen.keys() == expected.keys(), "unexpected Cephyr declaration inventory")
    return interface_target(forms), seen


def checked_clamp(args, parser):
    gen = load_generator()
    header = args.header.resolve(strict=True)
    require(header.is_file() and header.name == HEADER, f"expected real {HEADER}")
    library = args.cephyr_library.resolve(strict=True)
    default_resource = CEPHYR_CONFIG.parent
    if not (default_resource / "Cephyr.toml").is_file():
        default_resource = library.parent.parent / "share/cephyr"
    resource = (args.cephyr_resource_root or default_resource).resolve(strict=True)
    config = (args.config or resource / "Cephyr.toml").resolve(strict=True)
    binding = next((root / "cephyr/__init__.py" for root in
                    (resource / "python", library.parent / "cephyr/python")
                    if (root / "cephyr/__init__.py").is_file()), None)
    require(binding is not None and all((resource / name).is_dir()
            for name in ("triples", "stdlib/include")), "invalid Cephyr resource root or missing Python binding")
    sys.path.insert(0, str(binding.parents[1]))
    import cephyr

    require(Path(cephyr.__file__).resolve() == binding.resolve(), "unexpected Cephyr Python binding")
    compiler = cephyr.Compiler(library)
    roots = [header.parent]

    def binding_state():
        return [gen.file_id(file) for file in sorted(binding.parent.rglob("*.py"))]

    initial_binding = binding_state()
    state, includes, forced, reproducible = gen.compiler_inputs(config, resource, library, compiler, roots)
    require(state and reproducible and not forced, "cannot establish reproducible Cephyr provenance")
    scan = Inputs(roots, includes)
    source, source_sha = scan.read(header)
    fingerprint, origin, files = scan.fingerprint(header)
    require(fingerprint and origin, "cannot verify clamp preprocessing provenance")
    macro, ambiguous = implementation_macro(HEADER, source)
    require(macro == "MULIB_IMPL_CLAMP" and not ambiguous, "missing canonical clamp implementation")
    symbols = {f"mulib_clamp_{name}": arity for name, arity in ARITIES.items()}
    contract = load_portable_recipe_contract(FFI / "contracts/clamp.portable-recipe.json",
                                             kernel="clamp", revision=f"sha256:{source_sha}",
                                             arities=symbols)
    argv = ["--config", config, "--stdlib=cephyr", "--no-factir", "--no-blackstone",
            "--no-mir-opt", "-I", header.parent, "-"]
    include = f'#include "{source_path(header)}"\n'
    _, declarations, notes = gen.attempt(compiler, argv, include, resource)
    require(declarations.keys() == set(gen.STAGES), f"missing Cephyr declaration views: {notes}")
    for site in declarations["ast"][0]["source"]["line_map"]:
        path = site["file"]
        if path not in ("<stdin>", "<built-in>"):
            tracked = Path(path)
            require(tracked.is_absolute() and tracked.resolve() in files,
                    f"untracked Cephyr declaration source: {path}")
    _, definitions, notes = gen.attempt(compiler, argv, f"#define {macro} 1\n{include}", resource)
    require(definitions.keys() == set(gen.STAGES), f"missing Cephyr implementation views: {notes}")
    require(declarations_for(declarations["interface"][0], header, symbols, with_definitions=False)
            == declarations_for(definitions["interface"][0], header, symbols, with_definitions=True),
            "Cephyr declarations differ between declaration and implementation views")
    recipe, _, _ = gen.executable(header, "clamp", source_sha, definitions, files)
    require(Inputs(roots, includes).fingerprint(header) == (fingerprint, origin, files)
            and gen.compiler_inputs(config, resource, library, compiler, roots)[0] == state
            and binding_state() == initial_binding,
            "Cephyr provenance changed during extraction")
    exports = verify_recipe(recipe_document(recipe, parser), MODULE, source_sha)
    require({name: (symbol, arity) for name, (symbol, arity, _, _) in exports.items()}
            == {name: (f"mulib_clamp_{name}", arity) for name, arity in ARITIES.items()},
            "unexpected clamp export inventory")

    # The checked-in companion specifies the public precondition. Cephyr
    # supplies the body, not the manual's precondition, and proves it total.
    lines = [f"(ffispec 1 (kernel clamp) (header {HEADER})", f"  (sha256 {source_sha})"]
    for name, (symbol, arity, _, body) in sorted(exports.items()):
        params = [f"p{i}" for i in range(arity)]
        lines.extend((f"  (export {name} {symbol} (params {' '.join(params)})",
                      f"    (guard {render(contract[symbol].guard, params)})",
                      f"    (body {render(body, params)}))"))
    recipe = "\n".join([*lines, ")", ""]).encode("ascii")
    return recipe, verify_recipe(recipe_document(recipe, parser), MODULE, source_sha)


def c_expression(tree):
    if isinstance(tree, int):
        if tree == checks.MIN_I64:
            return "(-INT64_C(9223372036854775807) - INT64_C(1))"
        return f"(-INT64_C({-tree}))" if tree < 0 else f"INT64_C({tree})"
    if isinstance(tree, str):
        return {"true": "1", "false": "0"}.get(tree, f"p{tree[1:]}")
    op, *values = tree
    a, b, *tail = map(c_expression, values)
    if op == "if":
        return f"({a} ? {b} : {tail[0]})"
    operator = {"lt": "<", "le": "<=", "gt": ">", "ge": ">=", "eq": "==",
                "and": "&&", "or": "||", "add": "+", "sub": "-"}[op]
    return f"({a} {operator} {b})"


def native_image(exports, cc):
    lines = ["#include <stdint.h>", "#if !defined(__x86_64__) || !defined(__linux__) || !defined(__LP64__)",
             '#error "native kernel packages require an AMD64 Linux LP64 C toolchain"', "#endif"]
    for name, (_, arity, guard, body) in sorted(exports.items()):
        params = ", ".join([*(f"int64_t p{i}" for i in range(arity)), "int64_t *out"])
        lines.extend((f'__attribute__((visibility("default")))',
                      f"int on1x_kernel_native_{name}({params}) {{",
                      f"    if (!out || !({c_expression(guard)})) return 1;",
                      f"    *out = {c_expression(body)};", "    return 0;", "}"))
    with tempfile.TemporaryDirectory(prefix="on1x-kernel-package-") as directory:
        output = Path(directory) / "adapter.so"
        # GNU ld otherwise emits a zero-length .eh_frame / PT_LOAD with some
        # Clang releases, even when the translation unit has no unwind data.
        result = subprocess.run([cc, "-fuse-ld=bfd", "-x", "c", "-", "-std=c11", "-O2", "-march=x86-64",
                                 "-fPIC", "-fvisibility=hidden", "-fno-stack-protector",
                                 "-fno-asynchronous-unwind-tables", "-fno-unwind-tables",
                                 "-nostdlib", "-shared",
                                 "-Wl,--hash-style=sysv,-z,noexecstack,-z,defs,--build-id=none,"
                                 "--no-ld-generated-unwind-info",
                                 "-o", str(output)],
                                input="\n".join([*lines, ""]).encode("ascii"), check=False)
        require(result.returncode == 0, "native adapter compilation failed")
        image = read_bytes(output, IMAGE_MAX)
    require(len(image) >= 64 and image[:7] == b"\x7fELF\x02\x01\x01"
            and image[16:20] == b"\x03\x00\x3e\x00", "expected an AMD64 ELF shared image")
    return image


def read_bytes(path, limit):
    require(not path.is_symlink(), f"not a regular file: {path}")
    flags = getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_NONBLOCK", 0)
    with open(path, "rb", opener=lambda name, mode: os.open(name, mode | flags)) as stream:
        require(stat.S_ISREG(os.fstat(stream.fileno()).st_mode), f"not a regular file: {path}")
        payload = stream.read(limit + 1)
    require(len(payload) <= limit, f"input exceeds size limit: {path}")
    return payload


def wire_name(name):
    data = name.encode("ascii")
    return bytes([len(data)]) + data


def pack(recipe, image, exports):
    rows = metadata(MODULE, exports)
    output = bytearray(MAGIC + struct.pack(">5B3I", 1, 2 if image else 1, len(rows), 0, 0,
                                           1, len(recipe), len(image)))
    output += wire_name(MODULE)
    for row in rows:
        output += wire_name(row["name"]) + bytes([row["arity"]])
        output += bytes.fromhex(row["interface_sha256"]) + bytes.fromhex(row["behavior_sha256"])
    if image:
        output += bytes.fromhex(sha256(image))
    output += recipe + image
    return bytes(output)


def inspect(payload, parser):
    offset = 0

    def take(size):
        nonlocal offset
        require(size <= len(payload) - offset, "truncated kernel package")
        field_data = payload[offset:offset + size]
        offset += size
        return field_data

    def name(pattern):
        length, = take(1)
        require(1 <= length <= 63, "invalid package identifier length")
        value = take(length).decode("ascii")
        require(pattern.fullmatch(value), f"invalid package identifier {value!r}")
        return value

    require(take(len(MAGIC)) == MAGIC, "invalid kernel package magic")
    fmt, kind, count, dep_count, conflict_count, version, size, image_size = struct.unpack(">5B3I", take(17))
    require(fmt == 1 and kind in (1, 2) and 1 <= count <= 16 and dep_count <= 16
            and conflict_count <= 16 and version > 0 and 0 < size <= RECIPE_MAX
            and ((kind == 1 and image_size == 0) or (kind == 2 and 0 < image_size <= IMAGE_MAX)),
            "invalid kernel package header")
    module = name(MODULE_NAME)
    exports = []
    for _ in range(count):
        export = name(EXPORT_NAME)
        arity, = take(1)
        require(arity in (2, 3), "invalid kernel export arity")
        exports.append({"name": export, "arity": arity, "interface_sha256": take(32).hex(),
                        "behavior_sha256": take(32).hex()})
    deps = []
    for _ in range(dep_count):
        dep = name(MODULE_NAME)
        required_version, = struct.unpack(">I", take(4))
        require(required_version > 0, "invalid dependency version")
        deps.append({"module": dep, "version": required_version})
    conflicts = [name(MODULE_NAME) for _ in range(conflict_count)]
    deps_names = [dep["module"] for dep in deps]
    for names in ([row["name"] for row in exports], deps_names, conflicts):
        require(names == sorted(set(names)), "unsorted or duplicate package entries")
    require(module not in deps_names and module not in conflicts and not (set(deps_names) & set(conflicts)),
            "contradictory kernel constraints")
    image_sha = take(32).hex() if kind == 2 else None
    recipe = take(size)
    image = take(image_size)
    require(offset == len(payload), "trailing data in kernel package")
    require(image_sha is None or sha256(image) == image_sha, "native image SHA-256 mismatch")
    doc = recipe_document(recipe, parser)
    scope = checks.fields(doc[2:], {"kernel", "header", "sha256", "backend", "target"}, {"export"})
    source_sha = checks.atom(scope.get("sha256", []), "sha256", checks.DIGEST)
    require(module.startswith("stdffi."), "no checked recipe profile for this module")
    checked = verify_recipe(doc, module, source_sha)
    require(exports == metadata(module, checked), "package manifest differs from its checked recipe")
    return {"format": fmt, "kind": "portable" if kind == 1 else "native", "module": module,
            "version": version, "exports": exports, "dependencies": deps, "conflicts": conflicts,
            "source_sha256": source_sha, "recipe_sha256": sha256(recipe), "recipe_size": size,
            "image_sha256": image_sha, "image_size": image_size, "package_sha256": sha256(payload)}


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    commands = cli.add_subparsers(dest="command", required=True)
    build = commands.add_parser("build", help="derive a clamp v1 package from checked C source")
    build.add_argument("--header", type=Path, required=True)
    build.add_argument("--cephyr-library", type=Path, required=True)
    env_root = os.environ.get("ON1X_STDFFI_CEPHYR_RESOURCE_ROOT")
    build.add_argument("--cephyr-resource-root", type=Path,
                       default=Path(env_root) if env_root else None,
                       help="Cephyr resources (default: checkout or selected library's installation)")
    build.add_argument("--config", type=Path, help="Cephyr TOML (default: Cephyr.toml in resource root)")
    build.add_argument("--kind", choices=("portable", "native"), default="portable")
    build.add_argument("--output", type=Path, required=True)
    build.add_argument("--cc", default="cc", help="AMD64 Linux C compiler with GNU ld.bfd (native build only)")
    show = commands.add_parser("inspect", help="validate metadata and recipe; never load or execute an image")
    show.add_argument("package", type=Path)
    for command in (build, show):
        command.add_argument("--parser", type=Path, default=FFI / "FFIHandler.py",
                             help="trusted generated FFIHandler Python parser (installed default)")
    args = cli.parse_args()
    try:
        parser = checks.load_parser(args.parser.resolve(strict=True))
        if args.command == "inspect":
            print(json.dumps(inspect(read_bytes(args.package, PACKAGE_MAX), parser), indent=2))
        else:
            require(args.output.suffix == ".on1xkp" and not args.output.is_symlink(),
                    "output must be a non-symlink .on1xkp file")
            inputs = (args.header, args.cephyr_library, args.config, args.parser)
            require(not any(path and args.output.resolve() == path.resolve() for path in inputs),
                    "output must not overwrite a build input")
            recipe, exports = checked_clamp(args, parser)
            image = native_image(exports, args.cc) if args.kind == "native" else b""
            payload = pack(recipe, image, exports)
            inspect(payload, parser)
            args.output.parent.mkdir(parents=True, exist_ok=True)
            checks.write_if_changed(args.output, payload)
    except (OSError, ValueError, ImportError, Rejection, RuntimeError) as error:
        cli.exit(1, f"on1x-kernel-package: {error}\n")


if __name__ == "__main__":
    main()
