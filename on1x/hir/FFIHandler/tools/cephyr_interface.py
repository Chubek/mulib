"""Decode Cephyr C11Interface v1 as bounded, description-only data.

Based on LibStone e10ac69e, Cephyr/docs/C11Interface.md and the corresponding
v1 serializer. This is neither a C resolver nor evidence of callable ABI,
source origin, or safe admission. Numeric ``unknown`` and name ``none`` are
represented by None in their respective typed slots.
"""
from __future__ import annotations

from dataclasses import dataclass
import json
import re


# These are per projection, independent of any subprocess/output limit. The
# tree and all walks are bounded even when the caller did not spawn Cephyr.
MAX_BYTES = 4 * 1024 * 1024
MAX_TOKENS = 400_000
MAX_NODES = 300_000
MAX_DEPTH = 96
MAX_STRING_BYTES = 4096  # encoded payload, including escape characters
MAX_ATOM_BYTES = 64
MAX_LIST_ITEMS = 16_384
MAX_DECLARATIONS = 4096
MAX_TAGS = 4096
MAX_MEMBERS = 4096
MAX_PARAMETERS = 1024
MAX_REDECLARATIONS = 4096
_U64_MAX = (1 << 64) - 1
_I64_MIN = -(1 << 63)
_U32_MAX = (1 << 32) - 1


class InterfaceError(ValueError):
    """An invalid interface, with a bounded message and machine-readable code."""

    def __init__(self, code: str, message: str):
        super().__init__(message)
        self.code = code


@dataclass(frozen=True, slots=True)
class Layout:
    size: int | None
    align: int | None


@dataclass(frozen=True, slots=True)
class VoidType:
    pass


@dataclass(frozen=True, slots=True)
class IntegerType:
    name: str
    rank: int
    bits: int
    signed: bool
    size: int
    align: int


@dataclass(frozen=True, slots=True)
class RealType:
    name: str
    format: str
    bits: int
    size: int
    align: int


@dataclass(frozen=True, slots=True)
class PointerType:
    bits: int
    size: int
    align: int
    pointee: CType


@dataclass(frozen=True, slots=True)
class ArrayType:
    count: int | None
    size: int | None
    align: int | None
    element: CType


@dataclass(frozen=True, slots=True)
class FunctionType:
    prototype: bool
    variadic: bool
    result: CType
    parameters: tuple[CType, ...]


@dataclass(frozen=True, slots=True)
class QualifiedType:
    qualifiers: tuple[str, ...]
    type: CType


@dataclass(frozen=True, slots=True)
class TagRef:
    id: int


CType = (
    VoidType | IntegerType | RealType | PointerType | ArrayType
    | FunctionType | QualifiedType | TagRef
)


@dataclass(frozen=True, slots=True)
class Function:
    name: str
    type: FunctionType
    linkage: str
    storage: tuple[str, ...]
    definition: str
    calling_convention: str
    parameter_names: tuple[str | None, ...]
    specifiers: tuple[str, ...]


@dataclass(frozen=True, slots=True)
class Object:
    name: str
    type: CType
    linkage: str
    storage: tuple[str, ...]
    definition: str
    layout: Layout


@dataclass(frozen=True, slots=True)
class Typedef:
    name: str
    type: CType


@dataclass(frozen=True, slots=True)
class BitField:
    width: int
    shift: int
    container_offset: int


@dataclass(frozen=True, slots=True)
class Field:
    name: str | None
    offset: int
    type: CType
    bit_field: BitField | None
    flexible_array: bool


@dataclass(frozen=True, slots=True)
class Enumerator:
    name: str
    value: int


@dataclass(frozen=True, slots=True)
class Tag:
    id: int
    kind: str
    name: str | None
    complete: bool
    layout: Layout | None
    fields: tuple[Field, ...]
    underlying: IntegerType | None
    enumerators: tuple[Enumerator, ...]


