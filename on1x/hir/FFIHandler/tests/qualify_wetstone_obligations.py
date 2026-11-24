#!/usr/bin/env python3
"""Qualify the numeric helper with real Cephyr 6a7d52e8 and Wetstone."""
import argparse
import importlib.util
from itertools import combinations_with_replacement, product
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from ffi_obligations import ArrayCheck, MAX_QUERIES, RangeCheck, closed_return_check, run_obligations
from project_interface import CephyrProvider, ProjectionError, TARGET_ABIS


REVISION = "6a7d52e866db8eb441bb7a4f86ce91253732e740"
HEADER = b"wetstone-ffi-obligations 1\n"
SOURCE = b"""\
extern unsigned long wide(void);
extern long signed_wide(void);
extern char plain(void);
extern void rows(int items[static 4][3]);
extern void bounds(unsigned char items[static 3]);
extern void bounds(unsigned char items[static 8]);
extern void dynamic(unsigned int n, int items[static n]);
unsigned int mask8(unsigned int x) { return x & 255u; }
unsigned int mask16(unsigned int x) { return x & 65535u; }
unsigned int unmodeled(unsigned int x) { return x + 1u; }
unsigned int local_table(unsigned int x) {
    const unsigned int data[2] = {5u, 20u};
    return data[x & 1u];
}
"""


def upstream_parser(source):
    spec = importlib.util.spec_from_file_location(
        "cephyr_c11_interface", source / "tests/test_c11_interface.py")
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def expect_error(code, operation):
    try:
        operation()
    except ProjectionError as exc:
        assert exc.code == code, (code, exc.code)
    else:
        raise AssertionError(f"expected {code}")


def qualify_target(upstream, provider, helper):
    projection = provider.project(SOURCE)
    doc = upstream.parse(projection.output)
    assert doc[:2] == ["interface", 1]
    assert upstream.one(doc, "analysis-model")[1] == 1
    assert upstream.value(doc, "target") == provider.target

    # Map only fields needed by the qualification. Production callers pass
    # normalized values from the model decoder, not these raw list operations.
    def function(name):
        return upstream.named(doc, "function", name)

    def result_range(name):
        typ = upstream.value(function(name), "type")
        return tuple(upstream.one(upstream.value(typ, "result"), "value-range")[1:])

    def sites(name):
        fn = function(name)
        params = upstream.one(upstream.value(fn, "type"), "parameters")
        for site in upstream.forms(upstream.one(fn, "declarations"), "site"):
            for param in upstream.forms(site, "parameter"):
                if upstream.forms(param, "requires-array-access"):
                    size = upstream.value(upstream.value(params[param[1] + 1], "pointee"), "size")
                    yield upstream.value(param, "requires-array-access"), size

    def narrow(name, accepted):
        fn = function(name)
        memory = upstream.one(fn, "memory-safety")
        bodies = upstream.forms(fn, "body-proof")
        widths = None
        if bodies:
            body, = bodies
            for proof in (memory, body):
                assert upstream.value(proof, "model") == "closed-unsigned-expression-v2"
                assert upstream.value(proof, "scope") == "this-definition"
                assert upstream.value(proof, "solver") == "satie"
            widths = {width: status for _, width, status in
                      upstream.forms(upstream.one(body, "return-policies"), "unsigned-width")}
        return closed_return_check(result_range(name), accepted,
                                   memory_status=upstream.value(memory, "status"),
                                   width_statuses=widths)

    smin, smax = -(1 << 63), (1 << 63) - 1
    umax = (1 << 64) - 1
    assert result_range("wide") == (0, umax) and result_range("signed_wide") == (smin, smax)
    assert list(sites("rows")) == [(4, 12)]  # stride of int[3], not sizeof(int)
    assert list(sites("bounds")) == [(3, 1), (8, 1)]  # do not merge redeclaration sites
    assert list(sites("dynamic")) == [("unknown", 4)]
    row, = sites("rows")
    queries = (
        RangeCheck(result_range("wide"), (smin, umax)),
        RangeCheck(result_range("wide"), (smin, smax)),
        RangeCheck(result_range("signed_wide"), (0, umax)),
        RangeCheck(result_range("plain"), (0, 255)),
        RangeCheck(result_range("mask8"), (0, 300)),
        narrow("mask8", (0, 300)),  # not one of the source solver's width policies
        narrow("mask8", (0, 100)),
        narrow("mask16", (0, 65535)),
        narrow("unmodeled", (0, 65535)),
        ArrayCheck(*row, (48, 48)),
        ArrayCheck(*row, (47, 47)),
        ArrayCheck(*row, (47, 48)),
        *(ArrayCheck(*site, (4, 4)) for site in sites("bounds")),
        None,  # VLA minimum is unknown, never interpreted as zero
        ArrayCheck(umax, 2, (0, umax)),  # mathematical minimum exceeds size_t
        ArrayCheck(umax, 1, (umax, umax)),
        narrow("local_table", (0, 300)),
    )
    assert queries[8] is None
    assert upstream.value(upstream.one(function("local_table"), "memory-safety"), "checked-accesses") == 1
    expected_plain = "unknown" if provider.target == "x86_64-linux-gnu" else "proved"
    assert run_obligations(provider, helper, queries) == (
        "proved", "unknown", "unknown", expected_plain,
        "unknown", "proved", "unknown", "proved", "unknown",
        "proved", "disproved", "unknown", "proved", "disproved", "unknown",
        "disproved", "proved", "proved",
    )
    provider.verify_inputs()
    print(f"{provider.target}: checked {len(queries)} numeric obligations/unknown slots")


