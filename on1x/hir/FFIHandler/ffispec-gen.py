#!/usr/bin/env python3
"""Generate portable On1x FFI specs from reviewed MuLIB C11 definitions."""

from __future__ import annotations

import argparse
from bisect import bisect_right
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import re
import sys


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
CEPHYR_CONFIG = ROOT / "on1x/third_party/libstone/cornerstone/Cephyr/Cephyr.toml"
IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z0-9_]{0,63}\Z")
KERNEL = re.compile(r"[a-z][a-z0-9]*(?:-[a-z0-9]+)*\Z")
HEADER_ID = re.compile(r"[a-z0-9]+(?:[-_+][a-z0-9]+)*\Z")
MIN_I64, MAX_I64 = -(1 << 63), (1 << 63) - 1
DEPTH_LIMIT = 32
NODE_LIMIT = 512
DOCUMENT_LIMIT = 8 * 1024 * 1024
CONDITIONAL = re.compile(rb"^[ \t]*#[ \t]*(?:if|ifdef|ifndef|elif)\b[^\r\n]*", re.MULTILINE)
IMPL_MACRO = re.compile(rb"\bMULIB_IMPL_[A-Z0-9_]+\b")
COMPARISONS = {
    "less": "lt", "less_equal": "le", "greater": "gt",
    "greater_equal": "ge", "equal": "eq",
}
LOGIC = {"logical_and": "and", "logical_or": "or"}
ARITHMETIC = {"add": "add", "subtract": "sub"}
RESERVED = {"true", "false"}


class Rejection(Exception):
    """Input cannot be admitted by the portable FFI profile."""


def require(condition, explanation):
    if not condition:
        raise Rejection(explanation)


def identifier(name):
    require(isinstance(name, str) and IDENTIFIER.fullmatch(name) and name not in RESERVED,
            f"unsupported identifier: {name!r}")
    return name


# Read Cephyr's versioned interface S-expression (not C source or FFI input).
# Cephyr emits quoted strings with JSON-compatible escapes.
TOKEN = re.compile(r'\s+|[()]|"(?:[^"\\\x00-\x1f]|\\.)*"|[^()\s"]+')


def read_interface(text):
    forms = [[]]
    position = 0
    while position < len(text):
        match = TOKEN.match(text, position)
        require(match is not None, "malformed Cephyr interface token")
        token = match[0]
        position = match.end()
        if token == "(":
            require(len(forms) <= DEPTH_LIMIT, "Cephyr interface exceeds depth limit")
            forms.append([])
        elif token == ")":
            require(len(forms) > 1, "unbalanced Cephyr interface")
            form = forms.pop()
            forms[-1].append(form)
        elif token.startswith('"'):
            forms[-1].append(json.loads(token))
        elif not token.isspace():
            forms[-1].append(token)
    require(len(forms) == 1 and len(forms[0]) == 1, "unbalanced Cephyr interface")
    document = forms[0][0]
    require(isinstance(document, list) and document[:2] == ["interface", "1"],
            "unsupported Cephyr interface schema")
    return document[2:]


def field(forms, tag):
    matches = [form[1:] for form in forms if isinstance(form, list) and form[:1] == [tag]]
    require(len(matches) == 1, f"expected one Cephyr interface field {tag}")
    return matches[0]


def i64_type(form):
    require(form[:1] == ["integer"] and field(form, "bits") == ["64"]
            and field(form, "signed") == ["true"]
            and field(form, "size") == ["8"]
            and field(form, "value-range") == [str(MIN_I64), str(MAX_I64)],
            "not a resolved signed i64 type")
    rank, = field(form, "rank")
    require(rank in ("4", "5"), "unexpected resolved i64 integer rank")
    return int(rank)


def interface_model(forms):
    properties = field(forms, "target-properties")
    require(field(forms, "data-model") == ["LP64"]
            and field(properties, "byte-bits") == ["8"]
            and field(properties, "integer-representation") == ["twos-complement"],
            "unsupported Cephyr extraction target data model")


