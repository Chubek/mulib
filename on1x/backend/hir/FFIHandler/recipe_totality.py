"""Prove the bounded recipe subset's signed arithmetic defined on all C paths."""

from math import gcd

from generate_portable_recipe import MAX_I64, MIN_I64, require


def combine(a, b, factor=1):
    return tuple(x + factor * y for x, y in zip(a, b))


def negate(a, strict=False):
    # Integers: the complement of a <= 0 is -a + 1 <= 0.
    return (*(-x for x in a[:-1]), -a[-1] + strict)


def simplify(facts):
    """Represent each constraint as an integer affine form <= 0."""
    strongest = {}
    for *coefficients, constant in facts:
        divisor = gcd(*coefficients)
        if divisor == 0:
            if constant > 0:
                return None
            continue
        key = tuple(x // divisor for x in coefficients)
        constant = -(-constant // divisor)  # ceil, exact for integer assignments
        strongest[key] = max(constant, strongest.get(key, constant))
    return [(*key, constant) for key, constant in strongest.items()]


def impossible(facts, budget):
    """Bounded Fourier–Motzkin: only contradictions are evidence.

    Dropping an eliminated variable, or failing to find a contradiction, can
    never turn a satisfiable system into a proof of safety. Limit growth and
    reject rather than relying on unchecked integer arithmetic or a heuristic.
    """
    remaining = simplify(facts)
    while remaining:
        def cost(i):
            positive = sum(row[i] > 0 for row in remaining)
            negative = sum(row[i] < 0 for row in remaining)
            return (positive * negative, -(positive + negative))

        variables = {i for row in remaining for i, value in enumerate(row[:-1]) if value}
        if not variables:
            break
        i = min(sorted(variables), key=cost)
        positive, negative, zero = [], [], []
        for row in remaining:
            (positive if row[i] > 0 else negative if row[i] < 0 else zero).append(row)
        pairs = len(positive) * len(negative)
        budget[0] -= len(remaining) + pairs
        require(budget[0] >= 0 and pairs + len(zero) <= 2048, "totality proof exceeds solver budget")
        for a in positive:
            for b in negative:
                divisor = gcd(a[i], b[i])
                p, q = -b[i] // divisor, a[i] // divisor
                row = tuple(p * x + q * y for x, y in zip(a, b))
                require(all(value.bit_length() <= 256 for value in row), "totality proof coefficients too large")
                zero.append(row)
        remaining = simplify(zero)
    return remaining is None


class Totality:
    def __init__(self, arity):
        self.zero = (0,) * (arity + 1)
        self.params = {f"${i}": self.zero[:i] + (1,) + self.zero[i + 1:] for i in range(arity)}
        self.input_bounds = tuple(bound for a in self.params.values()
                                  for bound in (self.shift(a, -MAX_I64), self.shift(negate(a), MIN_I64)))
        self.budget = [30_000]
        self.paths = 4096

    @staticmethod
    def shift(a, amount):
        return (*a[:-1], a[-1] + amount)

    def visit(self, states):
        self.paths -= len(states)
        require(self.paths >= 0, "totality proof exceeds path budget")

    def integer(self, expr, states):
        self.visit(states)
        if type(expr) is int:
            require(MIN_I64 <= expr <= MAX_I64, "invalid signed i64 literal")
            return self.shift(self.zero, expr)
        if isinstance(expr, str):
            require(expr in self.params, "unknown signed i64 parameter")
            return self.params[expr]
        require(isinstance(expr, (list, tuple)) and len(expr) == 3 and expr[0] in ("add", "sub"),
                "invalid arithmetic expression")
        op, left, right = expr
        a, b = self.integer(left, states), self.integer(right, states)
        value = combine(a, b, 1 if op == "add" else -1)
        bounds = (self.shift(value, -MAX_I64), self.shift(negate(value), MIN_I64))
        for state in states:
            for valid in bounds:
                require(impossible((*self.input_bounds, *state, negate(valid, strict=True)), self.budget),
                        f"cannot prove signed i64 {op} is defined on every reachable path")
        return value

    def boolean(self, expr, states):
        self.visit(states)
        if expr == "true":
            return states, []
        if expr == "false":
            return [], states
        require(isinstance(expr, (list, tuple)) and len(expr) == 3
                and expr[0] in ("and", "or", "lt", "le", "gt", "ge", "eq"),
                "invalid boolean expression")
        op, left, right = expr
        if op in ("and", "or"):
            yes, no = self.boolean(left, states)
            if op == "and":
                next_yes, next_no = self.boolean(right, yes)
                return next_yes, no + next_no
            next_yes, next_no = self.boolean(right, no)
            return yes + next_yes, next_no
        a, b = self.integer(left, states), self.integer(right, states)
        delta = combine(a, b, -1)
        if op == "eq":
            negative = negate(delta)
            return ([(*state, delta, negative) for state in states],
                    [(*state, fact) for state in states for fact in
                     (self.shift(delta, 1), self.shift(negative, 1))])
        if op in ("gt", "ge"):
            delta = negate(delta)
        if op in ("lt", "gt"):
            delta = self.shift(delta, 1)
        return ([(*state, delta) for state in states],
                [(*state, negate(delta, strict=True)) for state in states])

    def result(self, expr, states):
        self.visit(states)
        if isinstance(expr, (list, tuple)) and len(expr) == 4 and expr[0] == "if":
            _, condition, yes, no = expr
            true_states, false_states = self.boolean(condition, states)
            self.result(yes, true_states)
            self.result(no, false_states)
        else:
            self.integer(expr, states)


def prove_totality(body, arity):
    """A successful proof covers all i64 inputs, not a guessed API guard."""
    Totality(arity).result(body, [()])
