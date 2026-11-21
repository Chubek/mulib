#!/usr/bin/env python3
"""Compile LLSynth-parsed On1xMETA rules into bounded, immutable C++ tables.

This is a validator/emitter over the *generated parser's parse tree*. It contains
no lexer or rule-language parser. On1x source parsing remains in the frontend.
"""
import argparse
import hashlib
import importlib.util
import itertools
import json
from pathlib import Path
import sys
import tempfile

MAX_BYTES = 262144
MAX_RULES = 512
MAX_TREE = 128
MAX_DEPTH = 32
ARITIES = {"Capture": 0, "Unary": 1, "Binary": 2, "Boolean": 0,
           "Integer": 0, "Unit": 0, "Select": 3}
PHASES = {"canonicalize": "Canonicalize", "desugar": "Desugar"}
GUARDS = {"always": "Always", "boolean": "Boolean", "integer": "Integer",
          "pure": "Pure"}
PROPERTIES = {
    "arity", "min_arity", "type", "resolved", "callee", "forbidden",
    "body", "operands", "result", "value",
}
STMT_KINDS = {"Stmt" + name for name in (
    "Let", "Assign", "Return", "If", "While", "Expr", "Block", "Template",
    "Range", "Break", "Continue", "Storage", "Initialize", "Region", "Leave",
    "Trap", "Defer", "PushDefer", "TaskGroup", "CspScope")}
EXPR_KINDS = {
    "Integer", "Float", "Boolean", "String", "Variable", "ClicheMember",
    "InherentMember", "Unary", "Binary", "Call", "Lambda", "Tuple",
    "PolyPack", "PolyProjection", "MacroCall", "SyntaxHole", "SyntaxBlock",
    "Extension", "SyntaxList", "SyntaxVector", "SyntaxKeyword", "Quote",
    "Splice", "Lift", "Run", "SyntaxLet", "Record", "Variant", "Match",
    "Array", "Field", "Index", "Slice", "Length", "PatternMatch", "Case",
    "Unit", "Region", "TagTest", "Payload", "Project", "Loop", "Propagate",
    "SourceCsp", "Csp", "UnsafeBlock", "Character", "ByteBuffer",
    "Interpolated", "RegexLiteral", "RegexMatch", "RegexCapture", "Borrow",
}


class Invalid(ValueError):
    pass


def fail(message):
    raise Invalid(message)