@dataclass(frozen=True, slots=True)
class Interface:
    version: int
    target: str
    data_model: str
    abi: str
    declarations: tuple[Function | Object | Typedef, ...]
    tags: tuple[Tag, ...]

    @property
    def functions(self) -> tuple[Function, ...]:
        return tuple(decl for decl in self.declarations if isinstance(decl, Function))


class _Atom(str):
    """Keep atom tokens distinct from the exact str type used for JSON strings."""


_Node = list["_Node"] | _Atom | str | int
_SPACE = re.compile(rb"[ \t\r\n]+")
_ATOM = re.compile(rb"[A-Za-z_][A-Za-z_0-9-]{0,%d}" % (MAX_ATOM_BYTES - 1))
_INTEGER = re.compile(rb"-?[0-9]{1,20}")
_DELIMITERS = b" \t\r\n()"


def _syntax(message: str, offset: int) -> InterfaceError:
    return InterfaceError("syntax", f"{message} at byte {offset}")


def _limit(message: str) -> InterfaceError:
    return InterfaceError("limit", message)


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise InterfaceError("schema", message)


def _quoted(data: bytes, start: int, max_string_bytes: int) -> tuple[str, int]:
    pos = start + 1
    while pos < len(data) and pos - start <= max_string_bytes + 1:
        char = data[pos]
        if char == 34:  # "
            try:
                text = json.loads(data[start:pos + 1].decode("utf-8"))
                # JSON's grammar admits escaped UTF-16 units; accept pairs
                # but not lone surrogates that cannot be expressed in UTF-8.
                text.encode("utf-8")
            except ValueError:
                raise _syntax("invalid UTF-8 or JSON string", start) from None
            return text, pos + 1
        pos += 2 if char == 92 else 1  # skip escaped byte while finding the quote
    if pos - start > max_string_bytes + 1:
        raise _limit("string byte limit exceeded")
    raise _syntax("unterminated string", start)


def parse_sexp(data: bytes, *, max_bytes=MAX_BYTES, max_tokens=MAX_TOKENS,
               max_nodes=MAX_NODES, max_depth=MAX_DEPTH,
               max_string_bytes=MAX_STRING_BYTES) -> list[_Node]:
    """Shared bounded lexer; interface and syntax callers enforce their own schemas."""
    if not isinstance(data, bytes):
        raise TypeError("S-expression input must be bytes")
    if len(data) > max_bytes:
        raise _limit("S-expression byte limit exceeded")

    pos = tokens = nodes = 0
    root = None
    stack: list[list[_Node]] = []
    while pos < len(data):
        ws = _SPACE.match(data, pos)
        if ws:
            pos = ws.end()
            continue
        if root is not None and not stack:
            raise _syntax("trailing form", pos)
        tokens += 1
        if tokens > max_tokens:
            raise _limit("token limit exceeded")
        char = data[pos]
        if char == 41:  # )
            if not stack:
                raise _syntax("unexpected closing parenthesis", pos)
            stack.pop()
            pos += 1
            continue
        nodes += 1
        if nodes > max_nodes:
            raise _limit("node limit exceeded")
        if stack and len(stack[-1]) >= MAX_LIST_ITEMS:
            raise _limit("list item limit exceeded")
        if char == 40:  # (
            if len(stack) >= max_depth:
                raise _limit("list depth limit exceeded")
            child: list[_Node] = []
            if stack:
                stack[-1].append(child)
            else:
                root = child
            stack.append(child)
            pos += 1
            continue
        if not stack:
            raise _syntax("expected a root list", pos)
        start = pos
        if char == 34:
            value, pos = _quoted(data, start, max_string_bytes)
        elif match := _INTEGER.match(data, pos):
            pos = match.end()
            if pos < len(data) and 48 <= data[pos] <= 57:
                raise _limit("decimal digit limit exceeded")
            value = int(match[0])
            if not _I64_MIN <= value <= _U64_MAX:
                raise _limit("decimal integer outside 64-bit bounds")
        elif match := _ATOM.match(data, pos):
            pos = match.end()
            if pos < len(data) and pos - start == MAX_ATOM_BYTES and (
                data[pos] in b"-_" or 48 <= data[pos] <= 57
                or 65 <= data[pos] <= 90 or 97 <= data[pos] <= 122
            ):
                raise _limit("atom byte limit exceeded")
            value = _Atom(match[0].decode("ascii"))
        else:
            raise _syntax("invalid token", pos)
        if pos < len(data) and data[pos] not in _DELIMITERS:
            raise _syntax("missing token delimiter or invalid token", pos)
        stack[-1].append(value)
    if stack or root is None:
        raise _syntax("missing or unclosed root list", pos)
    return root


