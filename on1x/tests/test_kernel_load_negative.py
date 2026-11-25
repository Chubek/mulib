"""Late KernelBind startup rejections and transactional rollback."""

import argparse
from dataclasses import dataclass, replace
import hashlib
import os
from pathlib import Path
import re
import struct
import sys
import tempfile

from support import run


# Keep the small v1 fixture writer independent of the production encoder:
# negative fixtures must reach the runtime, even if the pack CLI rejects them.
HEADER = struct.Struct(">8sBBBBBIII")
MAGIC = b"ON1XKP\r\n"
MODULE = "stdffi.clamp"
PACKAGE_ROOTS = "ON1X_KERNEL_PACKAGE_ROOTS"
NATIVE_POLICY = "ON1X_KERNEL_NATIVE_POLICY"
TRACE = "ON1X_KERNEL_IMPORT_TRACE"
PROVIDER_TRACE = "ON1X_KERNEL_TRACE"
EXPECTED_STDOUT = "123\n10\n9223372036854775807\n-9223372036854775808\n"
FIXTURES = Path(__file__).resolve().parent / "fixtures/kernel_load_negative"
ENV = {key: value for key, value in os.environ.items()
       if not key.startswith(("ON1X_", "LD_"))}


def sha256(data):
    return hashlib.sha256(data).digest()


def interface(module, export, arity):
    signature = "(" + ",".join(["i64"] * arity) + ")->i64"
    return sha256(("on1x.kernel.interface/v1\n"
                   f"module={module}\nexport={export}\nsignature={signature}\n").encode("ascii"))


@dataclass(frozen=True)
class Export:
    name: str
    arity: int
    interface: bytes
    behavior: bytes


@dataclass(frozen=True)
class Package:
    module: str
    version: int
    exports: tuple
    recipe: bytes
    deps: tuple = ()
    conflicts: tuple = ()
    image: bytes = b""

    def wire(self):
        def name(text):
            encoded = text.encode("ascii")
            return bytes([len(encoded)]) + encoded

        records = [name(self.module)]
        records += [name(e.name) + bytes([e.arity]) + e.interface + e.behavior
                    for e in self.exports]
        records += [name(dep) + struct.pack(">I", ver) for dep, ver in self.deps]
        records += [name(dep) for dep in self.conflicts]
        if self.image:
            records.append(sha256(self.image))
        records.extend([self.recipe, self.image])
        return (HEADER.pack(MAGIC, 1, 2 if self.image else 1, len(self.exports),
                            len(self.deps), len(self.conflicts), self.version,
                            len(self.recipe), len(self.image)) + b"".join(records))

    def policy(self):
        lines = ["on1x-native-policy-v1",
                 f"x86_64-unknown-linux-gnu sysv-clamp-v1 {self.module} {self.version} "
                 f"{sha256(self.image).hex()}"]
        lines.extend(f"{e.name} {e.arity} on1x_kernel_native_{e.name} "
                     f"{e.interface.hex()} {e.behavior.hex()}" for e in self.exports)
        return "\n".join([*lines, ""]).encode("ascii")


def baseline():
    # Metadata and behavior are independently computed from the vetted fixture.
    # Nonsemantic source hints are deliberately omitted.
    lower, upper = "-9223372036854775808", "9223372036854775807"
    exprs = [
        ("add_i64", ["a", "b"], "true",
         f"(if (and (gt b 0) (gt a (sub {upper} b))) {upper} "
         f"(if (and (lt b 0) (lt a (sub {lower} b))) {lower} (add a b)))"),
        ("i64", ["x", "lo", "hi"], "(le lo hi)", "(if (lt x lo) lo (if (gt x hi) hi x))"),
        ("sub_i64", ["a", "b"], "true",
         f"(if (and (lt b 0) (gt a (add {upper} b))) {upper} "
         f"(if (and (gt b 0) (lt a (add {lower} b))) {lower} (sub a b)))"),
    ]
    provenance = "cf3d0cf6e27b0d2844e25ea1e5bf7637a34508d29c42411aa855dfe97a1205e0"
    parts = [f"(ffispec 1 (kernel clamp) (header mulib-clamp.h) (sha256 {provenance})"]
    exports = []
    for name, params, guard, body in exprs:
        clauses = f"(guard {guard})\n(body {body})\n"
        parts.append(f"(export {name} mulib_clamp_{name} (params {' '.join(params)}) {clauses})")
        lookup = {param: f"${i}" for i, param in enumerate(params)}
        checked = re.sub(r"\b[a-z_]+\b", lambda word: lookup.get(word[0], word[0]), clauses)
        exports.append(Export(name, len(params), interface(MODULE, name, len(params)),
                              sha256(checked.encode("ascii"))))
    return Package(MODULE, 1, tuple(exports), ("\n".join(parts) + "\n)\n").encode("ascii"))


