"""Read Cephyr's views and lower the bounded portable C11 recipe subset."""

from __future__ import annotations

from bisect import bisect_right
from dataclasses import dataclass
import json
from pathlib import Path
import re


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
CEPHYR_CONFIG = ROOT / "on1x/third_party/libstone/cornerstone/Cephyr/Cephyr.toml"
IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z0-9_]{0,63}\Z")
KERNEL = re.compile(r"[a-z][a-z0-9]*(?:-[a-z0-9]+)*\Z")
MIN_I64, MAX_I64 = -(1 << 63), (1 << 63) - 1
DEPTH_LIMIT = 32
NODE_LIMIT = 512
DOCUMENT_LIMIT = 8 * 1024 * 1024
SYNTAX_DEPTH_LIMIT = 256  # Cephyr v1's value nesting limit; object members add a wire list.
TOKEN_LIMIT = 1_500_000
COMPARISONS = {
    "less": "lt", "less_equal": "le", "greater": "gt",
    "greater_equal": "ge", "equal": "eq",
}
LOGIC = {"logical_and": "and", "logical_or": "or"}
ARITHMETIC = {"add": "add", "subtract": "sub"}
RESERVED = {"true", "false"}
SELECTOR = re.compile(r"[a-zA-Z_0-9][a-zA-Z_0-9.-]{0,127}\Z")
# Cephyr's interface v1 C layout targets (docs/C11Interface.md), not On1x MIR
# selectors. The latter are independent of the target used during extraction.
CALLING_CONVENTIONS = {
    "x86_64-linux-gnu": "sysv",
    "aarch64-linux-gnu": "aapcs64",
    "riscv64-linux-gnu": "lp64",
}


class Rejection(Exception):
    """Input cannot be admitted by the portable FFI profile."""


def require(condition, explanation):
    if not condition:
        raise Rejection(explanation)


def identifier(name):
    require(isinstance(name, str) and IDENTIFIER.fullmatch(name) and name not in RESERVED,
            f"unsupported identifier: {name!r}")
    return name


# Read Cephyr's two versioned S-expression formats, not C or FFI input.
TOKEN = re.compile(r'[ \t\r\n]+|[()]|"(?:[^"\\\x00-\x1f]|\\.)*"|[^()"\x00-\x20]+')
DECIMAL = re.compile(r"-?(?:0|[1-9][0-9]*)\Z")


class Quoted(str):
    """A JSON-quoted S-expression string, as distinct from a bare atom."""


def read_sexp(text, stage, depth_limit):
    require(len(text) <= DOCUMENT_LIMIT, f"Cephyr {stage} exceeds output budget")
    forms = [[]]
    position = 0
    budget = TOKEN_LIMIT
    while position < len(text):
        match = TOKEN.match(text, position)
        require(match is not None, f"malformed Cephyr {stage} token")
        token = match[0]
        position = match.end()
        if token[0] in " \t\r\n":
            continue
        budget -= 1
        require(budget >= 0, f"Cephyr {stage} exceeds token budget")
        if token == "(":
            require(len(forms) <= depth_limit, f"Cephyr {stage} exceeds depth limit")
            forms.append([])
        elif token == ")":
            require(len(forms) > 1, f"unbalanced Cephyr {stage}")
            form = forms.pop()
            forms[-1].append(form)
        elif token.startswith('"'):
            try:
                string = json.loads(token)
                string.encode("utf-8")  # Reject lone JSON surrogate escapes.
            except ValueError as error:
                raise Rejection(f"invalid Cephyr {stage} string escape") from error
            forms[-1].append(Quoted(string))
        else:
            forms[-1].append(token)
    require(len(forms) == 1 and len(forms[0]) == 1 and isinstance(forms[0][0], list),
            f"expected one Cephyr {stage} document")
    return forms[0][0]


def atoms(values, *expected):
    return len(values) == len(expected) and all(type(value) is str and value == want
                                               for value, want in zip(values, expected))


def tagged(form, tag):
    return isinstance(form, list) and atoms(form[:1], tag)


def read_interface(text):
    document = read_sexp(text, "interface", DEPTH_LIMIT)
    require(atoms(document[:2], "interface", "1"),
             "unsupported Cephyr interface schema")
    require(all(isinstance(form, list) and form and type(form[0]) is str for form in document[2:]),
            "invalid Cephyr interface form")
    return document[2:]