def _head(node: _Node) -> str:
    _require(isinstance(node, list) and bool(node) and type(node[0]) is _Atom,
             "expected a list headed by an atom")
    return node[0]


def _clauses(nodes, required, optional=(), *, repeat=None):
    """Split a closed set of singleton clauses and one optional repeated kind."""
    found, repeated = {}, []
    for node in nodes:
        key = _head(node)
        if key == repeat:
            if len(repeated) >= MAX_MEMBERS:
                raise _limit("tag member count limit exceeded")
            repeated.append(node)
            continue
        _require(key in required or key in optional, "unknown clause")
        _require(key not in found, "duplicate singleton clause")
        found[key] = node
    _require(all(key in found for key in required), "missing required clause")
    return found, repeated


def _single(clauses, key):
    node = clauses[key]
    _require(len(node) == 2, f"wrong arity for {key} clause")
    return node[1]


def _atom(node, choices, where: str) -> str:
    _require(type(node) is _Atom and node in choices, f"invalid {where} atom")
    return str(node)


def _is_atom(node, spelling: str) -> bool:
    return type(node) is _Atom and node == spelling


def _bool(node) -> bool:
    return _atom(node, ("true", "false"), "boolean") == "true"


def _string(node) -> str:
    _require(type(node) is str and bool(node), "expected a nonempty quoted string")
    return node


def _name(node) -> str | None:
    return None if _is_atom(node, "none") else _string(node)


def _number(node, low=0, high=_U64_MAX) -> int:
    _require(type(node) is int and low <= node <= high, "invalid numeric field")
    return node


def _known(node, high=_U64_MAX) -> int | None:
    return None if _is_atom(node, "unknown") else _number(node, 1, high)


def _layout_values(clauses) -> Layout:
    size = _known(_single(clauses, "size"))
    align = _known(_single(clauses, "align"), _U32_MAX)
    _require(align is None or align & (align - 1) == 0, "alignment is not a power of two")
    return Layout(size, align)


def _layout(node) -> Layout:
    clauses, _ = _clauses(node[1:], ("size", "align"))
    return _layout_values(clauses)


# Constraints of the documented v1 built-in target pairs, not mappings from
# source C. Every field is read from the wire and checked, not synthesized.
# target: (ABI, convention, plain-char sign, long-double format and width)
_TARGETS = {
    "x86_64-linux-gnu": ("SysVAMD64", "sysv", True, "x87", 80),
    "aarch64-linux-gnu": ("AAPCS64", "aapcs64", False, "binary128", 128),
    "riscv64-linux-gnu": ("LP64", "lp64", False, "binary128", 128),
}
_INTEGERS = {
    "bool": (0, 1, False),
    "char": (1, 8, None),
    "signed-char": (1, 8, True), "unsigned-char": (1, 8, False),
    "short": (2, 16, True), "unsigned-short": (2, 16, False),
    "int": (3, 32, True), "unsigned-int": (3, 32, False),
    "long": (4, 64, True), "unsigned-long": (4, 64, False),
    "long-long": (5, 64, True), "unsigned-long-long": (5, 64, False),
}