def ok(*cmd, **extra_env):
    result = run(cmd, env={**ENV, **extra_env})
    assert result.returncode == 0, (result.args, result.returncode, result.stdout, result.stderr)
    return result


def rejected(exe, name, diagnostic, **extra_env):
    result = run([exe], env={**ENV, **extra_env})
    assert result.returncode > 0, (name, result.returncode, result.stdout, result.stderr)
    assert not result.stdout, (name, "application ran despite failed imports", result.stdout)
    assert re.search(diagnostic, result.stderr, re.IGNORECASE), (name, diagnostic, result.stderr)
    return result.stderr


def put(root, *packages):
    root.mkdir(parents=True, exist_ok=True)
    for package in packages:
        (root / (package.module + ".on1xkp")).write_bytes(package.wire())
    return str(root)


def auxiliary(base, module, deps=()):
    kernel = module.removeprefix("stdffi.").encode("ascii")
    recipe = (base.recipe.replace(b"(kernel clamp)", b"(kernel " + kernel + b")")
              .replace(b"mulib-clamp.h", b"mulib-" + kernel + b".h")
              .replace(b"mulib_clamp_", b"mulib_" + kernel + b"_"))
    exports = tuple(replace(e, interface=interface(module, e.name, e.arity))
                    for e in base.exports)
    return replace(base, module=module, exports=exports, deps=tuple(deps), recipe=recipe)