def ast_value(form, depth=0):
    require(depth <= SYNTAX_DEPTH_LIMIT, "Cephyr AST exceeds value nesting limit")
    if isinstance(form, list):
        if tagged(form, "array"):
            return [ast_value(item, depth + 1) for item in form[1:]]
        require(tagged(form, "object"), "expected Cephyr AST object or array")
        result = {}
        for member in form[1:]:
            require(isinstance(member, list) and len(member) == 2 and isinstance(member[0], Quoted),
                    "invalid Cephyr AST object member")
            key, value = member
            require(key not in result, f"duplicate Cephyr AST object member {key!r}")
            result[str(key)] = ast_value(value, depth + 1)
        return result
    if isinstance(form, Quoted):
        return str(form)
    if form in ("true", "false", "null"):
        return {"true": True, "false": False, "null": None}[form]
    require(len(form) <= 20 and DECIMAL.fullmatch(form), "invalid Cephyr AST scalar")
    value = int(form)
    require(MIN_I64 <= value <= MAX_I64, "Cephyr AST integer outside signed 64-bit range")
    return value


def coordinate(value):
    return type(value) is int and 1 <= value <= 0xffffffff


def validate_ast_node(node):
    # Check the v1 node envelope for every node. The portable subset checks
    # kind-specific fields before it uses a declaration, expression or statement.
    require(isinstance(node, dict) and node.keys() <= {"kind", "line", "flags", "children", "items", "attributes"}
            and isinstance(node.get("kind"), str) and node["kind"], "invalid Cephyr AST node")
    require(all(coordinate(node[key]) for key in ("line", "flags") if key in node),
            "invalid Cephyr AST line or flags")
    attributes = node.get("attributes", {})
    require(isinstance(attributes, dict)
            and all(type(value) in (str, bool, int)
                    or (isinstance(value, list)
                        and all(type(unit) is int and 0 <= unit <= 0xffffffff for unit in value))
                    for value in attributes.values()), "invalid Cephyr AST attributes")
    if node["kind"] == "list":
        require("children" not in node and isinstance(node.get("items"), list),
                "invalid Cephyr AST list")
        for item in node["items"]:
            validate_ast_node(item)
    else:
        children = node.get("children", {})
        require("items" not in node and isinstance(children, dict), "invalid Cephyr AST children")
        for child in children.values():
            if child is not None:
                validate_ast_node(child)


def physical_lines(text):
    return text.count("\n") + bool(text and not text.endswith("\n"))


def read_ast(text):
    # C11Syntax v1 encodes the same typed data model as JSON, with a leading
    # bare version and explicit (object ...) / (array ...) tags.
    document = read_sexp(text, "AST", 2 * SYNTAX_DEPTH_LIMIT + 4)
    require(len(document) == 2 and atoms(document[:1], "1"), "unsupported Cephyr AST schema")
    root = ast_value(document[1])
    require(isinstance(root, dict) and root.keys() == {"ast", "source"},
            "invalid Cephyr AST document")
    ast, source = root["ast"], root["source"]
    validate_ast_node(ast)
    require(ast["kind"] == "list", "Cephyr AST translation unit is not a list")
    require(isinstance(source, dict) and source.keys() == {"file", "text", "line_map"}
            and isinstance(source["file"], str) and source["file"] and "\0" not in source["file"]
            and isinstance(source["text"], str) and "\0" not in source["text"]
            and isinstance(source["line_map"], list), "invalid Cephyr AST source")
    physical = 0
    last_line = physical_lines(source["text"])
    for entry in source["line_map"]:
        require(isinstance(entry, dict) and entry.keys() == {"physical", "file", "line"}
                and isinstance(entry["file"], str) and "\0" not in entry["file"]
                and coordinate(entry["physical"]) and coordinate(entry["line"])
                and physical < entry["physical"] <= last_line
                and (physical != 0 or entry["physical"] == 1),
                "invalid Cephyr AST source map")
        physical = entry["physical"]
    root["schema_version"] = 1
    return root


def field(forms, tag):
    result = optional_field(forms, tag)
    require(result is not None, f"missing Cephyr interface field {tag}")
    return result


def optional_field(forms, tag):
    matches = [form for form in forms if isinstance(form, list) and form[:1] == [tag]]
    require(len(matches) <= 1, f"duplicate Cephyr interface field {tag}")
    if matches:
        require(tagged(matches[0], tag), f"invalid Cephyr interface field {tag}")
        return matches[0][1:]
    return None


