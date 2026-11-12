#!/usr/bin/env python3
"""Compare full-width money scaling with Python's exact integer arithmetic."""
import argparse
from pathlib import Path
import random
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = r'''
#define MULIB_IMPL_MONEY
#include "mulib-money.h"
#include <inttypes.h>
int main(void) {
  int64_t value, numerator, denominator;
  while (scanf("%" SCNd64 " %" SCNd64 " %" SCNd64,
               &value, &numerator, &denominator) == 3) {
    mulib_money_t input, output;
    if (!mulib_money_make(&input, value, "USD", 2)) return 1;
    output = input;
    int ok = mulib_money_scale(input, numerator, denominator, &output);
    printf("%d %" PRId64 "\n", ok, output.minor);
  }
  return ferror(stdin) || ferror(stdout);
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default='cc')
    args = parser.parse_args()
    maximum = (1 << 63) - 1
    minimum = -(1 << 63)
    values = [minimum, minimum + 1, -maximum // 2, -1, 0, 1,
              maximum // 2, maximum - 1, maximum]
    factors = [0, 1, 2, 3, maximum // 2, maximum - 1, maximum]
    cases = [(v, n, d) for v in values for n in factors for d in factors if d]
    rng = random.Random(0x4D754C4942)
    cases += [(rng.randint(minimum, maximum), rng.randint(0, maximum),
               rng.randint(1, maximum)) for _ in range(10000)]
    parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
    with tempfile.TemporaryDirectory(prefix='money-reference-', dir=parent) as tmp:
        source, binary = Path(tmp) / 'reference.c', Path(tmp) / 'reference'
        source.write_text(SOURCE)
        subprocess.run(shlex.split(args.cc) + ['-std=c11', '-Wall', '-Wextra',
                       '-Wpedantic', '-Werror', '-I' + str(ROOT / 'include'),
                       str(source), '-lm', '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)], input=''.join(
            f'{v} {n} {d}\n' for v, n, d in cases), capture_output=True,
            text=True, check=True, timeout=30)
    lines = result.stdout.splitlines()
    assert len(lines) == len(cases)
    for case, line in zip(cases, lines):
        value, numerator, denominator = case
        quotient, remainder = divmod(abs(value) * numerator, denominator)
        rounded = quotient + (2 * remainder >= denominator)
        expected = -rounded if value < 0 else rounded
        valid = minimum <= expected <= maximum
        ok, actual = map(int, line.split())
        assert bool(ok) == valid, (case, line, expected)
        assert actual == (expected if valid else value), (case, line, expected)
    print(f'Money exact integer reference: {len(cases)} cases passed')


if __name__ == '__main__':
    main()