def _type(node, target: str) -> CType:
    # This recursion only follows syntactic children, at most MAX_DEPTH;
    # layout and tag-graph validation below never recursively follow IDs.
    if _is_atom(node, "void"):
        return VoidType()
    kind = _head(node)
    if kind == "integer":
        _require(len(node) >= 2, "integer type lacks a name")
        name = _atom(node[1], _INTEGERS, "integer type name")
        clauses, _ = _clauses(node[2:], ("rank", "bits", "signed", "size", "align"))
        rank = _number(_single(clauses, "rank"), high=5)
        bits = _number(_single(clauses, "bits"), 1, 64)
        signed = _bool(_single(clauses, "signed"))
        layout = _layout_values(clauses)
        native_rank, native_bits, native_signed = _INTEGERS[name]
        if native_signed is None:
            native_signed = _TARGETS[target][2]
        size = (bits + 7) // 8
        _require((rank, bits, signed) == (native_rank, native_bits, native_signed)
                 and layout == Layout(size, size), "invalid integer representation for target")
        return IntegerType(name, rank, bits, signed, layout.size, layout.align)
    if kind == "real":
        _require(len(node) >= 2, "real type lacks a name")
        name = _atom(node[1], ("float", "double", "long-double"), "real type name")
        clauses, _ = _clauses(node[2:], ("format", "bits", "size", "align"))
        fmt = _atom(_single(clauses, "format"), ("binary32", "binary64", "x87", "binary128"),
                    "real format")
        bits = _number(_single(clauses, "bits"), 1, 128)
        layout = _layout_values(clauses)
        if name == "float":
            expected = ("binary32", 32, Layout(4, 4))
        elif name == "double":
            expected = ("binary64", 64, Layout(8, 8))
        else:
            expected = (*_TARGETS[target][3:], Layout(16, 16))
        _require((fmt, bits, layout) == expected, "invalid real representation for target")
        return RealType(name, fmt, bits, layout.size, layout.align)
    if kind == "pointer":
        clauses, _ = _clauses(node[1:], ("bits", "size", "align", "pointee"))
        bits = _number(_single(clauses, "bits"))
        layout = _layout_values(clauses)
        _require(bits == 64 and layout == Layout(8, 8), "invalid LP64 pointer representation")
        return PointerType(bits, layout.size, layout.align, _type(_single(clauses, "pointee"), target))
    if kind == "array":
        clauses, _ = _clauses(node[1:], ("count", "size", "align", "element"))
        count = _known(_single(clauses, "count"))
        layout = _layout_values(clauses)
        return ArrayType(count, layout.size, layout.align, _type(_single(clauses, "element"), target))
    if kind == "function-type":
        clauses, _ = _clauses(node[1:], ("prototype", "variadic", "result", "parameters"))
        prototype = _bool(_single(clauses, "prototype"))
        variadic = _bool(_single(clauses, "variadic"))
        params = clauses["parameters"]
        if len(params) - 1 > MAX_PARAMETERS:
            raise _limit("function parameter count limit exceeded")
        _require(prototype or len(params) == 1, "unprototyped function has a parameter list")
        _require(not variadic or (prototype and len(params) > 1), "invalid variadic function type")
        result = _type(_single(clauses, "result"), target)
        _require(not isinstance(result, (ArrayType, FunctionType, QualifiedType)),
                 "invalid function result shape")
        parameters = tuple(_type(param, target) for param in params[1:])
        _require(all(not isinstance(param, (VoidType, ArrayType, FunctionType, QualifiedType))
                     for param in parameters), "invalid adjusted parameter shape")
        return FunctionType(prototype, variadic, result, parameters)
    if kind == "qualified":
        _require(len(node) == 3 and _head(node[1]) == "qualifiers", "invalid qualified type shape")
        names = node[1]
        _require(2 <= len(names) <= 4, "invalid qualifier count")
        qualifiers = tuple(_atom(n, ("const", "volatile", "restrict"), "qualifier")
                           for n in names[1:])
        _require(len(set(qualifiers)) == len(qualifiers), "duplicate qualifier")
        child = _type(node[2], target)
        _require(not isinstance(child, (QualifiedType, FunctionType)), "invalid qualified type shape")
        if "restrict" in qualifiers:
            _require(isinstance(child, PointerType) and not isinstance(_base(child.pointee), FunctionType),
                     "invalid restrict-qualified type")
        return QualifiedType(qualifiers, child)
    if kind == "tag-ref":
        _require(len(node) == 2, "invalid tag-ref arity")
        return TagRef(_number(node[1]))
    raise InterfaceError("schema", "unknown type form")