def function_forms(forms):
    return [form for form in forms if isinstance(form, list) and form[:1] == ["function"]]


def declaration_sites(fn):
    for site in field(fn, "declarations"):
        require(site[:1] == ["site"], "unknown Cephyr declaration site")
        file, = field(site, "file")
        line, = field(site, "line")
        definition = [f for f in site[1:] if isinstance(f, list) and f[:1] == ["definition"]]
        require(definition in ([], [["definition", "true"]]) and int(line) > 0,
                "invalid Cephyr declaration site")
        yield Path(file).resolve(), int(line), bool(definition)


def signature(fn):
    typ, = field(fn, "type")
    require(typ[:1] == ["function-type"] and field(typ, "prototype") == ["true"]
            and field(typ, "variadic") == ["false"], "not a fixed prototype")
    result, = field(typ, "result")
    return result, field(typ, "parameters")


def interface_definition(fn, header, arity):
    require(field(fn, "definition") == ["defined"]
            and field(fn, "linkage") == ["external"]
            and field(fn, "storage") == ["none", "none"]
            and not field(fn, "specifiers"), "unsupported linkage or storage")
    params = tuple(identifier(name) for name in field(fn, "parameter-names"))
    require(len(params) == arity and len(set(params)) == len(params), "invalid parameters")
    result, param_types = signature(fn)
    require(len(param_types) == len(params), "parameter type count mismatch")
    rank = i64_type(result)
    require(all(i64_type(t) == rank for t in param_types), "inconsistent resolved integer types")

    lines = []
    declarations = 0
    for file, line, definition in declaration_sites(fn):
        require(file == header, "not exclusive to its canonical header")
        if definition:
            lines.append(line)
        else:
            declarations += 1
    require(len(lines) == 1 and declarations, "missing canonical definition or declaration")
    return lines[0], params, rank


def interface_exports(forms, header, exports):
    interface_model(forms)
    functions = function_forms(forms)
    definitions = {}
    for spec in exports.values():
        symbol = spec["symbol"]
        require(symbol not in definitions, f"duplicate reviewed symbol {symbol}")
        matches = [fn for fn in functions if fn[1:2] == [symbol]]
        require(len(matches) == 1, f"expected one resolved declaration for {symbol}")
        try:
            definitions[symbol] = interface_definition(matches[0][2:], header, spec["arity"])
        except Rejection as error:
            raise Rejection(f"{symbol}: {error}") from error
    return definitions


def index_ast_definitions(document):
    ast = document["ast"]
    require(document["schema_version"] == 1 and ast["kind"] == "list",
            "unsupported Cephyr AST schema")
    markers = document["source"]["line_map"]
    physical = [entry["physical"] for entry in markers]
    require(physical and all(a < b for a, b in zip(physical, physical[1:])),
            "invalid Cephyr AST source map")
    found = {}
    paths = {}
    for fn in ast["items"]:
        if fn["kind"] != "function_decl" or fn["children"]["body"] is None:
            continue
        children = fn["children"]
        symbol = children["name"]["attributes"]["name"]
        index = bisect_right(physical, fn["line"]) - 1
        require(index >= 0, f"missing AST origin for {symbol}")
        origin = markers[index]
        file = origin["file"]
        if file not in paths:
            paths[file] = Path(file).resolve()
        line = origin["line"] + fn["line"] - origin["physical"]
        found.setdefault(symbol, []).append((paths[file], line, fn))
    return found


def ast_body(matches, header, symbol, definition):
    line, params, rank = definition
    require(len(matches) == 1 and matches[0][:2] == (header, line),
            f"unmatched canonical AST definition for {symbol}")
    fn = matches[0][2]
    children = fn["children"]
    require(fn.get("attributes") == {"prototype": True, "variadic": False}
            and not fn.get("flags"), f"unsupported AST function declaration for {symbol}")
    param_nodes = items(children["parameters"])
    require(tuple(p["children"]["name"]["attributes"]["name"] for p in param_nodes) == params,
            f"AST and resolved parameters differ for {symbol}")
    ast_types = [children["type"]["children"]["type"]]
    ast_types += [p["children"]["type"] for p in param_nodes]
    require(all(node["kind"] == "integer_type" and not node.get("flags")
                and node["attributes"]["rank"] == rank
                and node["attributes"]["signed"] is True for node in ast_types),
            f"AST and resolved types differ for {symbol}")
    return children["body"]