def single_field(forms, tag):
    values = field(forms, tag)
    require(len(values) == 1, f"invalid Cephyr interface field {tag}")
    return values[0]


def i64_type(form):
    require(tagged(form, "integer") and atoms(field(form, "bits"), "64")
            and atoms(field(form, "signed"), "true")
            and atoms(field(form, "size"), "8")
            and atoms(field(form, "value-range"), str(MIN_I64), str(MAX_I64)),
            "not a resolved signed i64 type")
    rank = field(form, "rank")
    for number, name in ((4, "long"), (5, "long-long")):
        if atoms(rank, str(number)) and atoms(form[1:2], name):
            return number
    raise Rejection("unexpected resolved i64 integer rank")


def interface_target(forms):
    target = single_field(forms, "target")
    require(isinstance(target, Quoted) and SELECTOR.fullmatch(target) and target != "unknown",
             "invalid Cephyr extraction target")
    return str(target)


def interface_model(forms):
    target = interface_target(forms)
    properties = field(forms, "target-properties")
    require(target in CALLING_CONVENTIONS
            and atoms(field(forms, "data-model"), "LP64")
            and atoms(field(properties, "byte-bits"), "8")
            and atoms(field(properties, "integer-representation"), "twos-complement"),
             "unsupported Cephyr extraction target data model")
    return CALLING_CONVENTIONS[target]


def function_forms(forms):
    functions = [form for form in forms if tagged(form, "function")]
    require(all(len(form) >= 2 and isinstance(form[1], Quoted) for form in functions),
            "invalid Cephyr interface function name")
    return functions


def declaration_sites(fn):
    for site in field(fn, "declarations"):
        require(tagged(site, "site"), "unknown Cephyr declaration site")
        file = single_field(site, "file")
        line = single_field(site, "line")
        definition = optional_field(site, "definition")
        require(definition is None or atoms(definition, "true"), "invalid Cephyr declaration site")
        if atoms((file, line), "unknown", "unknown"):
            yield None, None, definition is not None
        else:
            require(isinstance(file, Quoted) and "\0" not in file and type(line) is str
                    and re.fullmatch(r"[1-9][0-9]{0,9}", line) and coordinate(int(line)),
                    "invalid Cephyr declaration site")
            # v1 records *presumed* source sites. Match the complete absolute
            # include path, not basenames, relative paths, or symlink aliases.
            yield str(file), int(line), definition is not None


def signature(fn):
    typ = single_field(fn, "type")
    require(tagged(typ, "function-type") and atoms(field(typ, "prototype"), "true")
            and atoms(field(typ, "variadic"), "false"), "not a fixed prototype")
    return single_field(typ, "result"), field(typ, "parameters")


def interface_definition(fn, header, convention):
    storage = field(fn, "storage")
    require(atoms(field(fn, "definition"), "defined")
            and atoms(field(fn, "linkage"), "external")
            and storage and all(atoms((value,), "none") or atoms((value,), "extern") for value in storage)
            and not field(fn, "specifiers"), "unsupported linkage or storage")
    require(atoms(field(fn, "calling-convention"), convention),
             "function calling convention does not match Cephyr extraction target")
    names = field(fn, "parameter-names")
    require(all(isinstance(name, Quoted) for name in names), "invalid parameter names")
    params = tuple(identifier(str(name)) for name in names)
    require(len(params) <= 128 and len(set(params)) == len(params), "invalid parameters")
    result, param_types = signature(fn)
    require(len(param_types) == len(params), "parameter type count mismatch")
    rank = i64_type(result)
    require(all(i64_type(t) == rank for t in param_types), "inconsistent resolved integer types")

    lines = []
    for file, line, definition in declaration_sites(fn):
        require(file == str(header), "not exclusive to its canonical header")
        if definition:
            lines.append(line)
    require(len(lines) == 1, "missing canonical definition")
    return lines[0], params, rank


def index_ast_definitions(document):
    source = document["source"]
    last_line = physical_lines(source["text"])
    markers = source["line_map"]
    physical = [entry["physical"] for entry in markers]
    found = {}
    for fn in items(document["ast"]):
        if fn["kind"] != "function_decl":
            continue
        children = fn.get("children", {})
        require("body" in children, "missing Cephyr AST function body field")
        if children["body"] is None:
            continue
        symbol = ast_name(children.get("name"))
        require("line" in fn and fn["line"] <= last_line, f"invalid AST line for {symbol}")
        index = bisect_right(physical, fn["line"]) - 1
        require(index >= 0, f"missing AST origin for {symbol}")
        origin = markers[index]
        line = origin["line"] + fn["line"] - origin["physical"]
        require(coordinate(line), f"invalid AST origin for {symbol}")
        found.setdefault(symbol, []).append((origin["file"], line, fn))
    return found