def _field(node, target: str) -> Field:
    _require(len(node) >= 2, "field lacks a name slot")
    name = _name(node[1])
    clauses, _ = _clauses(node[2:], ("offset", "type"), ("bit-field", "flexible-array"))
    offset = _number(_single(clauses, "offset"))
    bits = None
    if "bit-field" in clauses:
        bit, _ = _clauses(clauses["bit-field"][1:], ("width", "shift", "container-offset"))
        bits = BitField(_number(_single(bit, "width"), high=64),
                        _number(_single(bit, "shift"), high=7),
                        _number(_single(bit, "container-offset")))
    flexible = "flexible-array" in clauses
    if flexible:
        _require(_bool(_single(clauses, "flexible-array")), "flexible-array marker must be true")
    return Field(name, offset, _type(_single(clauses, "type"), target), bits, flexible)


def _tag(node, target: str) -> Tag:
    _require(len(node) >= 4, "tag lacks ID, kind or name slot")
    tag_id = _number(node[1])
    kind = _atom(node[2], ("struct", "union", "enum"), "tag kind")
    name = _name(node[3])
    if kind == "enum":
        clauses, raw_members = _clauses(node[4:], ("complete",), ("underlying",), repeat="enumerator")
    else:
        clauses, raw_members = _clauses(node[4:], ("complete",), ("layout",), repeat="field")
    if not _bool(_single(clauses, "complete")):
        _require(len(clauses) == 1 and not raw_members and name is not None,
                 "incomplete tag has body data or no name")
        return Tag(tag_id, kind, name, False, None, (), None, ())
    _require(bool(raw_members), "complete tag lacks members")
    if kind == "enum":
        _require("underlying" in clauses, "complete enum lacks an underlying type")
        underlying = _type(_single(clauses, "underlying"), target)
        _require(isinstance(underlying, IntegerType), "enum underlying type must be integer")
        members = []
        names = set()
        for member in raw_members:
            _require(len(member) == 3, "invalid enumerator arity")
            member_name = _string(member[1])
            _require(member_name not in names, "duplicate enumerator")
            names.add(member_name)
            # The v1 serializer uses signed decimal for enumerators. C11
            # enumerators are int; also reject values outside the declared
            # underlying representation's domain.
            low = -(1 << (underlying.bits - 1)) if underlying.signed else 0
            high = (1 << (underlying.bits - underlying.signed)) - 1
            value = _number(member[2], max(low, -(1 << 31)), min(high, (1 << 31) - 1))
            members.append(Enumerator(member_name, value))
        return Tag(tag_id, kind, name, True, None, (), underlying, tuple(members))
    _require("layout" in clauses, "complete record lacks layout")
    layout = _layout(clauses["layout"])
    _require(layout.size is not None and layout.align is not None and layout.size % layout.align == 0,
             "invalid complete record layout")
    return Tag(tag_id, kind, name, True, layout,
               tuple(_field(field, target) for field in raw_members), None, ())