def load_parser(path):
    spec = importlib.util.spec_from_file_location("on1x_meta_generated_parser", path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    module.on1x_parser_sha256 = hashlib.sha256(path.read_bytes()).hexdigest()
    return module


def leaves(node):
    """Iterative provenance lookup, never a second tokenization."""
    pending = [node]
    while pending:
        current = pending.pop()
        if current.children:
            pending.extend(reversed(current.children))
        elif current.token:
            yield current.token


def token(node):
    return next(leaves(node))


def string(node):
    try:
        return json.loads(token(node).lexeme)
    except json.JSONDecodeError as exc:
        fail("invalid quoted string: " + exc.msg)


def tree(node, depth=0):
    if depth >= MAX_DEPTH:
        fail("META_BUDGET: rule tree depth exceeds 32")
    children = node.children
    if token(children[0]).lexeme == "$":
        return ("Capture", token(children[1]).lexeme, ())
    kind = token(children[0]).lexeme
    if kind not in ARITIES or kind == "Capture":
        fail("unknown tree constructor " + kind)
    arguments = children[2].children
    text, result = "", []
    if arguments:
        if arguments[0].symbol == "Tree":
            result.append(tree(arguments[0], depth + 1))
        else:
            text = string(arguments[0])
        tail = arguments[1]
        while tail.children:
            result.append(tree(tail.children[1], depth + 1))
            tail = tail.children[2]
    if len(result) != ARITIES[kind]:
        fail(f"{kind} requires {ARITIES[kind]} tree operands")
    if kind in {"Select", "Unit"} and text:
        fail(kind + " requires empty text")
    if kind == "Boolean" and text not in {"true", "false"}:
        fail("Boolean requires true or false")
    if kind == "Integer":
        try:
            value = int(text)
        except ValueError:
            fail("Integer requires canonical decimal i64")
        if str(value) != text or not -(2**63) <= value < 2**63:
            fail("Integer requires canonical decimal i64")
    if kind == "Unary" and text not in {"!", "-"}:
        fail("unsupported unary operator " + text)
    if kind == "Binary" and text not in {
            "&&", "||", "==", "!=", "<", "<=", ">", ">=", "+", "-", "*", "/"}:
        fail("unsupported binary operator " + text)
    return (kind, text, tuple(result))


def walk(node):
    yield node
    for child in node[2]:
        yield from walk(child)


def weight(node):
    # Sugar constructors dominate their permitted, finite expansion. Every
    # application strictly decreases this natural-number potential.
    cost = 1
    if node[0] == "Binary":
        cost = {"&&": 100, "||": 100, "!=": 10, "<=": 10, ">=": 10}.get(node[1], 1)
    return cost + sum(weight(c) for c in node[2])


def boolean_eval(node, environment, trace):
    kind, text, children = node
    if kind == "Capture":
        trace.append(text)
        return environment[text]
    if kind == "Boolean":
        return text == "true"
    if kind == "Unary" and text == "!":
        return not boolean_eval(children[0], environment, trace)
    if kind == "Select":
        cond = boolean_eval(children[0], environment, trace)
        return boolean_eval(children[1 if cond else 2], environment, trace)
    if kind == "Binary":
        lhs = boolean_eval(children[0], environment, trace)
        if text == "&&":
            return lhs and boolean_eval(children[1], environment, trace)
        if text == "||":
            return lhs or boolean_eval(children[1], environment, trace)
        rhs = boolean_eval(children[1], environment, trace)
        if text == "==":
            return lhs == rhs
        if text == "!=":
            return lhs != rhs
    fail("boolean guard admits only boolean operators/literals/captures")


def integer_normal(node):
    """Proof normal form for a restrictive checked-i64 identity algebra.

    No arithmetic is evaluated in the host and no overflowing intermediate is
    removed, except identities whose checked operation cannot overflow.
    """
    kind, text, children = node
    if kind in {"Capture", "Integer"}:
        return node
    if kind == "Unary" and text == "-":
        # This is a literal spelling of signed zero, not a license to
        # evaluate checked negation (in particular, -INT64_MIN traps).
        if children[0] == ("Integer", "0", ()):
            return children[0]
        fail("unproved checked-i64 negation: only literal zero is admitted")
    if kind == "Unary" and text == "!":
        child = integer_normal(children[0])
        if child[0] == "Not":
            return child[2][0]
        return ("Not", "", (child,))
    if kind != "Binary":
        fail("integer guard admits only i64 identities/comparison duals")
    lhs, rhs = map(integer_normal, children)
    zero, one = ("Integer", "0", ()), ("Integer", "1", ())
    if text in {"+", "-"} and rhs == zero:
        return lhs
    if text == "+" and lhs == zero:
        return rhs
    if text in {"*", "/"} and rhs == one:
        return lhs
    if text == "*" and lhs == one:
        return rhs
    if text in {"!=", "<=", ">="}:
        positive = {"!=": "==", "<=": ">", ">=": "<"}[text]
        return ("Not", "", (("Binary", positive, (lhs, rhs)),))
    return ("Binary", text, (lhs, rhs))


def integer_type(node):
    kind, text, children = node
    if kind in {"Capture", "Integer"}:
        return "i64"
    if kind == "Unary" and text == "-" and integer_type(children[0]) == "i64":
        return "i64"
    if kind == "Unary" and text == "!" and integer_type(children[0]) == "bool":
        return "bool"
    if (kind == "Binary" and text not in {"&&", "||"}
            and all(integer_type(c) == "i64" for c in children)):
        return "bool" if text in {"==", "!=", "<", "<=", ">", ">="} else "i64"
    fail("integer rule has inconsistent operator types")


def validate_rule(rule):
    match, replacement = rule["match"], rule["replacement"]
    for node in (match, replacement):
        if len(list(walk(node))) > MAX_TREE:
            fail("META_BUDGET: too many tree nodes")
    if match[0] == "Capture":
        fail("match root must be a structural constructor")
    if any(n[0] == "Select" for n in walk(match)):
        fail("Select is a replacement-only constructor")
    captures = [n[1] for n in walk(match) if n[0] == "Capture"]
    uses = [n[1] for n in walk(replacement) if n[0] == "Capture"]
    if len(captures) != len(set(captures)):
        fail("match captures must be unique")
    if not set(uses) <= set(captures):
        fail("replacement contains an unbound capture")
    if len(uses) != len(set(uses)):
        fail("replacement duplicates a capture")
    if weight(replacement) >= weight(match):
        fail("termination: replacement must strictly decrease weighted tree size")
    guard = rule["guard"]
    if guard in {"boolean", "always"}:
        if guard == "always" and captures:
            fail("always guard cannot establish captured operand types")
        if len(captures) > 6:
            fail("META_BUDGET: boolean proof has more than six captures")
        for values in itertools.product((False, True), repeat=len(captures)):
            environment = dict(zip(captures, values))
            left_trace, right_trace = [], []
            left = boolean_eval(match, environment, left_trace)
            right = boolean_eval(replacement, environment, right_trace)
            if left != right or left_trace != right_trace:
                fail("semantic invariant: boolean value or lazy evaluation order changes")
        if set(captures) != set(uses):
            fail("semantic invariant: replacement drops a capture")
    elif guard == "integer":
        if captures != uses:
            fail("semantic invariant: i64 evaluation order/multiplicity changes")
        if integer_type(match) != integer_type(replacement):
            fail("semantic invariant: replacement result type changes")
        if integer_normal(match) != integer_normal(replacement):
            fail("semantic invariant: unproved checked-i64 rewrite")
    else:
        fail("pure guard cannot prove operand types; use boolean or integer")


def overlap(left, right):
    if left[0] == "Capture" or right[0] == "Capture":
        return True
    return left[:2] == right[:2] and len(left[2]) == len(right[2]) and all(
        overlap(a, b) for a, b in zip(left[2], right[2]))


def parse_file(parser, path):
    with path.open("rb") as source:
        data = source.read(MAX_BYTES + 1)
    if len(data) > MAX_BYTES:
        fail(f"{path}:1:1: META_BUDGET: file exceeds {MAX_BYTES} bytes")
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError as exc:
        fail(f"{path}:1:1: invalid UTF-8 at byte {exc.start}")
    # LLSynth owns both lexical and syntactic recognition. Its LR parser is
    # iterative; the bounded tree conversion below runs only after acceptance.
    try:
        root = parser.parse(text)
    except parser.LanguageError as exc:
        fail(f"{path}:{getattr(exc, 'line', 1)}:{getattr(exc, 'column', 1)}: {exc}")
    header = list(leaves(root.children[0]))
    if header[1].value != 1:
        fail(f"{path}:1:1: unsupported meta version")
    budgets = tuple(header[i].value for i in (5, 7, 9))
    if not (1 <= budgets[0] <= 16777216 and 1 <= budgets[1] <= 2000000
            and 1 <= budgets[2] <= 1024):
        fail(f"{path}:1:1: META_BUDGET: budget outside supported bounds")
    declarations = []
    pending = [root.children[1]]
    while pending:
        node = pending.pop()
        if node.symbol == "Declaration":
            declarations.append(node.children[0])
        else:
            pending.extend(reversed(node.children))
    result = []
    for node in declarations:
        first = token(node)
        location = {"file": path.name, "line": first.line, "column": first.column}
        c = node.children
        try:
            if node.symbol == "Rewrite":
                rule = {**location, "name": token(c[1]).lexeme,
                        "phase": token(c[3]).lexeme, "priority": token(c[5]).value,
                        "match": tree(c[7]), "replacement": tree(c[9]),
                        "guard": token(c[11]).lexeme}
                if rule["priority"] > 65535:
                    fail("priority exceeds 65535")
                if rule["guard"] not in GUARDS:
                    fail("unknown guard " + rule["guard"])
                validate_rule(rule)
                result.append(rule)
            else:
                constraint = {**location, "name": token(c[1]).lexeme,
                              "phase": "semantic", "kind": token(c[5]).lexeme,
                              "property": token(c[7]).lexeme, "value": string(c[8]),
                              "message": string(c[10])}
                validate_constraint(constraint)
                result.append(constraint)
        except Invalid as exc:
            fail(f"{path}:{first.line}:{first.column}: {exc}")
    return budgets, result, hashlib.sha256(data).hexdigest()


def validate_constraint(c):
    prop, value = c["property"], c["value"]
    if c["kind"] not in EXPR_KINDS | STMT_KINDS:
        fail("unknown expression/statement kind " + c["kind"])
    if prop not in PROPERTIES:
        fail("unknown semantic property " + prop)
    if prop in {"arity", "min_arity"}:
        if (not value.isascii() or not value.isdecimal()
                or len(value) > 5 or int(value) > 65535):
            fail("arity must be decimal in [0,65535]")
    elif prop == "type" and value not in {"bool", "i64", "unit"}:
        fail("unsupported type constraint")
    elif prop in {"resolved", "forbidden"} and value != "true":
        fail(prop + " requires true")
    elif prop == "callee" and value not in {"present", "absent"}:
        fail("callee requires present or absent")
    elif prop == "body" and value not in {"present", "absent"}:
        fail("body requires present or absent")
    elif prop == "operands" and value not in {"boolean", "same", "numeric"}:
        fail("operands requires boolean, same or numeric")
    elif prop == "result" and value != "operand0":
        fail("result requires operand0")
    elif prop == "value" and value not in {"present", "absent", "boolean"}:
        fail("value requires present, absent or boolean")
    if c["kind"] in STMT_KINDS and prop not in {"body", "value", "forbidden"}:
        fail("property is not supported for statement selectors")
    if c["kind"] in EXPR_KINDS and prop == "value":
        fail("value is only supported for statement selectors")
    if not c["message"] or len(c["message"]) > 512:
        fail("diagnostic message must have 1..512 characters")


def quote(text):
    # JSON escapes are C++ compatible for admitted printable ASCII. Encode
    # remaining UTF-8 bytes as fixed-width octal to avoid C++ universal-char
    # restrictions on control characters and greedy hexadecimal escapes.
    return '"' + "".join(chr(b) if 32 <= b < 127 and chr(b) not in {'"', '\\'}
                         else f"\\{b:03o}" for b in text.encode("utf-8")) + '"'


def compile_inputs(parser, inputs):
    if not 1 <= len(inputs) <= 64:
        fail("META_BUDGET: require 1..64 input modules")
    declarations, sources, budgets = [], [], None
    names, filenames = set(), set()
    for path in sorted(inputs, key=lambda p: p.name):
        if path.name in filenames:
            fail("input basenames must be unique for stable source provenance")
        filenames.add(path.name)
        file_budgets, items, digest = parse_file(parser, path)
        if budgets is not None and file_budgets != budgets:
            fail(f"{path}:1:1: all inputs must declare identical budgets")
        budgets = file_budgets
        for item in items:
            if item["name"] in names:
                fail(f"{path}:{item['line']}:1: duplicate declaration {item['name']}")
            names.add(item["name"])
        declarations.extend(items)
        if len(declarations) > MAX_RULES:
            fail("META_BUDGET: more than 512 declarations")
        sources.append({"file": path.name, "sha256": digest})
    rules = sorted((d for d in declarations if d["phase"] != "semantic"),
                   key=lambda r: (list(PHASES).index(r["phase"]), -r["priority"], r["name"]))
    constraints = sorted((d for d in declarations if d["phase"] == "semantic"),
                         key=lambda r: r["name"])
    for index, rule in enumerate(rules):
        for other in rules[:index]:
            if (rule["phase"], rule["priority"]) == (other["phase"], other["priority"]):
                if overlap(rule["match"], other["match"]):
                    fail(f"{rule['file']}:{rule['line']}:1: ambiguous equal-priority "
                         f"overlap: {other['name']} and {rule['name']}")
    nodes = []

    def emit_node(node):
        children = [emit_node(c) for c in node[2]]
        index = len(nodes)
        nodes.append((node[0], node[1], children))
        return index

    rows = []
    for rule in rules:
        left, right = emit_node(rule["match"]), emit_node(rule["replacement"])
        rows.append(" {" + ", ".join([
            quote(rule["name"]), "Phase::" + PHASES[rule["phase"]],
            str(rule["priority"]), str(left), str(right), "Guard::" + GUARDS[rule["guard"]],
            quote(rule["file"]), str(rule["line"]), str(rule["column"])]) + "},")
    lines = ["// Generated by On1xMETA; do not edit.", "#pragma once",
             "#include <on1x/meta_schema.hpp>", "namespace on1x::meta::generated {",
             "inline constexpr std::uint32_t schema_version = 1;",
             "inline constexpr Budgets budgets{" + ",".join(map(str, budgets)) + "};",
             f"inline constexpr std::array<Node, {len(nodes)}> nodes{{{{"]
    for kind, text, children in nodes:
        indexes = [str(i) for i in children] + ["no_node"] * (3 - len(children))
        lines.append(f" {{NodeKind::{kind}, {quote(text)}, {{{','.join(indexes)}}}, {len(children)}}},")
    lines += ["}};", f"inline constexpr std::array<Rule, {len(rows)}> rules{{{{", *rows, "}};",
              f"inline constexpr std::array<Constraint, {len(constraints)}> constraints{{{{"]
    for c in constraints:
        fields = [quote(c[k]) for k in ("name", "kind", "property", "value", "message", "file")]
        lines.append(" {" + ", ".join(fields + [str(c["line"]), str(c["column"])]) + "},")
    lines += ["}};", "}", ""]
    payload = "\n".join(lines)
    schema_sha256 = hashlib.sha256(
        (Path(__file__).resolve().parent.parent / "include/on1x/meta_schema.hpp").read_bytes()).hexdigest()
    generator_sha256 = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    provenance = {"schema_sha256": schema_sha256, "generator_sha256": generator_sha256,
                  "parser_sha256": parser.on1x_parser_sha256, "sources": sources,
                  "payload_sha256": hashlib.sha256(payload.encode()).hexdigest()}
    program_sha256 = hashlib.sha256(json.dumps(
        provenance, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
    lines[5:5] = [
        f"inline constexpr char program_sha256[] = {quote(program_sha256)};",
        f"inline constexpr char parser_sha256[] = {quote(parser.on1x_parser_sha256)};",
    ]
    output = "\n".join(lines)
    manifest = {"schema": 1, "parser": "LibStone LLSynth generated LR/DFA",
                "sources": sources, "rules": len(rules), "constraints": len(constraints),
                "nodes": len(nodes), "budgets": dict(zip(("steps", "nodes", "depth"), budgets)),
                "program_sha256": program_sha256, **provenance,
                "output_sha256": hashlib.sha256(output.encode()).hexdigest()}
    return output, json.dumps(manifest, sort_keys=True, indent=2) + "\n"


def publish(path, content):
    path.parent.mkdir(parents=True, exist_ok=True)
    # Validation is complete before any output is touched.
    with tempfile.NamedTemporaryFile("w", dir=path.parent, delete=False,
                                     encoding="utf-8", newline="\n") as stream:
        temporary = Path(stream.name)
        stream.write(content)
    temporary.replace(path)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--parser", type=Path, required=True)
    ap.add_argument("--output", type=Path, required=True)
    ap.add_argument("--manifest", type=Path)
    ap.add_argument("inputs", nargs="+", type=Path)
    args = ap.parse_args()
    try:
        outputs = [args.output] + ([args.manifest] if args.manifest else [])
        protected = [args.parser, Path(__file__), *args.inputs]
        if len({p.resolve() for p in outputs}) != len(outputs):
            fail("output path collision")
        if any(p.resolve() == q.resolve() for p in outputs for q in protected):
            fail("output would overwrite an input")
        parser = load_parser(args.parser)
        output, manifest = compile_inputs(parser, args.inputs)
        publish(args.output, output)
        if args.manifest:
            publish(args.manifest, manifest)
    except (Invalid, UnicodeError, OSError, RecursionError) as exc:
        print(f"On1xMETA: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