def ast_parts(node, kind, *, children=(), attributes=()):
    require(isinstance(node, dict) and node["kind"] == kind and not node.get("flags")
            and node.get("children", {}).keys() == set(children)
            and node.get("attributes", {}).keys() == set(attributes),
            f"unsupported AST shape, flags or attributes on {kind}")
    return node.get("children", {}), node.get("attributes", {})


def ast_name(node):
    _, attrs = ast_parts(node, "identifier", attributes=("name",))
    require(isinstance(attrs["name"], str), "invalid Cephyr AST name")
    return attrs["name"]


def ast_parameters(node):
    params = items(node)
    if len(params) == 1:
        param = params[0].get("children", {})
        if param.get("name") is None and (param.get("type") or {}).get("kind") == "void_type":
            ast_parts(params[0], "parameter_decl", children=("name", "type"))
            ast_parts(param["type"], "void_type")
            return [], []
    names, types = [], []
    for param in params:
        ch, _ = ast_parts(param, "parameter_decl", children=("name", "type"))
        names.append(ast_name(ch["name"]))
        types.append(ch["type"])
    return names, types


def ast_body(matches, header, symbol, definition):
    line, params, rank = definition
    require(len(matches) == 1 and matches[0][:2] == (str(header), line),
            f"unmatched canonical AST definition for {symbol}")
    fn = matches[0][2]
    children, attrs = ast_parts(fn, "function_decl", children=("name", "body", "parameters", "type"),
                                attributes=("prototype", "variadic"))
    typ, type_attrs = ast_parts(children["type"], "function_type", children=("type", "parameters"),
                               attributes=("prototype", "variadic"))
    require(all(a["prototype"] is True and a["variadic"] is False for a in (attrs, type_attrs)),
            f"unsupported AST function declaration for {symbol}")
    names, types = ast_parameters(children["parameters"])
    type_names, param_types = ast_parameters(typ["parameters"])
    require(tuple(names) == tuple(type_names) == params,
            f"AST and resolved parameters differ for {symbol}")
    for node in [typ["type"], *types, *param_types]:
        _, integer = ast_parts(node, "integer_type", attributes=("rank", "signed", "alignment", "plain_char"))
        require(integer["rank"] == rank and integer["signed"] is True and integer["plain_char"] is False
                and integer["alignment"] == 8, f"AST and resolved types differ for {symbol}")
    require(children["body"]["kind"] == "compound_stmt", f"unsupported AST function body for {symbol}")
    return children["body"]


def items(node):
    ast_parts(node, "list")
    return node["items"]


@dataclass(frozen=True)
class Expr:
    value: str | int | tuple
    kind: str