def _declaration(node, target: str) -> Function | Object | Typedef:
    kind = _head(node)
    _require(len(node) >= 2, "declaration lacks a name")
    name = _string(node[1])
    if kind == "typedef":
        clauses, _ = _clauses(node[2:], ("type",))
        return Typedef(name, _type(_single(clauses, "type"), target))
    symbol = ("type", "linkage", "storage", "definition")
    if kind == "object":
        clauses, _ = _clauses(node[2:], (*symbol, "layout"))
        definitions = ("declaration", "tentative", "defined")
    else:
        clauses, _ = _clauses(node[2:], (*symbol, "calling-convention", "parameter-names", "specifiers"))
        definitions = ("declaration", "defined", "inline-only")
    type_ = _type(_single(clauses, "type"), target)
    linkage = _atom(_single(clauses, "linkage"), ("internal", "external"), "linkage")
    definition = _atom(_single(clauses, "definition"), definitions, "definition")
    storage_node = clauses["storage"]
    if len(storage_node) - 1 > MAX_REDECLARATIONS:
        raise _limit("redeclaration count limit exceeded")
    _require(len(storage_node) > 1, "missing storage history")
    storage = tuple(_atom(s, ("none", "static", "extern"), "storage class") for s in storage_node[1:])
    _require(("static" in storage) == (linkage == "internal"), "storage/linkage mismatch")
    if kind == "object":
        _require(not isinstance(_base(type_), (VoidType, FunctionType)), "invalid object type shape")
        return Object(name, type_, linkage, storage, definition, _layout(clauses["layout"]))
    _require(isinstance(type_, FunctionType), "function declaration lacks a function type")
    convention = _single(clauses, "calling-convention")
    if not _is_atom(convention, _TARGETS[target][1]):
        raise InterfaceError("target", "calling convention does not match target")
    names = clauses["parameter-names"]
    _require(len(names) - 1 == len(type_.parameters), "parameter-name arity mismatch")
    raw_specifiers = clauses["specifiers"]
    _require(len(raw_specifiers) <= 3, "invalid function specifier count")
    specifiers = tuple(_atom(s, ("inline", "noreturn"), "function specifier")
                       for s in raw_specifiers[1:])
    _require(len(set(specifiers)) == len(specifiers), "duplicate function specifier")
    _require(definition != "inline-only"
             or (linkage == "external" and "inline" in specifiers and "extern" not in storage),
             "invalid inline-only definition")
    # Parameter names are presentation metadata. Preserve collisions as well
    # as unnamed slots; the generator must choose unique display names.
    return Function(name, type_, linkage, storage, definition, str(convention),
                    tuple(_name(n) for n in names[1:]), specifiers)


def _base(type_: CType) -> CType:
    return type_.type if isinstance(type_, QualifiedType) else type_


def _type_layout(type_: CType, tags: dict[int, Tag]) -> Layout:
    type_ = _base(type_)
    if isinstance(type_, (IntegerType, RealType, PointerType, ArrayType)):
        return Layout(type_.size, type_.align)
    if isinstance(type_, TagRef):
        tag = tags[type_.id]
        if tag.underlying is not None:
            return Layout(tag.underlying.size, tag.underlying.align)
        if tag.layout is not None:
            return tag.layout
    return Layout(None, None)


def _record_order(tags: dict[int, Tag]) -> list[Tag]:
    # Edges only follow by-value fields through arrays/qualifiers. Pointer
    # pointees (including signatures) were checked separately for references.
    edges: dict[int, list[int]] = {}
    for tag in tags.values():
        refs = edges[tag.id] = []
        for field in tag.fields:
            type_ = _base(field.type)
            while isinstance(type_, ArrayType):
                type_ = _base(type_.element)
            if isinstance(type_, TagRef):
                refs.append(type_.id)
    state = {}  # 1: active; 2: finished (memoized across multiple parents)
    order = []
    for tag_id in edges:
        if tag_id in state:
            continue
        stack = [(tag_id, iter(edges[tag_id]))]
        state[tag_id] = 1
        while stack:
            current, children = stack[-1]
            child = next(children, None)
            if child is None:
                stack.pop()
                state[current] = 2
                order.append(tags[current])
            elif state.get(child) == 1:
                raise InterfaceError("schema", "cyclic by-value tag layout")
            elif child not in state:
                state[child] = 1
                stack.append((child, iter(edges[child])))
    return order


