#!/usr/bin/env python3
"""Compile reviewed ffispec v1 recipes without embedding source text."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys

MAX_FILE = 65536
KERNEL = re.compile(r"[a-z][a-z0-9]*(?:-[a-z0-9]+)*")
IDENT = re.compile(r"[a-zA-Z_][a-zA-Z_0-9]{0,63}")
SELECTOR = re.compile(r"[a-zA-Z_0-9][a-zA-Z_0-9.-]{0,127}")
INTEGER = re.compile(r"-?(?:0|[1-9][0-9]{0,18})")
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
DIRECTIVES = {"hint": {"kernel-hot", "private-code"}, "rewrite": {"fold-identity"}}


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


def inputs(grammar, review_path, include, specs):
    review_bytes = read(review_path)
    review = json.loads(review_bytes)
    require(isinstance(review, dict) and review, "expected a nonempty review lock")
    headers = []
    for kernel, approved in sorted(review.items()):
        require(KERNEL.fullmatch(kernel) and approved.get("header") == f"mulib-{kernel}.h",
                "invalid reviewed kernel or noncanonical header name")
        header = approved["header"]
        digest = sha256(read(include / header, 8 * 1024 * 1024))
        require(digest == approved["sha256"], f"pinned header SHA-256 mismatch: {header}")
        headers.append([header, digest])
    sources = sorted((read(path), path) for path in specs)
    stamp = ["FFIHandler-bootstrap-v1", sha256(read(Path(__file__))), sha256(read(grammar)), sha256(review_bytes),
             [sha256(content) for content, _ in sources], headers]
    return review, sources, sha256(json.dumps(stamp, separators=(",", ":")).encode("ascii"))


def snapshot(stamp, payload):
    return f"// FFIHandler bootstrap {stamp} {sha256(payload)}\n".encode("ascii") + payload


def check_snapshot(path, stamp):
    data = read(path, 2 * 1024 * 1024)
    _, separator, payload = data.partition(b"\n")
    require(separator and data == snapshot(stamp, payload),
            "stale FFIHandler bootstrap; regenerate on1x_stdffi_regenerate with LibStone LLSynth")
    return data


def load_parser(path):
    spec = importlib.util.spec_from_file_location("ffihandler_llsynth", path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


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


def expression_tree(node, depth=0):
    require(depth <= 32, "FFI recipe exceeds nesting limit")
    if len(node.children) == 1:
        return spelling(node.children[0])
    _, op, *operands, _ = node.children
    return [spelling(op), *(expression_tree(child, depth + 1) for child in operands)]


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


class RecipeTable:
    def __init__(self):
        self.nodes = []
        self.exports = []
        self.recipes = []
        self.directives = []

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
                require(-(1 << 63) <= n < 1 << 63, "i64 literal out of range")
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
        self.nodes.append((op, expected, *(args + [0] * (3 - len(args))), immediate))
        return pos, canonical

    def document(self, doc, review, seen):
        tag, items = form(doc)
        require(tag == "ffispec" and items and items[0] == "1", "expected ffispec version 1")
        scope = fields(items[1:], {"kernel", "header", "sha256", "backend", "target"}, {"export"})
        kernel = atom(scope.get("kernel", []), "kernel", KERNEL)
        require(kernel in review and kernel not in seen, f"unreviewed or duplicate kernel {kernel!r}")
        seen.add(kernel)
        approved = review[kernel]
        header = atom(scope.get("header", []), "header")
        digest = atom(scope.get("sha256", []), "sha256", re.compile("[0-9a-f]{64}"))
        require(header == approved["header"] and digest == approved["sha256"],
                f"unreviewed header or SHA-256 for {kernel}")
        parent = selectors(scope, {"backend": "", "target": ""})
        members = set()
        for export in scope.get("export", []):
            require(len(export) >= 2, "export requires member and symbol")
            member = atom(export[:1], "member", IDENT)
            symbol = atom(export[1:2], "C symbol", IDENT)
            require(member in approved["exports"] and member not in members,
                    f"unreviewed or duplicate export {kernel}.{member}")
            members.add(member)
            identity = approved["exports"][member]
            require(symbol == identity["symbol"], f"unreviewed C symbol for {kernel}.{member}")
            options = fields(export[2:], {"params", "guard", "body", "backend", "target"}, DIRECTIVES)
            params = options.get("params", [])
            require(len(params) == identity["arity"] and
                    all(isinstance(p, str) and IDENT.fullmatch(p) and p not in {"true", "false"} for p in params)
                    and len(set(params)) == len(params), f"invalid parameters for {kernel}.{member}")
            require(len(options.get("guard", [])) == 1 and len(options.get("body", [])) == 1,
                    f"expected one guard and body for {kernel}.{member}")
            guard, guard_identity = self.expression(options["guard"][0], params, "bool")
            body, body_identity = self.expression(options["body"][0], params, "i64", tail=True)
            require((guard_identity, body_identity) == (identity["guard"], identity["body"]),
                    f"unreviewed guard or body for {kernel}.{member}")
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
                    entry = (actual["backend"], actual["target"], kind, action, "speculative" in scoped)
                    require(entry not in directives, "duplicate FFI directive")
                    directives.add(entry)
            self.directives.extend(sorted(directives))
            name = f"@stdffi_{kernel.replace('-', '_')}_{member}"
            require(all(item[3] != name for item in self.exports), f"duplicate internal function {name}")
            self.exports.append((kernel, member, symbol, name, header, digest, len(params)))
            self.recipes.append((guard, body, start, len(self.directives) - start))
        require(members == approved["exports"].keys(), f"missing approved exports for {kernel}")

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
            "// Generated from reviewed FFI recipes. Do not edit.",
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
    mode = cli.add_mutually_exclusive_group(required=True)
    mode.add_argument("--parser", type=Path)
    mode.add_argument("--bootstrap", type=Path)
    cli.add_argument("--grammar", type=Path, required=True)
    cli.add_argument("--review", type=Path, required=True)
    cli.add_argument("--include", type=Path, required=True)
    cli.add_argument("--output", type=Path, required=True)
    cli.add_argument("specs", type=Path, nargs="+")
    args = cli.parse_args()
    path = args.review
    try:
        review, sources, stamp = inputs(args.grammar, args.review, args.include, args.specs)
        if args.bootstrap:
            result = check_snapshot(args.bootstrap, stamp)
        else:
            parser = load_parser(args.parser)
            tables, seen = RecipeTable(), set()
            for content, path in sources:
                try:
                    parsed, _ = parser.parse(content.decode("ascii")).children
                except parser.LanguageError as exc:
                    raise ValueError(str(exc)) from exc
                _, tag, version, clauses, _ = parsed.children
                tables.document([spelling(tag), spelling(version), *map(field_tree, list_items(clauses))],
                                review, seen)
            require(seen == review.keys(), "missing reviewed kernel specifications")
            result = snapshot(stamp, tables.header().encode("ascii"))
        # Only replace the output after every file has passed admission.
        temporary = args.output.with_suffix(".tmp")
        temporary.write_bytes(result)
        temporary.replace(args.output)
    except (AttributeError, KeyError, TypeError, ValueError, OSError) as exc:
        cli.exit(1, f"{path}: invalid standard FFI recipe: {exc}\n")


if __name__ == "__main__":
    main()