def ast_definitions(document, header, definitions):
    index = index_ast_definitions(document)
    return {symbol: ast_body(index.get(symbol, []), header, symbol, definition)
            for symbol, definition in definitions.items()}


def items(node):
    require(node["kind"] == "list", "expected Cephyr AST list")
    return node.get("items", [])


@dataclass(frozen=True)
class Expr:
    value: str | int | tuple
    kind: str


class Lower:
    def __init__(self, params):
        self.params = {name: f"${index}" for index, name in enumerate(params)}
        self.remaining = NODE_LIMIT

    def visit(self, node, depth, *attributes):
        self.remaining -= 1
        require(depth <= DEPTH_LIMIT and self.remaining >= 0, "C body exceeds portable profile budget")
        require(not node.get("flags") and set(node.get("attributes", ())) <= set(attributes),
                f"unsupported C AST flags or attributes on {node['kind']}")
        return node.get("children", {})

    def expression(self, node, depth):
        kind = node["kind"]
        attrs = node.get("attributes", {})
        ch = self.visit(node, depth, *{
            "identifier": ("name",), "integer": ("spelling", "value"),
            "subtract": ("unary",),
        }.get(kind, ()))

        def expr(key):
            return self.expression(ch[key], depth + 1)

        if kind == "identifier":
            name = attrs["name"]
            require(name in self.params, f"non-parameter C expression: {name!r}")
            return Expr(self.params[name], "i64")
        if kind == "integer":
            # In the reviewed LP64 model int is 32 bits and long/long long
            # are 64 bits. Do not erase unsignedness, radix, or C promotions.
            spelling = re.fullmatch(r"(0|[1-9][0-9]*)([lL]|ll|LL)?", attrs["spelling"])
            require(spelling is not None, f"unsupported C integer literal {attrs['spelling']!r}")
            value = int(attrs["value"])
            require(0 <= value <= MAX_I64 and str(value) == spelling[1], "invalid C integer literal")
            return Expr(value, "i64" if spelling[2] or value >= 1 << 31 else "i32")
        if kind == "conditional":
            cond, yes, no = expr("condition"), expr("then"), expr("else")
            require(cond.kind == "bool" and yes.kind == no.kind == "i64",
                    "C conditional must have a boolean test and i64 branches")
            return Expr(("if", cond.value, yes.value, no.value), "i64")
        if kind in COMPARISONS or kind in LOGIC or kind in ARITHMETIC:
            left, right = expr("lhs"), expr("rhs")
            if kind in LOGIC:
                require(left.kind == right.kind == "bool", "non-boolean C logical operands")
                return Expr((LOGIC[kind], left.value, right.value), "bool")
            require(left.kind in ("i32", "i64") and right.kind in ("i32", "i64"),
                    "non-integer C arithmetic or comparison operands")
            if kind in COMPARISONS:
                return Expr((COMPARISONS[kind], left.value, right.value), "bool")
            wide = "i64" in (left.kind, right.kind)
            if attrs.get("unary"):
                # Cephyr represents unary minus as synthetic 0 minus rhs.
                require(left.value == 0 and type(right.value) is int, "unsupported C unary minus")
            if type(left.value) is int and type(right.value) is int:
                value = left.value + right.value if kind == "add" else left.value - right.value
                low, high = (MIN_I64, MAX_I64) if wide else (-(1 << 31), (1 << 31) - 1)
                require(low <= value <= high, "overflow in C constant expression")
                return Expr(value, "i64" if wide else "i32")
            require(wide, "non-i64 C arithmetic")
            return Expr((ARITHMETIC[kind], left.value, right.value), "i64")
        raise Rejection(f"unsupported C expression: {kind}")

    def statement(self, node, continuation=None, depth=0):
        ch = self.visit(node, depth)
        kind = node["kind"]
        if kind == "compound_stmt":
            for statement in reversed(items(ch["items"])):
                continuation = self.statement(statement, continuation, depth + 1)
            return continuation
        if kind == "return":
            result = self.expression(ch["expr"], depth + 1)
            require(result.kind in ("i32", "i64"), "C return value must be signed integer")
            return result.value
        if kind == "if":
            cond = self.expression(ch["condition"], depth + 1)
            require(cond.kind == "bool", "C if test must be boolean")
            yes = self.statement(ch["then"], continuation, depth + 1)
            no = (self.statement(ch["else"], continuation, depth + 1)
                  if ch["else"] is not None else continuation)
            require(yes is not None and no is not None, "C body may fall through without a return")
            return ("if", cond.value, yes, no)
        raise Rejection(f"unsupported C statement: {kind}")