def _validate_record(tag: Tag, tags: dict[int, Tag], has_flexible: set[int], target: str) -> None:
    if tag.layout is None:
        return
    size, align = tag.layout.size, tag.layout.align
    names = set()
    visible_member = False
    end = 0  # bit extent of preceding fields; union alternatives may overlap
    for index, field in enumerate(tag.fields):
        if field.name is not None:
            _require(field.name not in names, "duplicate named record field")
            names.add(field.name)
        offset = field.offset
        _require(offset <= size and (tag.kind != "union" or offset == 0),
                 "field offset outside record layout")
        type_ = _base(field.type)
        natural = _type_layout(type_, tags)
        start = offset * 8
        bits = field.bit_field
        if bits is not None:
            _require(not field.flexible_array and isinstance(type_, IntegerType)
                     and type_.name in ("bool", "int", "unsigned-int"), "invalid bit-field base type")
            _require(bits.width <= type_.bits and (bits.width != 0 or field.name is None),
                     "invalid bit-field width")
            _require(bits.container_offset <= offset and bits.container_offset % type_.align == 0
                     and (offset - bits.container_offset) * 8 + bits.shift + bits.width <= type_.size * 8,
                     "invalid bit-field container")
            _require(tag.kind != "union" or bits.shift == 0, "invalid union bit-field shift")
            _require(bits.width != 0 or (bits.shift == 0 and offset == bits.container_offset),
                     "invalid zero-width bit-field placement")
            # An unnamed field can have a base type with alignment larger
            # than the record (SysV/RV64), and its full container need not
            # fit inside the record. Only its occupied bits must fit.
            _require((field.name is None and target != "aarch64-linux-gnu") or align >= type_.align,
                     "invalid bit-field alignment")
            start += bits.shift
            stop = start + bits.width
        else:
            _require(natural.align is not None and align >= natural.align
                     and offset % natural.align == 0, "invalid field alignment")
            if field.flexible_array:
                _require(tag.kind == "struct" and index == len(tag.fields) - 1 and visible_member
                         and field.name is not None and isinstance(type_, ArrayType)
                         and type_.count is None and type_.size is None, "invalid flexible array member")
                has_flexible.add(tag.id)
                stop = start
            else:
                _require(natural.size is not None, "record field is not a complete object")
                if isinstance(type_, TagRef) and type_.id in has_flexible:
                    _require(tag.kind == "union", "structure field contains a flexible array")
                    has_flexible.add(tag.id)
                stop = start + natural.size * 8
            if field.name is None:
                _require(isinstance(type_, TagRef) and tags[type_.id].layout is not None
                         and tags[type_.id].name is None, "invalid anonymous record field")
                visible_member = True
        visible_member |= field.name is not None
        _require(stop <= size * 8 and (tag.kind == "union" or start >= end),
                 "record field extent overlaps or exceeds its layout")
        end = stop
    _require(visible_member, "record lacks a named member")


