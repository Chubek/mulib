#!/usr/bin/env python3
"""Compile the generated executable ffispec index into immutable tables."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys
import tempfile

MAX_FILE = 65536
MAX_INDEX = 8 * 1024 * 1024
MAX_RECIPES = 1024
MAX_EXPRESSION_NODES = 512
MAX_PROOF_STEPS = 8192
MAX_TABLE_EXPORTS = 4096
MAX_TABLE_NODES = 131072
MAX_TABLE_DIRECTIVES = 32768
MIN_I64, MAX_I64 = -(1 << 63), (1 << 63) - 1
KERNEL = re.compile(r"(?=.{1,128}\Z)[a-z][a-z0-9]*(?:-[a-z0-9]+)*")
IDENT = re.compile(r"[a-zA-Z_][a-zA-Z_0-9]{0,63}")
SELECTOR = re.compile(r"[a-zA-Z_0-9][a-zA-Z_0-9.-]{0,127}")
INTEGER = re.compile(r"-?(?:0|[1-9][0-9]{0,18})")
DIGEST = re.compile(r"[0-9a-f]{64}")
OPS = {
    "lt": ("Lt", "bool", "i64", "i64"),
    "le": ("Le", "bool", "i64", "i64"),
    "gt": ("Gt", "bool", "i64", "i64"),
    "ge": ("Ge", "bool", "i64", "i64"),
    "eq": ("Eq", "bool", "i64", "i64"),
    "and": ("And", "bool", "bool", "bool"),
    "or": ("Or", "bool", "bool", "bool"),
    "add": ("Add", "i64", "i64", "i64"),
    "sub": ("Sub", "i64", "i64", "i64"),
    "if": ("If", "i64", "bool", "i64", "i64"),
}
# Action -> required effective backend, or None for portable selectors.
DIRECTIVES = {
    "hint": {
        "kernel-hot": None,
        "canonicalized": None,
        "local-memory": None,
        "private-code": None,
        "cold-code": "mir-c11",
        "no-inline": "mir-c11",
    },
    "rewrite": {
        "fold-identity": None,
        "memgraph-local": None,
        "meta-canonical": None,
    },
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read(path, limit=MAX_FILE):
    require(path.is_file() and not path.is_symlink(), f"not a regular file: {path}")
    with path.open("rb") as stream:
        data = stream.read(limit + 1)
    require(len(data) <= limit and b"\0" not in data, f"invalid input size or NUL: {path}")
    return data


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def unique_fields(pairs):
    result = {}
    for name, value in pairs:
        require(name not in result, f"duplicate JSON field {name!r}")
        result[name] = value
    return result


def invalid_constant(value):
    raise ValueError(f"invalid JSON constant {value!r}")


def generated_recipes(index_path):
    """Only the generator's executable manifest is a build input; ignore catalogues."""
    index = json.loads(read(index_path, MAX_INDEX).decode("utf-8"),
                       object_pairs_hook=unique_fields, parse_constant=invalid_constant)
    require(isinstance(index, dict) and type(index.get("version")) is int and index["version"] == 1,
            "expected generated recipe index version 1")
    records = index.get("recipes")
    require(isinstance(records, list) and len(records) <= MAX_RECIPES, "expected bounded recipes array")
    root = index_path.parent.resolve(strict=True)
    result = {}
    for record in records:
        require(isinstance(record, dict), "expected generated recipe object")
        name, identity = record.get("file"), record.get("header")
        require(isinstance(name, str) and isinstance(identity, str), "expected recipe file and header identity")
        match = re.fullmatch(r"recipes/([a-z][a-z0-9-]{0,127})\.ffi", name)
        require(match is not None and KERNEL.fullmatch(match[1]), f"invalid recipe path: {name!r}")
        kernel = match[1]
        require(kernel not in result, f"duplicate generated recipe for {kernel}")
        path = root / name
        require(path.resolve(strict=True) == path, f"not a canonical recipe path: {path}")
        header = Path(identity)
        require(header.is_absolute() and str(header) == identity and header.name == f"mulib-{kernel}.h"
                and header.resolve(strict=True) == header, f"not a canonical header path for {kernel}: {identity!r}")
        digest = sha256(read(header, MAX_INDEX))
        if "sha256" in record:
            require(record["sha256"] == digest, f"header SHA-256 mismatch: {header}")
        result[kernel] = (path, header.name, digest)
    return sorted(result.items())