def structured_rejections(exe, work, base):
    original = base.wire()
    def bad_header(offset, value):
        return original[:offset] + value + original[offset + len(value):]

    wire_cases = [
        ("bad-magic", b"BAD" + original[3:], r"malformed package"),
        ("unknown-format-version", bad_header(8, bytes([2])), r"malformed package"),
        ("unknown-provider-kind", bad_header(9, bytes([3])), r"malformed package"),
        ("non-ascii-module", bad_header(HEADER.size + 1, b"\xff"), r"malformed package"),
        ("duplicate-export", replace(base, exports=(base.exports[0],) * 3).wire(),
         r"malformed package"),
        ("truncated-header", original[:HEADER.size - 1], r"malformed package"),
        ("truncated-payload", original[:-1], r"malformed package"),
        ("trailing-bytes", original + b"\0", r"malformed package"),
        ("length-disagreement", bad_header(17, struct.pack(">I", len(base.recipe) - 1)),
         r"malformed package"),
        ("recipe-size-limit", replace(base, recipe=base.recipe.ljust(65537)).wire(),
         r"malformed package"),
        ("export-count-limit", bad_header(10, bytes([17])), r"malformed package"),
        ("dep-count-limit", bad_header(11, bytes([17])), r"malformed package"),
        ("conflict-count-limit", bad_header(12, bytes([17])), r"malformed package"),
        ("recipe-syntax", replace(base, recipe=b"(" + base.recipe).wire(), r"recipe"),
    ]
    for name, data, diagnostic in wire_cases:
        root = work / name
        root.mkdir()
        (root / (MODULE + ".on1xkp")).write_bytes(data)
        rejected(exe, name, diagnostic, **{PACKAGE_ROOTS: str(root)})

    empty = put(work / "absent")
    rejected(exe, "absent", r"package not found", **{PACKAGE_ROOTS: empty})
    good_root = put(work / "good", base)
    assert ok(exe, **{PACKAGE_ROOTS: good_root}).stdout == EXPECTED_STDOUT
    put(exe.parent, base)
    rejected(exe, "no roots with adjacent package", r"no explicit package roots")
    rejected(exe, "absent with adjacent package", r"package not found", **{PACKAGE_ROOTS: empty})
    rejected(exe, "empty root entry", r"root", **{PACKAGE_ROOTS: good_root + ":"})
    rejected(exe, "root count", r"roots?", **{PACKAGE_ROOTS: ":".join([good_root] * 17)})
    copy_root = put(work / "duplicate", base)
    rejected(exe, "duplicate package", r"ambiguous package",
             **{PACKAGE_ROOTS: good_root + ":" + copy_root})

    oversize = Path(put(work / "package-size-limit")) / (MODULE + ".on1xkp")
    with oversize.open("wb") as out:
        out.write(original)
        out.truncate(4 * 1024 * 1024 + 65536 + 8192 + 1)
    rejected(exe, "package size limit", r"package.*(?:size|limit|large|bound)|too large",
             **{PACKAGE_ROOTS: str(oversize.parent)})

    first, clamp, last = base.exports
    absent = replace(base, exports=(first, clamp),
                     recipe=base.recipe.split(b"(export sub_i64", 1)[0] + b")\n")
    forged = replace(first, interface=b"\0" * 32)
    wrong_arity = replace(first, arity=3, interface=interface(MODULE, first.name, 3))
    # Same declared signature, different (well-typed) i64 body and genuinely
    # recomputed behavior hash. A separate case keeps the stale baseline hash.
    changed_recipe = base.recipe.replace(b"(if (gt x hi) hi x)", b"(if (gt x hi) hi lo)")
    assert changed_recipe != base.recipe, base.recipe
    behavior = sha256(b"(guard (le $1 $2))\n"
                      b"(body (if (lt $0 $1) $1 (if (gt $0 $2) $2 $1)))\n")
    assert behavior != clamp.behavior
    changed = replace(clamp, behavior=behavior)
    too_deep = base.recipe.replace(b"(if (lt x lo) lo (if (gt x hi) hi x))",
                                   b"(add 0 " * 34 + b"x" + b")" * 34)
    assert too_deep != base.recipe
    wrong_type = base.recipe.replace(b"(guard (le lo hi))", b"(guard x)")
    assert wrong_type != base.recipe
    # Claim the saturating contract but carry plain addition instead.
    plain_add = re.sub(rb"(?m)^\(body \(if \(and \(gt b 0\).*",
                       b"(body (add a b))", base.recipe)
    assert plain_add != base.recipe
    for name, package, diagnostic in [
        ("missing-export", absent, r"missing export|export not found"),
        ("invalid-signature-hash", replace(base, exports=(forged, clamp, last)),
         r"malformed package"),
        ("signature-substitution", replace(base, exports=(wrong_arity, clamp, last)),
         r"arity|interface|signature"),
        ("stale-behavior-hash", replace(base, recipe=changed_recipe), r"behavior"),
        ("unsaturated-add-with-stale-hash", replace(base, recipe=plain_add), r"behavior"),
        ("behavior-substitution", replace(base, recipe=changed_recipe, exports=(first, changed, last)),
         r"behavior"),
        ("recipe-depth-limit", replace(base, recipe=too_deep), r"depth|nest|limit"),
        ("recipe-type-error", replace(base, recipe=wrong_type), r"recipe"),
    ]:
        rejected(exe, name, diagnostic, **{PACKAGE_ROOTS: put(work / name, package)})

    dep = "stdffi.aux"
    required = replace(base, deps=((dep, 1),))
    rejected(exe, "missing dep", r"package not found",
             **{PACKAGE_ROOTS: put(work / "missing-dep", required)})
    rejected(exe, "dependency cycle", r"dependency cycle",
             **{PACKAGE_ROOTS: put(work / "cycle", required, auxiliary(base, dep, [(MODULE, 1)]))})
    rejected(exe, "version mismatch", r"dependency version mismatch",
             **{PACKAGE_ROOTS: put(work / "version", required,
                                   replace(auxiliary(base, dep), version=2))})
    rejected(exe, "package conflict", r"package conflict",
             **{PACKAGE_ROOTS: put(work / "conflict", required,
                                   replace(auxiliary(base, dep), conflicts=(MODULE,)))})
    modules = [f"stdffi.node{i:02}" for i in range(64)]
    chain = [replace(base, deps=((modules[0], 1),))]
    chain.extend(auxiliary(base, module, [(modules[i + 1], 1)] if i + 1 < len(modules) else ())
                 for i, module in enumerate(modules))
    rejected(exe, "graph node limit", r"package.*limit|too many packages",
             **{PACKAGE_ROOTS: put(work / "package-count-limit", *chain)})
    return good_root


