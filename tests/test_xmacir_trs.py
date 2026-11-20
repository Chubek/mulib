#!/usr/bin/env python3
"""Test the bundled XMacIR rules against independent scalar and byte oracles."""

from pathlib import Path
import operator
import random
import re
import shutil
import subprocess
import unittest


LIB = Path(__file__).resolve().parents[1] / "library" / "xmacir-trs"
M4 = shutil.which("m4")
MASK = (1 << 32) - 1
ENTRY = "example_value"
ABI = "capsule_entry,capsule_bridge,capsule_ctx,capsule_err"
SCALAR_PASSES = ("peephole", "strength", "commute")
PASSES = SCALAR_PASSES + ("repack",)
BINOPS = {
    "ADD": operator.add, "SUB": operator.sub, "MUL": operator.mul,
    "AND": operator.and_, "OR": operator.or_, "XOR": operator.xor,
}
LIMITS = (0, 1, 2, 32766, 32767, 32768, 65536, 99999, 100000,
          1 << 31, MASK - 1, MASK)


def scalar(*ops, entry=ENTRY):
    body = "".join(f"XM_PUSH({op})\n" if isinstance(op, int) else f"XM_{op}()\n"
                   for op in ops)
    return f"XM_U32({entry})\n{body}XM_RET()\n"


def evaluate(text, *, with_peak=False):
    lines = text.splitlines(keepends=True)
    if lines[0] != f"XM_U32({ENTRY})\n" or lines[-1] != "XM_RET()\n":
        raise ValueError("entry or terminal changed")
    stack, peak = [], 0
    for line in lines[1:-1]:
        match = re.fullmatch(r"XM_([A-Z]+)\((0|[1-9][0-9]*)?\)\n", line)
        if not match:
            raise ValueError(f"not a canonical record: {line!r}")
        op, literal = match.groups()
        if op == "PUSH" and literal is not None and int(literal) <= MASK:
            stack.append(int(literal))
        elif literal is not None:
            raise ValueError(f"bad argument: {line!r}")
        elif op in BINOPS:
            right, left = stack.pop(), stack.pop()
            stack.append(BINOPS[op](left, right) & MASK)
        elif op == "DROP":
            stack.pop()
        elif op == "DUP":
            stack.append(stack[-1])
        elif op == "SWAP":
            stack[-2], stack[-1] = stack[-1], stack[-2]
        else:
            raise ValueError(f"unknown operation: {line!r}")
        peak = max(peak, len(stack))
    value, = stack
    return (value, peak) if with_peak else value


def blob(*chunks, abi=ABI):
    return (f"XM_BLOB({abi})\n"
            + "".join(f"XM_BYTES(h{chunk.hex()})\n" for chunk in chunks)
            + "XM_END()\n")


def random_scalar(rng):
    ops, depth = [], 0
    for _ in range(96):
        choices = ["PUSH"]
        if depth:
            choices.extend(("DUP", "DROP"))
        if depth >= 2:
            choices.extend(("SWAP", *BINOPS))
        op = rng.choice(choices)
        if op == "PUSH":
            ops.append(rng.choice(LIMITS) if rng.randrange(3)
                       else rng.randrange(1 << rng.choice((15, 32))))
            depth += 1
        else:
            ops.append(op)
            if op == "DUP":
                depth += 1
            elif op != "SWAP":
                depth -= 1
    if not depth:
        ops.append(1)
    ops.extend(rng.choice(tuple(BINOPS)) for _ in range(depth - 1))
    return scalar(*ops)


