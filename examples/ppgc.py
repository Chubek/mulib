#!/usr/bin/env python3
"""Compile a .ppg (JSON v1) formatter to a standalone Python program."""
import argparse
import inspect
import json
import re
import sys


LIMIT = 2 * 1024 * 1024
FORMS = {
    "text": "s", "fill": "s", "soft": "", "hard": "", "cat": "dd",
    "choice": "dd", "align": "d", "group": "d", "nest": "nd",
    "source": "nnnd", "annot": "sd", "region": "sd", "paragraph": "sd",
    "math-atom": "s", "frac": "dd", "sup": "dd", "sub": "dd", "root": "d",
}


def read_json(path):
    def pairs(items):
        result = dict(items)
        if len(result) != len(items):
            raise ValueError("duplicate JSON key")
        return result

    def invalid(_):
        raise ValueError("non-finite JSON number")

    if path == "-":
        data = sys.stdin.buffer.read(LIMIT + 1)
    else:
        with open(path, "rb") as stream:
            data = stream.read(LIMIT + 1)
    if len(data) > LIMIT:
        raise ValueError("JSON input limit exceeded")
    return json.loads(data.decode("utf-8"), object_pairs_hook=pairs, parse_constant=invalid)


def quote(value):
    if not isinstance(value, str):
        raise ValueError("expected string")
    if len(value.encode("utf-8")) > 1024 * 1024:
        raise ValueError("quoted string limit exceeded")
    if any(ord(c) < 32 and c not in "\r\n\t" or c in "\ufffe\uffff" for c in value):
        raise ValueError("unsupported document character")
    return json.dumps(value, ensure_ascii=False)


def compile_rules(spec):
    if (not isinstance(spec, dict) or set(spec) != {"version", "rules"} or
        type(spec["version"]) is not int or spec["version"] != 1 or
        not isinstance(spec["rules"], list) or not 1 <= len(spec["rules"]) <= 512):
        raise ValueError("expected .ppg version 1 with 1..512 rules")
    table = {}
    for rule in spec["rules"]:
        if not isinstance(rule, dict) or set(rule) != {"pattern", "emit"}:
            raise ValueError("expected pattern and emit fields")
        names = {}

        def pattern(node, depth):
            if depth > 64:
                raise ValueError("pattern depth exceeded")
            if isinstance(node, dict) and set(node) == {"capture"}:
                name = node["capture"]
                if not isinstance(name, str) or not re.fullmatch(r"[A-Za-z_][A-Za-z_0-9]*", name) or name in names:
                    raise ValueError("invalid or duplicate capture")
                names[name] = len(names)
                return (0, names[name])
            if isinstance(node, list):
                return (1, tuple(pattern(child, depth + 1) for child in node))
            if node is None or type(node) in (str, int, bool):
                return (2, node)
            raise ValueError("invalid pattern")

        def template(node, typ, depth):
            if depth > 64:
                raise ValueError("template depth exceeded")
            if isinstance(node, dict) and set(node) == ({"render"} if typ == "d" else {"value"}):
                name = next(iter(node.values()))
                if not isinstance(name, str) or name not in names:
                    raise ValueError("unknown capture")
                return (3 if typ == "d" else 4 if typ == "s" else 5, names[name])
            if typ == "s":
                return (6, quote(node))
            if typ == "n":
                if type(node) is not int or not 0 <= node <= 0xffffffff:
                    raise ValueError("invalid unsigned literal")
                return (6, str(node))
            if not isinstance(node, list) or not node or not isinstance(node[0], str):
                raise ValueError("expected document template")
            tag, *args = node
            if tag == "matrix":
                if not 1 <= len(args) <= 32:
                    raise ValueError("invalid matrix rows")
                cols = None
                rows = []
                for row in args:
                    if (not isinstance(row, list) or not 2 <= len(row) <= 33 or
                        row[0] != "row" or (cols is not None and cols != len(row))):
                        raise ValueError("expected rectangular matrix")
                    cols = len(row)
                    rows.append((7, "row", tuple(template(x, "d", depth + 2) for x in row[1:])))
                return (7, tag, tuple(rows))
            types = FORMS.get(tag)
            if types is None or len(types) != len(args):
                raise ValueError(f"unknown form or wrong arity: {tag!r}")
            modes = {
                "region": {"greedy", "fits", "search"},
                "paragraph": {"greedy", "ragged", "knuth-plass"},
                "annot": {"normal", "keyword", "identifier", "number", "operator", "comment",
                          "string", "math-ident", "math-number", "math-operator"},
            }
            if tag in modes and isinstance(args[0], str) and args[0] not in modes[tag]:
                raise ValueError(f"unknown {tag} mode: {args[0]!r}")
            return (7, tag, tuple(template(x, t, depth + 1) for x, t in zip(args, types)))

        root = rule["pattern"]
        if not isinstance(root, list) or not root or not isinstance(root[0], str):
            raise ValueError("rule must start with a literal AST tag")
        match = pattern(root, 0)
        code = template(rule["emit"], "d", 0)
        table.setdefault(root[0], []).append((match, len(names), code))
    return table