def check_transport(helper):
    def invoke(payload):
        return subprocess.run((str(helper),), input=payload, capture_output=True, timeout=10)

    row = b"array 1 1 1 1\n"
    result = invoke(HEADER + row * MAX_QUERIES)
    assert result.returncode == 0 and result.stdout == HEADER + b"proved\n" * MAX_QUERIES
    for payload in (
        b"", b"wetstone-ffi-obligations 2\n" + row, HEADER + row * (MAX_QUERIES + 1),
        HEADER + row[:-1], HEADER + b"range 5 1 0 6\n", HEADER + b"array 1 0 1 1\n",
        HEADER + b"array 1 1 2 1\n", HEADER + b"array 1 1 -1 5\n",
        HEADER + b"array 1 1 0 18446744073709551616\n",
        HEADER + b"range -9223372036854775809 7 0 6\n",
        HEADER + b"range 0 1 0 1\x00\n", HEADER + b"array " + b" " * 96 + b"\n",
    ):
        assert invoke(payload).returncode == 2, payload[:96]
    print("numeric helper protocol bounds and malformed rows checked")


def check_arithmetic(provider, helper):
    queries, expected = [], []
    intervals = tuple(combinations_with_replacement(range(-2, 3), 2))
    for value_range, accepted_range in product(intervals, repeat=2):
        queries.append(RangeCheck(value_range, accepted_range))
        accepted = range(accepted_range[0], accepted_range[1] + 1)
        expected.append("proved" if all(value in accepted for value in
                                        range(value_range[0], value_range[1] + 1)) else "unknown")
    for count, stride, bounds in product(range(1, 4), range(1, 4),
                                         combinations_with_replacement(range(8), 2)):
        queries.append(ArrayCheck(count, stride, bounds))
        outcomes = {capacity >= count * stride for capacity in range(bounds[0], bounds[1] + 1)}
        expected.append({frozenset((True,)): "proved", frozenset((False,)): "disproved"}
                        .get(frozenset(outcomes), "unknown"))
    assert run_obligations(provider, helper, queries) == tuple(expected)
    print(f"{len(queries)} small-interval queries agree with enumerated integer results")


def check_policy_statuses():
    declared, accepted = (0, (1 << 32) - 1), (0, 65535)
    widths = {8: "unknown", 16: "proved", 31: "unknown"}
    for status in ("disproved", "unknown", None):
        assert closed_return_check(declared, accepted, memory_status=status, width_statuses=widths) is None
    assert closed_return_check(declared, accepted, memory_status="proved",
                               width_statuses={8: "disproved", 16: "unknown", 31: "unknown"}) is None
    for malformed in ({8: "proved", 16: "disproved", 31: "unknown"}, {8: "proved"}, {**widths, 8: "yes"}):
        try:
            closed_return_check(declared, accepted, memory_status="proved", width_statuses=malformed)
        except ValueError:
            pass
        else:
            raise AssertionError(f"accepted malformed policies: {malformed}")


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--cephyr", type=Path, required=True)
    cli.add_argument("--libstone-root", type=Path, required=True)
    cli.add_argument("--helper", type=Path, required=True)
    cli.add_argument("--work", type=Path, required=True)
    args = cli.parse_args()
    libstone = args.libstone_root.resolve(strict=True)
    assert subprocess.check_output(("git", "-C", libstone, "rev-parse", "HEAD"),
                                   text=True).strip() == REVISION
    cephyr, helper = (path.resolve(strict=True) for path in (args.cephyr, args.helper))
    source = libstone / "cornerstone/Cephyr"
    upstream = upstream_parser(source)
    tool_inputs = (cephyr.parent / "libcephyr.so.0.1.0", source / "triples",
                   libstone / "cornerstone/Archon/stdspec")
    with tempfile.TemporaryDirectory(dir=args.work.resolve(strict=True)) as scratch:
        for target in TARGET_ABIS:
            provider = CephyrProvider(cephyr, target, libstone_revision=REVISION,
                                      scratch_root=scratch, tool_inputs=(*tool_inputs, helper))
            qualify_target(upstream, provider, helper)
        unselected = CephyrProvider(cephyr, next(iter(TARGET_ABIS)), libstone_revision=REVISION,
                                    scratch_root=scratch, tool_inputs=tool_inputs)
        expect_error("wetstone_input", lambda: run_obligations(
            unselected, helper, (RangeCheck((0, 1), (0, 1)),)))
        expect_error("wetstone_limit", lambda: run_obligations(
            provider, helper, iter([None] * (MAX_QUERIES + 1))))
        check_arithmetic(provider, helper)
    check_policy_statuses()
    check_transport(helper)


if __name__ == "__main__":
    main()