def reviewed_expression(tree, arity, budget, depth=0, *, tail=False):
    """Validate and normalize a reviewed guard/body without interpreting source text."""
    budget[0] -= 1
    require(depth <= DEPTH_LIMIT and budget[0] >= 0, "reviewed expression exceeds budget")
    if type(tree) is int:
        require(MIN_I64 <= tree <= MAX_I64, "reviewed literal outside i64")
        return Expr(tree, "i64")
    if tree == "true":
        return Expr(tree, "bool")
    if isinstance(tree, str):
        require(tree in {f"${index}" for index in range(arity)}, "unknown reviewed parameter")
        return Expr(tree, "i64")
    require(isinstance(tree, (list, tuple)) and tree and isinstance(tree[0], str),
            "invalid bounded expression")
    op, *args = tree
    infos = [reviewed_expression(arg, arity, budget, depth + 1, tail=op == "if" and index > 0)
             for index, arg in enumerate(args)]
    kinds = [info.kind for info in infos]
    if op == "if":
        require(tail and kinds == ["bool", "i64", "i64"], "invalid reviewed if expression")
        result = "i64"
    elif op in LOGIC.values():
        require(kinds == ["bool", "bool"], "invalid reviewed logical expression")
        result = "bool"
    else:
        require(kinds == ["i64", "i64"] and op in {*COMPARISONS.values(), *ARITHMETIC.values()},
                "invalid reviewed arithmetic or comparison expression")
        result = "i64" if op in ARITHMETIC.values() else "bool"
    return Expr((op, *(info.value for info in infos)), result)


def render(expr, params):
    if isinstance(expr, tuple):
        return "(" + " ".join(render(item, params) for item in expr) + ")"
    if isinstance(expr, str) and expr.startswith("$"):
        return params[int(expr[1:])]
    return str(expr)


def cephyr_result(result, stage):
    require(result.ok, f"Cephyr {stage} failed:\n{result.diagnostics.decode('utf-8', 'replace').strip()}")
    require(len(result.output) <= DOCUMENT_LIMIT, f"Cephyr {stage} exceeds output budget")
    return result.output.decode("utf-8")


def cephyr_inputs(config, include, header, switch, target):
    args = ["--config", config, "--stdlib=cephyr", "--no-factir", "-I", include]
    if target is not None:
        args += ["--target", target]
    args += ["-"]
    define = f"#define {switch}\n" if switch is not None else ""
    return args, f"{define}#include <{header.name}>\n"


def canonical_header(include, kernel):
    header = include / f"mulib-{kernel}.h"
    require(header.resolve(strict=True) == header and header.is_file(),
            "canonical header must be a regular file, not an alias")
    return header


def conventional_switch(kernel):
    return "MULIB_IMPL_" + kernel.upper().replace("-", "_").replace("+", "P")


