"""Positive-only foreign evidence from Cephyr's interface and syntax projections."""

import tempfile

from cephyr_interface import (
    ArrayType, FunctionType, InterfaceError, PointerType, QualifiedType, TagRef, _Atom, parse_sexp,
)
from project_interface import ProjectionError, _capture


def unqualify(typ):
    while isinstance(typ, QualifiedType):
        typ = typ.type
    return typ


def callback_pointer(typ):
    typ = unqualify(typ)
    return isinstance(typ, PointerType) and isinstance(unqualify(typ.pointee), FunctionType)


def declaration_evidence(function, tags):
    """Follow resolved types and reachable tags, not C spelling or parameter names."""
    facts, visited = set(), set()
    if "noreturn" in function.specifiers:
        facts.add("noreturn")
    pending = [function.type]
    remaining = 4096
    while pending and remaining:
        remaining -= 1
        typ = unqualify(pending.pop())
        if isinstance(typ, FunctionType):
            if typ.variadic:
                facts.add("variadic")
            if not typ.prototype:
                facts.add("no_prototype")
            pending.extend((*typ.parameters, typ.result))
        elif isinstance(typ, PointerType):
            if callback_pointer(typ):
                facts.add("taint_function_pointer")
            pending.append(typ.pointee)
        elif isinstance(typ, ArrayType):
            pending.append(typ.element)
        elif isinstance(typ, TagRef) and typ.id not in visited:
            visited.add(typ.id)
            pending.extend(field.type for field in tags[typ.id].fields)
    return sorted(facts), not pending


def _value(form):
    """Decode the bounded Cephyr C11Syntax v1 map/array wire representation."""
    if type(form) is _Atom and form in ("true", "false", "null"):
        return {"true": True, "false": False, "null": None}[form]
    if type(form) in (str, int):
        return form
    if isinstance(form, list) and form and type(form[0]) is _Atom:
        if form[0] == "array":
            return [_value(value) for value in form[1:]]
        if form[0] == "object":
            result = {}
            for pair in form[1:]:
                if (not isinstance(pair, list) or len(pair) != 2 or type(pair[0]) is not str
                        or pair[0] in result):
                    break
                result[pair[0]] = _value(pair[1])
            else:
                return result
    raise InterfaceError("syntax_schema", "invalid Cephyr C11Syntax map, array, or scalar")


def _kind(node):
    return node.get("kind") if isinstance(node, dict) else None


def _child(node, name):
    children = node.get("children") if isinstance(node, dict) else None
    return children.get(name) if isinstance(children, dict) else None


def _name(node):
    if _kind(node) == "identifier" and isinstance(node.get("attributes"), dict):
        return node["attributes"].get("name")
    return None


def _items(node):
    if _kind(node) == "list" and isinstance(node.get("items"), list):
        return node["items"]
    return ()


def callback_sites(raw, functions):
    """Match an unconditional first-statement return-call through a typed parameter.

    No intervening scopes, shadowing, control flow, or assigned values are
    modeled. Require identifier arguments bound to parameters only, so an
    argument expression cannot alter the callee or prevent reaching the call.
    The caller must take both projections from the same controlled Cephyr TU.
    """
    root = parse_sexp(raw, max_string_bytes=4 * 1024 * 1024)
    if len(root) != 2 or type(root[0]) is not int or root[0] != 1:
        raise InterfaceError("syntax_schema", "expected Cephyr C11Syntax v1")
    document = _value(root[1])
    if (not isinstance(document, dict) or set(document) != {"ast", "source"}
            or _kind(document["ast"]) != "list"
            or not isinstance(document["ast"].get("items"), list)):
        raise InterfaceError("syntax_schema", "expected Cephyr C11Syntax translation unit")
    result = {}
    for node in _items(document["ast"]):
        if _kind(node) != "function_decl":
            continue
        symbol = _name(_child(node, "name"))
        if not isinstance(symbol, str) or symbol not in functions:
            continue
        function = functions[symbol]
        body = _child(node, "body")
        if (_kind(body) != "compound_stmt" or function.definition == "declaration"
                or not function.type.prototype or function.type.variadic):
            continue
        params = _items(_child(node, "parameters"))
        if not params or not all(_kind(p) == "parameter_decl" for p in params):
            continue
        names = tuple(_name(_child(p, "name")) for p in params)
        if (len(names) != len(function.type.parameters) or not all(isinstance(n, str) for n in names)
                or len(set(names)) != len(names)):
            continue
        statements = _items(_child(body, "items"))
        if not statements or _kind(statements[0]) != "return":
            continue
        call = _child(statements[0], "expr")
        if _kind(call) != "call":
            continue
        callee = _child(call, "callee")
        if (_kind(callee) == "indirect_ref" and isinstance(callee.get("children"), dict)
                and set(callee["children"]) == {"expr", "field"} and callee["children"]["field"] is None):
            callee = _child(callee, "expr")
        name = _name(callee)
        children = call.get("children")
        if not isinstance(children, dict) or set(children) != {"callee", "arguments"}:
            continue
        arguments = children["arguments"]
        if arguments is not None and (_kind(arguments) != "list" or not isinstance(
                arguments.get("items"), list)):
            continue
        if (name not in names or not callback_pointer(function.type.parameters[names.index(name)])
                or any(_name(arg) not in names for arg in _items(arguments))):
            continue
        line = call.get("line")
        if type(line) is int and 1 <= line < 2**32:
            result[symbol] = {"parameter": names.index(name), "preprocessed_line": line}
    return result


def wetstone_evidence(provider, helper, functions, sites):
    """Use the pinned Wetstone pass through the isolated, bounded tool harness."""
    if not sites:
        return {}
    symbols = sorted(sites)
    payload = "".join(f"{len(functions[s].type.parameters)} {sites[s]['parameter']}\n"
                      for s in symbols).encode("ascii")
    provider._verify_tool_files()
    with tempfile.TemporaryDirectory(prefix="on1x-wetstone-", dir=provider.scratch_root) as cwd:
        status, out, _ = _capture((str(provider.prlimit),
                                  f"--as={provider.limits.address_space_bytes}",
                                  "--core=0", "--", str(helper)), payload, cwd, provider.limits)
    provider._verify_tool_files()
    lines = out.splitlines(keepends=True)
    if (status != 0 or len(lines) != len(symbols)
            or any(line not in (b"tainted\n", b"not_established\n") for line in lines)):
        raise ProjectionError("wetstone_failed", "invalid or incomplete Wetstone body evidence")
    return {symbol: sites[symbol] for symbol, line in zip(symbols, lines, strict=True)
            if line == b"tainted\n"}