def _validate_graph(declarations, tags: dict[int, Tag], target: str) -> None:
    # Visit syntax-tree types once without expanding nominal references.
    pending = [decl.type for decl in declarations]
    pending.extend(field.type for tag in tags.values() for field in tag.fields)
    arrays = []
    while pending:
        type_ = pending.pop()
        if isinstance(type_, TagRef):
            _require(type_.id in tags, "dangling tag reference")
        elif isinstance(type_, PointerType):
            pending.append(type_.pointee)
        elif isinstance(type_, ArrayType):
            arrays.append(type_)
            pending.append(type_.element)
        elif isinstance(type_, QualifiedType):
            pending.append(type_.type)
        elif isinstance(type_, FunctionType):
            pending.append(type_.result)
            pending.extend(type_.parameters)

    # Reject cycles before validating layout arithmetic. Validate records in
    # dependency order so flexible-array containment is also memoized by ID.
    has_flexible: set[int] = set()
    for tag in _record_order(tags):
        _validate_record(tag, tags, has_flexible, target)

    for type_ in arrays:
        element = _base(type_.element)
        natural = _type_layout(element, tags)
        _require(natural.size is not None and natural.align is not None
                 and (not isinstance(element, TagRef) or element.id not in has_flexible),
                 "invalid array element type")
        size = None if type_.count is None else type_.count * natural.size
        _require((size is None or size <= _U64_MAX) and type_.size == size
                 and type_.align == natural.align, "inconsistent array layout")

    for decl in declarations:
        if isinstance(decl, Object):
            natural = _type_layout(decl.type, tags)
            actual = decl.layout
            _require(actual.size == natural.size
                     and (natural.align is None or (actual.align is not None and actual.align >= natural.align))
                     and (decl.definition == "declaration" or actual.size is not None),
                     "inconsistent object layout")
        elif isinstance(decl, Function) and decl.definition != "declaration":
            signature = decl.type
            _require(isinstance(signature.result, VoidType) or _type_layout(signature.result, tags).size is not None,
                     "incomplete function definition result type")
            _require(all(_type_layout(parameter, tags).size is not None for parameter in signature.parameters),
                     "incomplete function definition parameter type")


def decode_interface(data: bytes, *, expected_target: str | None = None) -> Interface:
    """Decode a complete v1 document, rejecting unknown schema and target data.

    If supplied, ``expected_target`` is the caller's independently selected
    target. Omitting it still validates the document's built-in target, ABI
    and data model against one another.
    """
    root = parse_sexp(data, max_bytes=MAX_BYTES, max_tokens=MAX_TOKENS,
                      max_nodes=MAX_NODES, max_depth=MAX_DEPTH,
                      max_string_bytes=MAX_STRING_BYTES)
    _require(_head(root) == "interface", "expected C11Interface root")
    if len(root) < 2 or type(root[1]) is not int or root[1] != 1:
        raise InterfaceError("version", "unsupported C11Interface version")
    envelope, raw_tags, raw_declarations = [], [], []
    for item in root[2:]:
        kind = _head(item)
        if kind == "tag":
            if len(raw_tags) >= MAX_TAGS:
                raise _limit("tag count limit exceeded")
            raw_tags.append(item)
        elif kind in ("typedef", "function", "object"):
            if len(raw_declarations) >= MAX_DECLARATIONS:
                raise _limit("declaration count limit exceeded")
            raw_declarations.append(item)
        else:
            envelope.append(item)
    clauses, _ = _clauses(envelope, ("target", "data-model", "abi"))
    target = _string(_single(clauses, "target"))
    abi = _string(_single(clauses, "abi"))
    if (target not in _TARGETS or (expected_target is not None and target != expected_target)
            or abi != _TARGETS[target][0] or not _is_atom(_single(clauses, "data-model"), "LP64")):
        raise InterfaceError("target", "interface target, data model or ABI mismatch")

    tags = {}
    for raw in raw_tags:
        tag = _tag(raw, target)
        _require(tag.id not in tags, "duplicate tag ID")
        tags[tag.id] = tag
    declarations, names = [], set()
    for raw in raw_declarations:
        decl = _declaration(raw, target)
        _require(decl.name not in names, "duplicate ordinary declaration")
        names.add(decl.name)
        declarations.append(decl)
    _validate_graph(declarations, tags, target)
    return Interface(1, target, "LP64", abi, tuple(declarations), tuple(tags.values()))