def generate(compiler, config, include, kernel, review, *, target=None):
    require(KERNEL.fullmatch(kernel) and review["header"] == f"mulib-{kernel}.h",
            "invalid reviewed kernel or noncanonical header name")
    header = canonical_header(include, kernel)
    header_bytes = header.read_bytes()
    sha256 = hashlib.sha256(header_bytes).hexdigest()
    require(sha256 == review["sha256"], f"{header.name}: header SHA-256 differs from reviewed pin")
    exports = review["exports"]
    require(exports, "reviewed profile has no exports")
    args, source = cephyr_inputs(config, include, header, conventional_switch(kernel), target)
    ast = json.loads(cephyr_result(compiler.parse_c11(args, source), "C11 AST"))
    interface = read_interface(cephyr_result(compiler.emit_interface(args, source), "resolved interface"))
    require(header.read_bytes() == header_bytes, f"{header.name}: header changed during extraction")
    definitions = interface_exports(interface, header, exports)
    bodies = ast_definitions(ast, header, definitions)

    lines = [f"(ffispec 1 (kernel {kernel}) (header {header.name})", f"  (sha256 {sha256})"]
    for member, spec in exports.items():
        symbol = identifier(spec["symbol"])
        _, params, _ = definitions[symbol]
        try:
            body = Lower(params).statement(bodies[symbol])
            expected = reviewed_expression(spec["body"], len(params), [NODE_LIMIT], tail=True)
            require(expected.kind == "i64" and expected.value == body, "C body differs from reviewed expression")
            guard = reviewed_expression(spec["guard"], len(params), [NODE_LIMIT])
            require(guard.kind == "bool", "reviewed guard is not boolean")
        except Rejection as error:
            raise Rejection(f"{symbol}: {error}") from error
        param_form = " ".join(("params", *params))
        lines += [f"  (export {identifier(member)} {symbol} ({param_form})",
                  f"    (guard {render(guard.value, params)})",
                  f"    (body {render(body, params)})",
                  "    (hint kernel-hot (target x86_64-unknown-linux-gnu) (speculative))",
                  "    (hint private-code (backend mir-c11))",
                  "    (rewrite fold-identity (speculative)))"]
    return "\n".join([*lines, ")", ""])


INPUT_ERRORS = (Rejection, ImportError, OSError, RuntimeError, ValueError, LookupError, TypeError)


def diagnostic(code, detail, *, position=None):
    entry = {"code": code, "detail": detail}
    if position is not None:
        entry["position"] = position
    return entry


def type_rejection(typ, position, tags):
    try:
        i64_type(typ)
    except Rejection:
        head = typ[0] if isinstance(typ, list) and typ else typ
        if head == "qualified":
            nested = type_rejection(typ[-1], position, tags)
            if nested is not None:
                return nested
        if head == "pointer":
            category = " ".join(field(typ, "category"))
            return diagnostic("unsupported-pointer", f"{category} pointer", position=position)
        if head == "tag-ref" and len(typ) == 2 and typ[1] in tags:
            kind, name = tags[typ[1]]
            if kind in ("struct", "union"):
                return diagnostic("unsupported-record", f"{kind} {name}", position=position)
            return diagnostic("unsupported-type", f"{kind} {name}", position=position)
        if head == "integer":
            bits, = field(typ, "bits")
            signed, = field(typ, "signed")
            meaning = "signed" if signed == "true" else "unsigned"
            return diagnostic("unsupported-type", f"{meaning} {bits}-bit integer", position=position)
        return diagnostic("unsupported-type", f"{head} is not a plain signed i64", position=position)
    return None


