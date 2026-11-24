"""Extract the file-scope structural subset of a replayed Cephyr C11Syntax v1."""
from dataclasses import dataclass

from cephyr_interface import InterfaceError, parse_sexp


MAX_BYTES = 16 * 1024 * 1024  # The default provider's output budget.


@dataclass(frozen=True, slots=True)
class FunctionSyntax:
    name: str
    declarations: int
    has_body: bool


def _require(condition, message):
    if not condition:
        raise InterfaceError("syntax_schema", message)


def _object(value):
    _require(isinstance(value, list) and value and value[0] == "object"
             and type(value[0]) is not str, "expected C11Syntax object")
    fields = {}
    for field in value[1:]:
        _require(isinstance(field, list) and len(field) == 2 and type(field[0]) is str,
                 "expected C11Syntax object field")
        key, value = field
        _require(key not in fields, "duplicate C11Syntax object field")
        fields[key] = value
    return fields


def _array(value):
    _require(isinstance(value, list) and value and value[0] == "array"
             and type(value[0]) is not str, "expected C11Syntax array")
    return value[1:]


def _kind(value, expected):
    kind = _object(value).get("kind")
    return type(kind) is str and kind == expected


def declaration_syntax(data: bytes) -> tuple[FunctionSyntax, ...]:
    """Consume just direct declaration edges, not nested declarations or body contents.

    The provider must replay these same bytes through Cephyr for the full
    versioned schema, replay equality, and resolved interface. We do not use
    numeric parser flags, presumed source locations, or traversal of statements.
    """
    root = parse_sexp(data, max_bytes=MAX_BYTES, max_tokens=2_000_000, max_nodes=1_500_000,
                      max_depth=260, max_string_bytes=MAX_BYTES)
    if not root or type(root[0]) is not int or root[0] != 1:
        raise InterfaceError("syntax_version", "expected Cephyr C11Syntax version 1")
    _require(len(root) == 2, "invalid C11Syntax envelope")
    document = _object(root[1])
    _require(document.keys() == {"ast", "source"}, "invalid C11Syntax document fields")
    # Do not accept source labels as filesystem paths or include ownership.
    ast = _object(document["ast"])
    _require(type(ast.get("kind")) is str and ast["kind"] == "list", "expected file-scope syntax list")
    decls = {}
    for item in _array(ast.get("items")):
        node = _object(item)
        kind = node.get("kind")
        _require(type(kind) is str, "expected quoted C11Syntax node kind")
        if kind not in ("function_decl", "variable_decl"):
            continue
        children = _object(node.get("children"))
        if kind == "variable_decl":
            # Pinned resolve_file_declarations accepts a variable_decl whose
            # type node is a function_type (not a pointer to a function).
            if not _kind(children.get("type"), "function_type"):
                continue
            has_body = False
        else:
            value = children.get("body")
            _require(value is not None, "missing C11Syntax function body slot")
            if isinstance(value, list):
                _require(_kind(value, "compound_stmt"),
                         "C11Syntax function body is not a compound statement")
                has_body = True
            else:
                _require(value == "null" and type(value) is not str,
                         "invalid C11Syntax function body slot")
                has_body = False
        name = _object(children.get("name"))
        _require(name.get("kind") == "identifier", "expected C11Syntax function identifier")
        symbol = _object(name.get("attributes")).get("name")
        _require(type(symbol) is str and bool(symbol), "missing C11Syntax function name")
        count, has_definition = decls.get(symbol, (0, False))
        _require(not (has_definition and has_body), "duplicate C11Syntax function body")
        decls[symbol] = count + 1, has_definition or has_body
    return tuple(FunctionSyntax(name, *decls[name]) for name in sorted(decls))


def pair_with_interface(syntax: tuple[FunctionSyntax, ...], interface):
    """Cross-check structural evidence against the resolved same-TU projection."""
    functions = {fn.name: fn for fn in interface.functions}
    if len(syntax) != len(functions) or not all(
            (fn := functions.get(row.name)) is not None
            and len(fn.storage) == row.declarations
            and (fn.definition in ("defined", "inline-only")) == row.has_body
            for row in syntax):
        raise InterfaceError("syntax_mismatch",
                             "C11Syntax and C11Interface disagree on file-scope declarations")
    return {row.name: row for row in syntax}