@unittest.skipUnless(M4, "GNU m4 not found")
class RewriteTests(unittest.TestCase):
    def rewrite(self, name, source):
        proc = subprocess.run([M4, "-P", "--", str(LIB / f"{name}.xmtrs"), "-"],
                              input=source.encode("ascii"), capture_output=True, timeout=5)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(proc.stderr, b"")
        return proc.stdout.decode("ascii")

    def test_stack_and_identities(self):
        source = scalar(MASK, "DUP", "DROP", 3, "DROP", 9, "SWAP", "SWAP", "DROP",
                        0, "ADD", 0, "SUB", 1, "MUL", MASK, "AND", 0, "OR", 0, "XOR")
        self.assertEqual(self.rewrite("peephole", source), scalar(MASK))
        self.assertEqual(self.rewrite("repack", source), source)
        for ops in ((1, "DUP", "ADD"), (1, 2, "SWAP", "SUB"), (1, 2, "SWAP", "DROP")):
            with self.subTest(ops=ops):
                source = scalar(*ops)
                self.assertEqual(evaluate(self.rewrite("peephole", source)), evaluate(source))

    def test_exact_folding_and_bounds(self):
        for left, right in ((32767, 32767), (3, 7), (0, 32767), (0, 1)):
            for op, fn in BINOPS.items():
                with self.subTest(left=left, right=right, op=op):
                    self.assertEqual(self.rewrite("peephole", scalar(left, right, op)),
                                     scalar(fn(left, right) & MASK))
        for left, right, op in ((MASK, 1, "ADD"), (0, MASK, "SUB"),
                                (MASK, MASK, "MUL"), (65536, 65536, "MUL"),
                                (32768, 2, "MUL"), (2, 32768, "MUL"),
                                (99999, 2, "MUL"), (100000, 2, "MUL")):
            with self.subTest(left=left, right=right, op=op):
                source = scalar(left, right, op)
                self.assertEqual(self.rewrite("peephole", source), source)

    def test_strength_reduction(self):
        for value in (0, 1, 32768, 1 << 31, MASK - 1, MASK):
            for right, op, replacement in ((2, "MUL", ("DUP", "ADD")),
                                            (0, "MUL", ("DROP", 0)),
                                            (0, "AND", ("DROP", 0)),
                                            (MASK, "OR", ("DROP", MASK))):
                with self.subTest(value=value, right=right, op=op):
                    # The lower value must survive both the rewrite and the
                    # padding PUSH/DROP following the matched operation.
                    prefix, suffix = (5678, value), (2345, "DROP", "SUB")
                    source = scalar(*prefix, right, op, *suffix)
                    result = self.rewrite("strength", source)
                    self.assertEqual(result, scalar(*prefix, *replacement, *suffix))
                    self.assertEqual(evaluate(result), evaluate(source))
        for ops in ((0, MASK, "MUL"), (2, MASK, "MUL"), (MASK, 2147483647, "OR"),
                    (MASK, 1, "AND"), (MASK, 0, "SUB"), (1, 2, "DUP", "MUL", "ADD")):
            with self.subTest(ops=ops):
                source = scalar(*ops)
                self.assertEqual(self.rewrite("strength", source), source)

    def test_commuting_swaps(self):
        for op in BINOPS.keys() - {"SUB"}:
            with self.subTest(op=op):
                source = scalar(0, MASK - 5, "SWAP", op)
                result = self.rewrite("commute", source)
                self.assertEqual(result, scalar(0, MASK - 5, op))
                self.assertEqual(evaluate(result), evaluate(source))
        for source, expected in ((scalar(7, "DUP", "SWAP", "SWAP", "SUB"),
                                  scalar(7, "DUP", "SUB")),
                                 (scalar(2, 3, "SWAP", "SWAP", "SUB"),
                                  scalar(2, 3, "SUB")),
                                 (scalar(2, 3, "SWAP", "SUB"), scalar(2, 3, "SWAP", "SUB")),
                                 (scalar(2, 3, "SWAP", 5, "ADD", "SUB"),
                                  scalar(2, 3, "SWAP", 5, "ADD", "SUB"))):
            with self.subTest(source=source):
                result = self.rewrite("commute", source)
                self.assertEqual(result, expected)
                self.assertEqual(evaluate(result), evaluate(source))

    def test_random_scalar(self):
        rng = random.Random(82055)
        for case in range(120):
            source = random_scalar(rng)
            expected, input_peak = evaluate(source, with_peak=True)
            for name in SCALAR_PASSES:
                with self.subTest(case=case, name=name):
                    result = self.rewrite(name, source)
                    value, peak = evaluate(result, with_peak=True)
                    self.assertEqual(value, expected)
                    self.assertLessEqual(peak, input_peak)
                    self.assertLessEqual(result.count("\n"), source.count("\n"))

    def test_blob_repacking(self):
        rng = random.Random(39550)
        for length in (0, 1, 31, 32, 33, 63, 64, 65, 256, 1024):
            with self.subTest(length=length):
                payload = bytes(i & 255 for i in range(length))
                chunks, pos = [b""], 0
                while pos < length:
                    count = rng.randint(1, 32)
                    chunks.extend((payload[pos:pos + count], b""))
                    pos += count
                self.check_blob(chunks, payload)
        # No C-string truncation or padding: keep leading, internal and
        # trailing zeros even when the trailing row consists only of zeros.
        center = b"\xaa\x00\xbb"
        self.check_blob([bytes(31), center, bytes(32)], bytes(31) + center + bytes(32))

    def check_blob(self, chunks, payload):
        source = blob(*chunks)
        packed = blob(*(payload[i:i + 32] for i in range(0, len(payload), 32)))
        for name in SCALAR_PASSES:
            self.assertEqual(self.rewrite(name, source), source)
        self.assertEqual(self.rewrite("repack", source), packed)
        self.assertEqual(self.rewrite("repack", packed), packed)

    def test_namespace(self):
        # Valid identifiers near the reserved m4_ and XM_ namespaces.
        for name in PASSES:
            script = (LIB / f"{name}.xmtrs").read_text(encoding="ascii")
            definitions = re.findall(r"\bm4_(?:define|pushdef)\(`([^']+)'", script)
            self.assertTrue(definitions)
            self.assertTrue(all(symbol.startswith("XM_") for symbol in definitions))
            for entry in ("XM", "m4", "XMx_RET", "m4x_eval", "m4Xeval", "XMX_PUSH"):
                with self.subTest(name=name, entry=entry):
                    source = scalar(7, entry=entry)
                    self.assertEqual(self.rewrite(name, source), source)
            source = blob(b"\x00\xff", abi="m4x_func,XMx_Bridge,XM,m4")
            self.assertEqual(self.rewrite(name, source), source)
            # This processor-level control tests -P independently: the
            # actual XMacIR parser additionally disallows bare builtin names.
            source = blob(b"\x00\xff", abi="divert,ifelse,dnl,changequote")
            self.assertEqual(self.rewrite(name, source), source)

    def test_chaining(self):
        source = scalar(3, 7, "SWAP", "ADD", 2, "MUL", MASK, "OR")
        expected = evaluate(source)
        for name in ("commute", "strength", "peephole", "repack"):
            source = self.rewrite(name, source)
            self.assertEqual(evaluate(source), expected)
        self.assertEqual(source, scalar(10, "DUP", "ADD", "DROP", MASK))


if __name__ == "__main__":
    unittest.main()