def project_exports(interface, ast_index, header):
    interface_model(interface)
    tags = {form[1]: form[2:4] for form in interface
            if isinstance(form, list) and form[:1] == ["tag"]}
    exports = []
    for _, symbol, *fn in sorted(function_forms(interface), key=lambda form: form[1]):
        sites = list(declaration_sites(fn))
        local = [site for site in sites if site[0] == header]
        if not local or field(fn, "linkage") != ["external"]:
            continue  # Not a public C function belonging to this header.
        reasons = []
        if not any(site[2] for site in local):
            reasons.append(diagnostic("no-implementation", "no definition in this header under the projection profile"))
        try:
            result, params = signature(fn)
        except Rejection as error:
            reasons.append(diagnostic("unsupported-profile", str(error)))
        else:
            positions = ["result", *(f"parameter[{i}]" for i in range(len(params)))]
            for typ, position in zip([result, *params], positions):
                problem = type_rejection(typ, position, tags)
                if problem is not None:
                    reasons.append(problem)
            if not reasons:
                try:
                    definition = interface_definition(fn, header, len(params))
                    body = ast_body(ast_index.get(symbol, []), header, symbol, definition)
                except Rejection as error:
                    reasons.append(diagnostic("unsupported-profile", str(error)))
                else:
                    try:
                        lower = Lower(definition[1]).statement(body)
                        checked = reviewed_expression(lower, len(params), [NODE_LIMIT], tail=True)
                        require(checked.kind == "i64", "body does not return i64")
                    except Rejection as error:
                        reasons.append(diagnostic("unsupported-body", str(error)))
        exports.append({
            "symbol": symbol,
            "declaration_lines": sorted({line for _, line, definition in local if not definition}),
            "definition_lines": sorted({line for _, line, definition in local if definition}),
            "eligibility": "rejected" if reasons else "shape-compatible",
            "reasons": reasons,
        })
    return exports


def project(compiler, config, include, kernel, *, target=None):
    projection = {"status": "failed"}

    def report_error(stage, error):
        # Paths underneath the checkout should not make inventories differ
        # merely because the repository was cloned into another directory.
        detail = str(error).replace(str(include.parent) + "/", "")
        projection.setdefault("errors", []).append(diagnostic(stage, detail))

    try:
        require(HEADER_ID.fullmatch(kernel), "invalid canonical header name for projection")
        header = canonical_header(include, kernel)
        data = header.read_bytes()
        observed = {m.decode("ascii") for line in CONDITIONAL.findall(data) for m in IMPL_MACRO.findall(line)}
        conventional = conventional_switch(kernel)
        if conventional in observed:
            switch = conventional
        elif len(observed) == 1:
            switch = next(iter(observed))
        else:
            switch = conventional if observed else None
        projection["implementation_macro"] = switch
        projection["source_sha256"] = hashlib.sha256(data).hexdigest()
        args, source = cephyr_inputs(config, include, header, switch, target)
    except INPUT_ERRORS as error:
        report_error("input", error)
        return projection

    stages = [
        ("c11-ast", compiler.parse_c11, lambda text: index_ast_definitions(json.loads(text))),
        ("resolved-interface", compiler.emit_interface, read_interface),
    ]
    results = {}
    for stage, invoke, decode in stages:
        try:
            results[stage] = decode(cephyr_result(invoke(args, source), stage))
        except INPUT_ERRORS as error:
            report_error(stage, error)
    if projection.get("errors"):
        return projection

    try:
        require(header.read_bytes() == data, "canonical header changed during projection")
        exports = project_exports(results["resolved-interface"], results["c11-ast"], header)
    except INPUT_ERRORS as error:
        report_error("export-inventory", error)
        return projection
    projection.update(status="projected", exports=exports)
    return projection