class Lower:
    def __init__(self, params):
        self.params = {name: f"${index}" for index, name in enumerate(params)}
        self.remaining = NODE_LIMIT

    def visit(self, node, depth):
        self.remaining -= 1
        require(depth <= DEPTH_LIMIT and self.remaining >= 0, "C body exceeds portable profile budget")
        require(isinstance(node, dict), "missing C AST body node")
        return node["kind"]

    def expression(self, node, depth):
        kind = self.visit(node, depth)
        if kind == "identifier":
            name = ast_name(node)
            require(name in self.params, f"non-parameter C expression: {name!r}")
            return Expr(self.params[name], "i64")
        if kind == "integer":
            # In the accepted LP64 model int is 32 bits and long/long long
            # are 64 bits. Do not erase unsignedness, radix, or C promotions.
            _, attrs = ast_parts(node, kind, attributes=("spelling", "value"))
            require(isinstance(attrs["spelling"], str) and isinstance(attrs["value"], str),
                    "invalid C integer literal")
            spelling = re.fullmatch(r"(0|[1-9][0-9]*)([lL]|ll|LL)?", attrs["spelling"])
            require(spelling is not None, f"unsupported C integer literal {attrs['spelling']!r}")
            require(len(attrs["value"]) <= 19 and attrs["value"] == spelling[1], "invalid C integer literal")
            value = int(attrs["value"])
            require(value <= MAX_I64, "invalid C integer literal")
            return Expr(value, "i64" if spelling[2] or value >= 1 << 31 else "i32")
        if kind == "conditional":
            ch, _ = ast_parts(node, kind, children=("condition", "then", "else"))
            cond, yes, no = (self.expression(ch[key], depth + 1) for key in ("condition", "then", "else"))
            require(cond.kind == "bool" and yes.kind in ("i32", "i64") and no.kind in ("i32", "i64"),
                     "C conditional must have a boolean test and signed integer branches")
            return Expr(("if", cond.value, yes.value, no.value),
                        "i64" if "i64" in (yes.kind, no.kind) else "i32")
        if kind in COMPARISONS or kind in LOGIC or kind in ARITHMETIC:
            ch, attrs = ast_parts(node, kind, children=("lhs", "rhs"),
                                  attributes=("unary",) if kind == "subtract" else ())
            require(type(attrs.get("unary", False)) is bool, "invalid C unary-minus flag")
            left, right = (self.expression(ch[key], depth + 1) for key in ("lhs", "rhs"))
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
                require(left.value == 0, "invalid synthetic C unary minus")
            if type(left.value) is int and type(right.value) is int:
                value = left.value + right.value if kind == "add" else left.value - right.value
                low, high = (MIN_I64, MAX_I64) if wide else (-(1 << 31), (1 << 31) - 1)
                require(low <= value <= high, "overflow in C constant expression")
                return Expr(value, "i64" if wide else "i32")
            require(wide, "non-i64 C arithmetic")
            return Expr((ARITHMETIC[kind], left.value, right.value), "i64")
        raise Rejection(f"unsupported C expression: {kind}")

    def statement(self, node, continuation=None, depth=0):
        kind = self.visit(node, depth)
        if kind == "compound_stmt":
            ch, _ = ast_parts(node, kind, children=("items",))
            for statement in reversed(items(ch["items"])):
                continuation = self.statement(statement, continuation, depth + 1)
            return continuation
        if kind == "return":
            ch, _ = ast_parts(node, kind, children=("expr",))
            result = self.expression(ch["expr"], depth + 1)
            require(result.kind in ("i32", "i64"), "C return value must be signed integer")
            return result.value
        if kind == "if":
            ch, _ = ast_parts(node, kind, children=("condition", "then", "else"))
            cond = self.expression(ch["condition"], depth + 1)
            require(cond.kind == "bool", "C if test must be boolean")
            yes = self.statement(ch["then"], continuation, depth + 1)
            no = (self.statement(ch["else"], continuation, depth + 1)
                  if ch["else"] is not None else continuation)
            require(yes is not None and no is not None, "C body may fall through without a return")
            return ("if", cond.value, yes, no)
        raise Rejection(f"unsupported C statement: {kind}")


def recipe_expression(tree, arity, budget, depth=0, *, tail=False):
    """Check the expanded tree against ffispec v1's own bounds and types."""
    budget[0] -= 1
    require(depth <= DEPTH_LIMIT and budget[0] >= 0, "portable expression exceeds budget")
    if type(tree) is int:
        require(MIN_I64 <= tree <= MAX_I64, "portable literal outside i64")
        return Expr(tree, "i64")
    if tree == "true":
        return Expr(tree, "bool")
    if isinstance(tree, str):
        require(tree in {f"${index}" for index in range(arity)}, "unknown portable parameter")
        return Expr(tree, "i64")
    require(isinstance(tree, (list, tuple)) and tree and isinstance(tree[0], str),
            "invalid bounded expression")
    op, *args = tree
    infos = [recipe_expression(arg, arity, budget, depth + 1, tail=op == "if" and index > 0)
             for index, arg in enumerate(args)]
    kinds = [info.kind for info in infos]
    if op == "if":
        require(tail and kinds == ["bool", "i64", "i64"], "invalid portable if expression")
        result = "i64"
    elif op in LOGIC.values():
        require(kinds == ["bool", "bool"], "invalid portable logical expression")
        result = "bool"
    else:
        require(kinds == ["i64", "i64"] and op in {*COMPARISONS.values(), *ARITHMETIC.values()},
                 "invalid portable arithmetic or comparison expression")
        result = "i64" if op in ARITHMETIC.values() else "bool"
    return Expr((op, *(info.value for info in infos)), result)


def render(expr, params):
    if isinstance(expr, tuple):
        return "(" + " ".join(render(item, params) for item in expr) + ")"
    if isinstance(expr, str) and expr.startswith("$"):
        return params[int(expr[1:])]
    return str(expr)
