#!/usr/bin/env python3
"""Independent rational 2D vertex oracle for weighted-L1 Cassowary constraints."""
import argparse
from fractions import Fraction
from itertools import combinations
import random
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--solver', required=True)
    args = parser.parse_args()
    rng = random.Random(77019)
    for case in range(100):
        # coefficient x,y, relation(EQ=0,LE=1,GE=2), rhs, strength(0=hard)
        cs = [(1, 0, 1, 10, 0), (1, 0, 2, -10, 0),
              (0, 1, 1, 10, 0), (0, 1, 2, -10, 0)]
        if case % 4 == 0:
            cs.append((1, -1, 0, 0, 0))
        for _ in range(8):
            a, b = rng.randint(-3, 3), rng.randint(-3, 3)
            if a == b == 0:
                a = 1
            cs.append((a, b, rng.randrange(3), rng.randint(-15, 15), rng.choice([1, 3, 1000])))

        def cost(x, y):
            total = 0
            for a, b, relation, rhs, weight in cs:
                error = a * x + b * y - rhs
                violation = abs(error) if relation == 0 else max(0, error if relation == 1 else -error)
                if not weight and violation:
                    return None
                total += weight * violation
            return total

        candidates = []
        for first, second in combinations(cs, 2):
            a, b, _, rhs, _ = first
            c, d, _, target, _ = second
            determinant = a * d - b * c
            if determinant:
                x = Fraction(rhs * d - b * target, determinant)
                y = Fraction(a * target - rhs * c, determinant)
                value = cost(x, y)
                if value is not None:
                    candidates.append(value)
        expected = min(candidates)
        text = f'2 {len(cs)}\n' + '\n'.join(' '.join(map(str, c)) for c in cs) + '\n'
        output = subprocess.check_output([args.solver, '--oracle'], input=text, text=True).split()
        assert output[0] == '0', (case, output, text)
        x, y = map(float, output[1:])
        # Tolerant feasibility separately; don't test floating residual == zero.
        actual = 0.0
        for a, b, relation, rhs, weight in cs:
            error = a * x + b * y - rhs
            violation = abs(error) if relation == 0 else max(0, error if relation == 1 else -error)
            if not weight:
                assert violation < 1e-6, (case, violation)
            actual += weight * violation
        assert abs(actual - float(expected)) < 1e-5 * max(1, float(expected)), (case, actual, expected, x, y)
    print('Cassowary: 100 rational weighted-L1 vertex reference cases passed')


if __name__ == '__main__':
    main()