def batch(open_compiler, config, include, profiles, *, target=None, project_headers=False, selected=None):
    headers = {path.name[6:-2]: path.name for path in include.glob("mulib-*.h")}
    available = headers.keys() | profiles.keys()
    kernels = available if not selected else set(selected)
    require(kernels <= available, f"unknown batch kernel(s): {', '.join(sorted(kernels - available))}")
    entries = []
    failed = False
    compiler, compiler_error = None, None
    if kernels & headers.keys() and (project_headers or kernels & headers.keys() & profiles.keys()):
        try:
            compiler = open_compiler()
        except INPUT_ERRORS as error:
            compiler_error = str(error)
    for kernel in sorted(kernels):
        header = headers.get(kernel, f"mulib-{kernel}.h")
        entry = {"kernel": kernel, "header": header, "status": "unavailable"}
        if kernel not in profiles:
            detail = ("Projection is diagnostic only; no reviewed recipe exists." if project_headers else
                      "Discovered header; C11 extraction and translation were not attempted without review.")
            entry.update(reason="no-reviewed-profile", detail=detail)
        elif kernel not in headers:
            failed = True
            entry.update(reason="missing-header", detail="Reviewed canonical header was not discovered.")
        elif compiler_error is not None:
            failed = True
            entry.update(reason="cephyr-unavailable", detail=compiler_error)
        else:
            try:
                spec = generate(compiler, config, include, kernel, profiles[kernel], target=target)
                entry.update(status="admitted", reason="reviewed-i64",
                             exports=list(profiles[kernel]["exports"]),
                             spec_sha256=hashlib.sha256(spec.encode("utf-8")).hexdigest())
            except INPUT_ERRORS as error:
                failed = True
                entry.update(reason="profile-rejected", detail=str(error))
        if project_headers:
            if kernel not in headers:
                projection = {"status": "failed", "errors": [diagnostic("input", entry["detail"])]}
            elif compiler_error is not None:
                projection = {"status": "failed", "errors": [diagnostic("cephyr-unavailable", compiler_error)]}
            else:
                projection = project(compiler, config, include, kernel, target=target)
            failed |= projection["status"] == "failed"
            entry["projection"] = projection
        entries.append(entry)
    admitted = sum(entry["status"] == "admitted" for entry in entries)
    report = {
        "schema_version": 1,
        "discovered_headers": len(headers),
        "reviewed_profiles": len(profiles),
        "admitted": admitted,
        "unavailable": len(entries) - admitted,
        "kernels": entries,
    }
    if project_headers:
        projected = sum(entry["projection"]["status"] == "projected" for entry in entries)
        report.update(projected=projected, failed_projection=len(entries) - projected)
    if selected:
        report["selected_kernels"] = len(entries)
    return report, int(failed)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    selection = parser.add_mutually_exclusive_group(required=True)
    selection.add_argument("kernel", nargs="?", help="reviewed MuLIB kernel ID (e.g. clamp)")
    selection.add_argument("--batch", action="store_true", help="report every canonical MuLIB header as JSON")
    parser.add_argument("--project", action="store_true", help="project selected batch headers with Cephyr without admission")
    parser.add_argument("--select", action="append", metavar="KERNEL", help="limit batch to this kernel (repeatable)")
    parser.add_argument("--cephyr-library", type=Path, help="path to libcephyr.so; default: CEPHYR_LIBRARY or loader search")
    parser.add_argument("--config", type=Path, default=CEPHYR_CONFIG, help="Cephyr driver configuration")
    parser.add_argument("--target", help="override Cephyr extraction target (LP64 only)")
    output = parser.add_mutually_exclusive_group()
    output.add_argument("-o", "--output", type=Path, help="write the portable .ffi file (default: stdout)")
    output.add_argument("--check", type=Path, help="fail if the named .ffi file differs from fresh generation")
    options = parser.parse_args()
    if options.batch and (options.check is not None or options.output is not None):
        parser.error("--batch reports to stdout; --check and --output require a kernel")
    if not options.batch and (options.project or options.select):
        parser.error("--project and --select require --batch")

    def open_compiler():
        from cephyr import Compiler
        return Compiler(options.cephyr_library)

    try:
        profiles = json.loads((HERE / "reviewed.json").read_text(encoding="utf-8"))
        include = (ROOT / "kernels").resolve()
        if options.batch:
            report, status = batch(open_compiler, options.config, include, profiles, target=options.target,
                                   project_headers=options.project, selected=options.select)
            print(json.dumps(report, indent=2))
            return status
        require(options.kernel in profiles, f"no reviewed portable profile for kernel {options.kernel!r}")
        spec = generate(open_compiler(), options.config, include,
                        options.kernel, profiles[options.kernel], target=options.target)
        if options.check is not None:
            require(options.check.read_bytes() == spec.encode("utf-8"), f"stale generated spec: {options.check}")
        elif options.output is not None:
            options.output.write_bytes(spec.encode("utf-8"))
        else:
            sys.stdout.write(spec)
    except INPUT_ERRORS as error:
        print(f"ffispec-gen: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