def bound_document(text):
    """Use the same document limits as the runtime structural matcher, before LR parsing."""
    nodes, depth = -1, 0  # The runtime counts all nodes except the outer list.
    for token in re.finditer(r"[;#][^\r\n]*|[()]|[^ \t\r\n();#]+", text):
        first = token[0][0]
        if first == "(":
            depth += 1
        elif first == ")":
            depth -= 1
            continue
        elif first in ";#":
            continue
        nodes += 1
        require(nodes <= 16384 and depth <= 41, "FFI recipe exceeds document depth or node limit")


def load_parser(path):
    spec = importlib.util.spec_from_file_location("ffihandler_llsynth", path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def write_if_changed(output, content):
    try:
        with output.open("rb") as stream:
            if stream.read(len(content) + 1) == content:
                return
    except FileNotFoundError:
        pass
    with tempfile.NamedTemporaryFile(dir=output.parent, delete=False) as stream:
        temporary = Path(stream.name)
        try:
            stream.write(content)
            stream.close()
            temporary.replace(output)
        finally:
            temporary.unlink(missing_ok=True)


def list_items(tail):
    """Traverse a generated left-recursive list without Python recursion."""
    items = []
    while tail.children:
        tail, child = tail.children
        require(len(items) < 128, "FFI list exceeds item limit")
        items.append(child)
    return reversed(items)


def spelling(node):
    return node.token.lexeme


def expression_tree(node, depth=0, budget=None):
    if budget is None:
        budget = [MAX_EXPRESSION_NODES]
    require(depth <= 32 and budget[0], "FFI recipe exceeds expression depth or node limit")
    budget[0] -= 1
    if len(node.children) == 1:
        return spelling(node.children[0])
    _, op, *operands, _ = node.children
    return [spelling(op), *(expression_tree(child, depth + 1, budget) for child in operands)]


def field_tree(choice):
    node, = choice.children
    _, label, *values, _ = node.children
    tag = spelling(label)
    if node.symbol == "Export":
        member, symbol, options = values
        return [tag, spelling(member), spelling(symbol), *map(field_tree, list_items(options))]
    if node.symbol in {"Guard", "Body"}:
        return [tag, expression_tree(values[0])]
    if node.symbol == "Params":
        return [tag, *map(spelling, list_items(values[0]))]
    if node.symbol == "Directive":
        action, options = values
        return [tag, spelling(action), *map(field_tree, list_items(options))]
    return [tag, *map(spelling, values)]


def form(value):
    require(isinstance(value, list) and value and isinstance(value[0], str), "expected named FFI form")
    return value[0], value[1:]


def fields(values, single, repeated=()):
    result = {}
    for value in values:
        key, args = form(value)
        if key in repeated:
            result.setdefault(key, []).append(args)
        else:
            require(key in single and key not in result, f"unknown or duplicate field: {key!r}")
            result[key] = args
    return result


def atom(items, what, pattern=None):
    require(len(items) == 1 and isinstance(items[0], str)
            and (pattern is None or pattern.fullmatch(items[0])), f"invalid {what}")
    return items[0]


def selectors(values, parent):
    output = parent.copy()
    for key in ("backend", "target"):
        if key not in values:
            continue
        word = atom(values[key], key)
        require(word == "*" or SELECTOR.fullmatch(word), f"invalid {key} selector")
        output[key] = "" if word == "*" else word
    return output


def linear(left, right, sign):
    terms = dict(left[0])
    for index, coefficient in right[0]:
        terms[index] = terms.get(index, 0) + sign * coefficient
    return tuple(sorted((i, n) for i, n in terms.items() if n)), left[1] + sign * right[1]


class I64Proof:
    """Conservative, path-sensitive bounds for the typed, pure recipe subset.

    Facts bound individual parameters and normalized linear forms. For a
    single-parameter unit form, an exact equality permits looking up a direct
    peer's interval. Otherwise we do not combine different forms: lack of a
    proof is rejection, never implicit C wrapping, On1x trapping, or speculation.
    """
    def __init__(self):
        self.remaining = MAX_PROOF_STEPS

    def visit(self):
        self.remaining -= 1
        require(self.remaining >= 0, "FFI overflow proof exceeds work limit")

    @staticmethod
    def orientation(terms):
        if terms and terms[0][1] < 0:
            return tuple((i, -n) for i, n in terms), -1
        return terms, 1

    def bounds(self, affine, facts):
        terms, offset = affine
        lo = hi = 0
        for index, coefficient in terms:
            a, b = facts.get(((index, 1),), (MIN_I64, MAX_I64))
            if coefficient < 0:
                a, b = b, a
            lo += a * coefficient
            hi += b * coefficient
        key, sign = self.orientation(terms)
        if key in facts:
            lower, upper = facts[key]
            if sign < 0:
                lower, upper = -upper, -lower
            lo, hi = max(lo, lower), min(hi, upper)
        return lo + offset, hi + offset

    def equality_peer_bounds(self, affine, facts, lo, hi):
        terms, offset = affine
        if len(terms) != 1 or abs(terms[0][1]) != 1:
            return lo, hi
        ((index, sign),) = terms
        for pair, interval in facts.items():
            self.visit()
            if interval != (0, 0) or len(pair) != 2:
                continue
            (a, ca), (b, cb) = pair
            if (ca, cb) != (1, -1) or index not in (a, b):
                continue
            # Only x-y == 0 on this path, and only the peer's own interval.
            peer = b if index == a else a
            peer_lo, peer_hi = facts.get(((peer, 1),), (MIN_I64, MAX_I64))
            if sign < 0:
                peer_lo, peer_hi = -peer_hi, -peer_lo
            lower, upper = max(lo, peer_lo + offset), min(hi, peer_hi + offset)
            if lower <= upper:
                lo, hi = lower, upper
                if MIN_I64 <= lo and hi <= MAX_I64:
                    break
        return lo, hi

    def restrict(self, affine, facts, minimum=None, maximum=None):
        a, b = self.bounds(affine, facts)
        lo = max(a, minimum) if minimum is not None else a
        hi = min(b, maximum) if maximum is not None else b
        if lo > hi:
            return []
        if (lo, hi) == (a, b):
            return [facts]
        terms, offset = affine
        key, sign = self.orientation(terms)
        lo, hi = lo - offset, hi - offset
        if sign < 0:
            lo, hi = -hi, -lo
        return [{**facts, key: (lo, hi)}]

    def integer(self, expr, facts):
        self.visit()
        if type(expr) is int:
            return (), expr
        if isinstance(expr, str):
            return ((int(expr[1:]), 1),), 0
        op, lhs, rhs = expr
        value = linear(self.integer(lhs, facts), self.integer(rhs, facts), 1 if op == "add" else -1)
        lo, hi = self.bounds(value, facts)
        if lo < MIN_I64 or hi > MAX_I64:
            lo, hi = self.equality_peer_bounds(value, facts, lo, hi)
        require(MIN_I64 <= lo and hi <= MAX_I64, f"cannot prove signed i64 {op} does not overflow")
        return value

    def branch(self, expr, facts):
        self.visit()
        if isinstance(expr, str):
            return ([facts], []) if expr == "true" else ([], [facts])
        op, lhs, rhs = expr
        if op == "and" or op == "or":
            yes, no = self.branch(lhs, facts)
            reachable = yes if op == "and" else no
            out_yes, out_no = ([], no) if op == "and" else (yes, [])
            for earlier in reachable:
                rhs_yes, rhs_no = self.branch(rhs, earlier)
                out_yes.extend(rhs_yes)
                out_no.extend(rhs_no)
            return out_yes, out_no
        diff = linear(self.integer(lhs, facts), self.integer(rhs, facts), -1)
        if op == "eq":
            return (self.restrict(diff, facts, 0, 0),
                    self.restrict(diff, facts, maximum=-1) + self.restrict(diff, facts, minimum=1))
        if op in ("gt", "ge"):
            diff = linear(((), 0), diff, -1)
        cap = -1 if op in ("lt", "gt") else 0
        return self.restrict(diff, facts, maximum=cap), self.restrict(diff, facts, minimum=cap + 1)

    def result(self, expr, facts):
        self.visit()
        if isinstance(expr, (list, tuple)) and expr[0] == "if":
            _, cond, then, otherwise = expr
            yes, no = self.branch(cond, facts)
            for earlier in yes:
                self.result(then, earlier)
            for earlier in no:
                self.result(otherwise, earlier)
        else:
            self.integer(expr, facts)


def prove_i64_body(body):
    """Check an already typed canonical body (parameters represented as '$N')."""
    I64Proof().result(body, {})


class RecipeTable:
    def __init__(self):
        self.nodes = []
        self.exports = []
        self.recipes = []
        self.directives = []
        self.names = set()

    def expression(self, expr, params, expected, tail=False):
        if isinstance(expr, str):
            if expected == "bool" and expr in ("true", "false"):
                op, args, immediate, canonical = "Bool", [], int(expr == "true"), expr
            elif expected == "i64" and expr in params:
                index = params.index(expr)
                op, args, immediate, canonical = "Param", [], index, f"${index}"
            else:
                require(expected == "i64" and INTEGER.fullmatch(expr), f"invalid {expected} operand {expr!r}")
                n = int(expr)
                require(MIN_I64 <= n <= MAX_I64, "i64 literal out of range")
                op, args, immediate, canonical = "Integer", [], n, n
        else:
            name, children = form(expr)
            require(name in OPS, f"unknown recipe operator {name!r}")
            op, result, *inputs = OPS[name]
            require(len(children) == len(inputs) and result == expected and (name != "if" or tail),
                    f"wrong arity, type, or position for {name!r}")
            pairs = [self.expression(child, params, kind, name == "if" and i > 0)
                     for i, (child, kind) in enumerate(zip(children, inputs))]
            args = [pair[0] for pair in pairs]
            immediate = 0
            canonical = [name, *(pair[1] for pair in pairs)]
        pos = len(self.nodes)
        require(pos < MAX_TABLE_NODES, "generated executable table exceeds node limit")
        self.nodes.append((op, expected, *(args + [0] * (3 - len(args))), immediate))
        return pos, canonical

    def document(self, doc, kernel, header, digest):
        tag, items = form(doc)
        require(tag == "ffispec" and items and items[0] == "1", "expected ffispec version 1")
        scope = fields(items[1:], {"kernel", "header", "sha256", "backend", "target"}, {"export"})
        require(atom(scope.get("kernel", []), "kernel", KERNEL) == kernel,
                "recipe kernel differs from its generated index path")
        require(atom(scope.get("header", []), "header") == header,
                f"recipe header differs from the indexed header for {kernel}")
        require(atom(scope.get("sha256", []), "sha256", DIGEST) == digest,
                f"recipe header SHA-256 mismatch for {kernel}")
        parent = selectors(scope, {"backend": "", "target": ""})
        members, symbols = set(), set()
        require(scope.get("export"), "expected at least one executable export")
        for export in scope["export"]:
            require(len(self.exports) < MAX_TABLE_EXPORTS, "generated executable table exceeds export limit")
            require(len(export) >= 2, "export requires member and symbol")
            member = atom(export[:1], "member", IDENT)
            symbol = atom(export[1:2], "C symbol", IDENT)
            require(member not in members and symbol not in symbols,
                    f"duplicate export member or C symbol for {kernel}.{member}")
            members.add(member)
            symbols.add(symbol)
            options = fields(export[2:], {"params", "guard", "body", "backend", "target"}, DIRECTIVES)
            # ffispec v1 has exactly one function type family: a fixed list of
            # signed i64 parameters and a signed i64 result. No C ABI is loaded.
            params = options.get("params")
            require(params is not None and len(set(params)) == len(params)
                    and all(IDENT.fullmatch(p) and p not in {"true", "false"} for p in params),
                    f"invalid signed i64 signature for {kernel}.{member}")
            require(len(options.get("guard", [])) == 1 and len(options.get("body", [])) == 1,
                     f"expected one guard and body for {kernel}.{member}")
            guard, guard_identity = self.expression(options["guard"][0], params, "bool")
            body, body_identity = self.expression(options["body"][0], params, "i64", tail=True)
            try:
                I64Proof().branch(guard_identity, {})
                # Guards (and MIR directives) do not prove C arithmetic safe.
                # The body must be defined on the entire i64 input domain.
                prove_i64_body(body_identity)
            except ValueError as error:
                raise ValueError(f"{kernel}.{member}: {error}") from error
            inherited = selectors(options, parent)
            start = len(self.directives)
            directives = set()
            for kind, accepted_actions in DIRECTIVES.items():
                for args in options.get(kind, []):
                    action = atom(args[:1], f"{kind} action")
                    require(action in accepted_actions, f"unsupported {kind} directive")
                    scoped = fields(args[1:], {"backend", "target", "speculative"})
                    require(not scoped.get("speculative"), "speculative marker takes no arguments")
                    actual = selectors(scoped, inherited)
                    backend = accepted_actions[action]
                    require(backend is None or actual["backend"] == backend,
                            f"{kind} {action} requires effective backend {backend}")
                    entry = (actual["backend"], actual["target"], kind, action, "speculative" in scoped)
                    require(entry not in directives, "duplicate FFI directive")
                    directives.add(entry)
            require(start + len(directives) <= MAX_TABLE_DIRECTIVES,
                    "generated executable table exceeds directive limit")
            self.directives.extend(sorted(directives))
            name = f"@stdffi_{kernel.replace('-', '_')}_{member}"
            require(name not in self.names, f"duplicate internal function {name}")
            self.names.add(name)
            self.exports.append((kernel, member, symbol, name, header, digest, len(params)))
            self.recipes.append((guard, body, start, len(self.directives) - start))

    def header(self):
        def quote(item):
            if isinstance(item, bool):
                return "true" if item else "false"
            return json.dumps(item) if isinstance(item, str) else str(item)

        def table(kind, name, rows):
            return [f"inline constexpr std::array<{kind},{len(rows)}> {name} = {{{{",
                    *(" {"+",".join(row)+"}," for row in rows), "}};"]

        def format_node(node):
            opcode, kind, a, b, c, imm = node
            if imm == -(1 << 63):
                value = "(-INT64_C(9223372036854775807)-1)"
            else:
                value = ("-" if imm < 0 else "") + f"INT64_C({abs(imm)})"
            return (f"Code::{opcode}", "true" if kind == "bool" else "false",
                    str(a), str(b), str(c), value)

        return "\n".join([
            "// Generated from the executable FFI recipe index. Do not edit.",
            "#pragma once",
            "namespace on1x::stdffi::tables {",
            *table("Node", "nodes", [format_node(n) for n in self.nodes]),
            *table("Export", "exports", [list(map(quote, row)) for row in self.exports]),
            *table("Recipe", "recipes", [list(map(str, row)) for row in self.recipes]),
            *table("Directive", "directives", [list(map(quote, row)) for row in self.directives]),
            "}",
            "",
        ])


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--parser", type=Path, required=True)
    cli.add_argument("--generated-index", type=Path, required=True)
    cli.add_argument("--output", type=Path, required=True)
    args = cli.parse_args()
    path = args.generated_index
    try:
        parser = load_parser(args.parser)
        tables = RecipeTable()
        for kernel, (path, header, digest) in generated_recipes(args.generated_index):
            source = read(path).decode("ascii")
            bound_document(source)
            try:
                parsed, _ = parser.parse(source).children
            except parser.LanguageError as exc:
                raise ValueError(str(exc)) from exc
            _, tag, version, clauses, _ = parsed.children
            tables.document([spelling(tag), spelling(version), *map(field_tree, list_items(clauses))],
                            kernel, header, digest)
        # A failed admission leaves the old output intact, and an unchanged
        # registry must not force dependent C++ objects to be rebuilt.
        write_if_changed(args.output, tables.header().encode("ascii"))
    except (AttributeError, KeyError, TypeError, ValueError, OSError, RuntimeError) as exc:
        cli.exit(1, f"{path}: invalid standard FFI recipe: {exc}\n")


if __name__ == "__main__":
    main()
