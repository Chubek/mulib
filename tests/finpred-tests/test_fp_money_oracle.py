"""Differential cases using Python integers, never floating-point prices."""

import itertools
import random
import subprocess
import sys

MINOR_MIN = -(2**63)
MINOR_MAX = 2**63 - 1


def round_ratio(numerator, denominator):
    """Nearest minor unit; ties away from zero."""
    q, r = divmod(abs(numerator), denominator)
    result = q + int(2 * r >= denominator)
    return -result if numerator < 0 else result


def expected(op, x, n, d):
    if op == "s" and x >= 0 and n > 0 and d > 0:
        result, remainder = divmod(x * n, d)
        if remainder:
            return None
    elif op == "p" and x >= 0 and n > 0 and d > 0:
        result = round_ratio(x * d, n)
    elif op == "f" and n > 0 and d > 0:
        result = round_ratio(x * n, d)
    elif op == "d" and x >= 0 and n >= 0:
        result = x * n
    elif op == "o" and x > 0 and n > 0 and d >= 0:
        result = x * n
        if result > d:
            return None
    else:
        return None
    return result if MINOR_MIN <= result <= MINOR_MAX else None


def main():
    edges = [MINOR_MIN, MINOR_MIN + 1, -3, -1, 0, 1, 2, 3, 9999, 10000,
             2**53 - 1, 2**53, 2**53 + 1, MINOR_MAX - 2, MINOR_MAX - 1, MINOR_MAX]
    ratios = [-1, 0, 1, 2, 3, 5000, 10000, 2**31 - 1, MINOR_MAX - 1, MINOR_MAX]
    cases = list(itertools.product("spfdo", edges, ratios, ratios))
    rng = random.Random(0xF170)
    for _ in range(3000):
        positive = rng.randrange(1, MINOR_MAX)
        # Large operands with small denominators and near-one ratios exercise
        # overflow checks even when the exact product does not fit int64.
        n = rng.choice([rng.randrange(1, 200), positive, positive + 1])
        d = rng.choice([rng.randrange(1, 200), positive, positive + 1])
        x = rng.choice([rng.randrange(-1000, 1001), rng.randrange(MINOR_MIN, MINOR_MAX + 1)])
        cases.append((rng.choice("spfdo"), x, n, d))
    stdin = "".join(f"{op} {x} {n} {d}\n" for op, x, n, d in cases)
    output = subprocess.run([sys.argv[1]], input=stdin, capture_output=True, text=True, check=True)
    lines = output.stdout.splitlines()
    assert len(lines) == len(cases), (len(lines), len(cases))
    for case, line in zip(cases, lines):
        result = expected(*case)
        want = (0, -12345) if result is None else (1, result)
        got = tuple(map(int, line.split()))
        assert got == want, (case, got, want)
    print(f"FinPRED Python Money differential passed: {len(cases)} cases")


if __name__ == "__main__":
    main()