def native_rejections(args, exe, work, base):
    def image(name, *options, nostdlib=True):
        shared = work / f"{name}.so"
        ok(args.cc, "-std=c11", "-O2", "-shared", "-fPIC", "-fvisibility=hidden",
           "-fno-unwind-tables", "-fno-asynchronous-unwind-tables", "-fuse-ld=bfd",
           "-Wl,--hash-style=sysv,-z,noexecstack,--no-ld-generated-unwind-info",
           "-I", args.root.parent / "kernels",
           *(["-nostdlib"] if nostdlib else []), FIXTURES / "native.c", *options,
           "-o", shared)
        return shared

    policy = work / "trusted-native-policy"

    def select(name, shared, approved=True):
        package = replace(base, image=shared.read_bytes())
        root = put(work / name, package)
        if approved:
            policy.write_bytes(package.policy())
        return package, {PACKAGE_ROOTS: root, NATIVE_POLICY: str(policy)}

    valid_image = image("valid")
    valid, approved = select("native-valid", valid_image)
    result = ok(exe, **approved, **{PROVIDER_TRACE: "1"})
    assert result.stdout == EXPECTED_STDOUT, result
    assert f"module={MODULE} provider=native package_sha256={sha256(valid.wire()).hex()}" in (
        result.stderr), result
    rejected(exe, "absent native policy", r"native.*policy|policy.*native",
             **{PACKAGE_ROOTS: approved[PACKAGE_ROOTS]})
    policy.write_bytes(valid.policy().replace(sha256(valid.image).hex().encode("ascii"),
                                              b"0" * 64, 1))
    rejected(exe, "mismatched native policy", r"not approved", **approved)
    policy.write_bytes(valid.policy())

    corrupt = replace(valid, image=valid.image[:-1] + bytes([valid.image[-1] ^ 1]))
    corrupt_root = Path(put(work / "native-image-integrity"))
    # Flip only the actual image, leaving the manifest's digest untouched.
    (corrupt_root / (MODULE + ".on1xkp")).write_bytes(
        valid.wire()[:-len(valid.image)] + corrupt.image)
    rejected(exe, "native image integrity", r"malformed package",
             **{**approved, PACKAGE_ROOTS: str(corrupt_root)})

    profile = r"native ELF.*profile|qBFD rejected"
    _, env = select("native-missing-export", image("missing", "-DOMIT_SUB"))
    rejected(exe, "native missing export", profile + r"|native adapter.*defined", **env)
    _, env = select("native-has-init", image("init", "-DWITH_INIT", "-Wl,-init,negative_init"))
    rejected(exe, "native DT_INIT", profile, **env)
    _, env = select("native-has-dependency", image("dep", "-Wl,--no-as-needed", "-lc"))
    rejected(exe, "native DT_NEEDED", profile, **env)

    marked_image = image("constructor", "-DWITH_MARKER", nostdlib=False)
    marker = work / "constructor-marker"
    marker_env = {"KERNEL_LOAD_NEGATIVE_MARKER": str(marker)}
    # Control: the harmless fixture would leave a file if dlopen ran.
    ok(sys.executable, "-c", "import ctypes, sys; ctypes.CDLL(sys.argv[1])", marked_image,
       **marker_env)
    assert marker.is_file(), "constructor marker control did not run"
    marker.unlink()
    _, env = select("native-constructor", marked_image)
    rejected(exe, "native constructor", profile, **env, **marker_env)
    assert not marker.exists(), "constructor ran before validation"

    policy.write_bytes(valid.policy())
    return approved, valid


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("compiler", type=Path)
    parser.add_argument("root", type=Path)
    parser.add_argument("work", type=Path)
    parser.add_argument("cc")
    parser.add_argument("--cflag", action="append", default=[])
    parser.add_argument("--native", action="store_true")
    parser.add_argument("--import-library", type=Path, required=True)
    args = parser.parse_args()

    args.work.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=args.work.resolve(), prefix="run-") as temp:
        work = Path(temp)
        ENV["TMPDIR"] = temp
        runtime = args.root / "runtime"
        c_source, exe, probe = [work / name for name in ("main.c", "app", "rollback")]
        ok(args.compiler, "--emit-c", "--kernel-load-time", FIXTURES / "main.on1x",
           "-o", c_source)
        emitted = c_source.read_text()
        assert "ON1X_KERNEL_IMPORT_TABLE_ABI_VERSION != 1" in emitted
        assert "mulib_clamp_" not in emitted
        assert re.search(r"on1x_kernel_import_init\(.*?on1x_environ_init\(", emitted)
        build = [args.cc, "-std=c11", "-I", runtime, *args.cflag, c_source,
                 runtime / "on1x_runtime.c", args.root / "stdlib/environ/runtime.c",
                 FIXTURES / "startup.c", "-Wl,--wrap=on1x_environ_init"]
        link_flags = args.import_library.with_name("on1x_kernel_import.link").read_text().splitlines()
        libs = [args.import_library, *link_flags, "-lm"]
        ok(*build, *libs, "-o", exe)
        base = baseline()
        good_root = structured_rejections(exe, work, base)
        good_env = {PACKAGE_ROOTS: good_root}
        # The production binary must ignore test-only fault injection.
        assert ok(exe, **good_env, ON1X_KERNEL_IMPORT_FAIL_SLOT="1").stdout == EXPECTED_STDOUT

        rollback_env, rollback_package = {}, base
        if args.native:
            rollback_env, rollback_package = native_rejections(args, exe, work, base)
        dep = auxiliary(base, "stdffi.aux")
        rollback_env[PACKAGE_ROOTS] = put(
            work / "rollback-packages", replace(rollback_package, deps=((dep.module, 1),)), dep)
        ok(*build, "-DON1X_KERNEL_IMPORT_TESTING", runtime / "on1x_kernel_import.c",
           FIXTURES / "rollback.c", "-Wl,--wrap=on1x_kernel_import_init", *libs, "-o", probe)
        result = ok(probe, **rollback_env, **{TRACE: "1"})
        assert result.stdout == EXPECTED_STDOUT, result
        trace = re.findall(r"^on1x: kernel import trace (\S+) (\d+) (.+)$",
                           result.stderr, re.MULTILINE)
        acquire = [("acquire-recipe", "0", dep.module + "/-"), ("acquire-recipe", "1", MODULE + "/-")]
        release = [("release-recipe", "1", MODULE + "/-"), ("release-recipe", "0", dep.module + "/-")]
        if args.native:
            acquire.append(("acquire-native", "1", MODULE + "/-"))
            release.insert(0, ("release-native", "1", MODULE + "/-"))
        slots = [(slot, identity) for event, slot, identity in trace if event == "publish-slot"]
        assert [slot for slot, _ in slots] == ["0", "1", "2"], trace
        assert {identity for _, identity in slots} == {f"{MODULE}/{e.name}" for e in base.exports}
        assert "injected post-bind failure" in result.stderr, result
        assert trace == [
            *acquire, *(("bind-slot", *slot) for slot in slots[:2]),
            *(("unbind-slot", *slot) for slot in reversed(slots[:2])), *release,
            *acquire, *(("bind-slot", *slot) for slot in slots),
            *(("publish-slot", *slot) for slot in slots),
        ], trace
    print("late kernel import rejection and rollback: PASS")


if __name__ == "__main__":
    main()