def execute(table, ast):
    output = []
    work, remaining = 500000, LIMIT

    def step():
        nonlocal work
        work -= 1
        if work < 0:
            raise ValueError("formatter work limit exceeded")

    def put(text):
        nonlocal remaining
        remaining -= len(text.encode("utf-8"))
        if remaining < 0:
            raise ValueError("formatter output limit exceeded")
        output.append(text)

    def match(pattern, node, slots):
        step()
        op, arg = pattern
        if op == 0:
            slots[arg] = node
            return True
        if op == 1:
            return isinstance(node, list) and len(node) == len(arg) and all(
                match(p, n, slots) for p, n in zip(arg, node))
        return type(node) is type(arg) and node == arg

    def render(node, depth):
        step()
        if depth > 128:
            raise ValueError("expanded document depth exceeded")
        if not isinstance(node, list) or not node or not isinstance(node[0], str):
            raise ValueError("expected tagged AST node")
        for pattern, size, code in table.get(node[0], ()):
            slots = [None] * size
            if match(pattern, node, slots):
                emit(code, slots, depth)
                return
        raise ValueError(f"no formatter rule for {node[0][:80]!r}")

    def emit(code, slots, depth):
        step()
        if depth > 128:
            raise ValueError("expanded document depth exceeded")
        op, arg, *args = code
        if op == 3:
            render(slots[arg], depth)
        elif op == 4:
            put(quote(slots[arg]))
        elif op == 5:
            n = slots[arg]
            if type(n) is not int or not 0 <= n <= 0xffffffff:
                raise ValueError("invalid unsigned capture")
            put(str(n))
        elif op == 6:
            put(arg)
        else:
            put("(" + arg)
            for child in args[0]:
                put(" ")
                emit(child, slots, depth + 1)
            put(")")

    render(ast, 0)
    return "".join(output)


def generated_main():
    try:
        if len(sys.argv) > 2:
            raise ValueError("usage: formatter.py [AST.json|-]")
        sys.stdout.buffer.write(execute(TABLE, read_json(sys.argv[1] if len(sys.argv) == 2 else "-")).encode("utf-8"))
    except (ValueError, OSError, RecursionError) as exc:
        sys.exit(f"formatter: {exc}")


def main():
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument("spec")
    options = args.parse_args()
    try:
        table = compile_rules(read_json(options.spec))
        source = "# Generated by ppgc.py; .ppg v1\nimport json\nimport sys\n"
        source += f"LIMIT = {LIMIT}\nTABLE = {ascii(table)}\n"
        for function in (read_json, quote, execute, generated_main):
            source += "\n" + inspect.getsource(function)
        sys.stdout.write(source + '\nif __name__ == "__main__":\n    generated_main()\n')
    except (ValueError, OSError, RecursionError) as exc:
        args.exit(1, f"ppgc: {exc}\n")


if __name__ == "__main__":
    main()
