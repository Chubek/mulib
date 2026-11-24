"""Decode Cephyr C11Interface v1 and its additive analysis model 1.

Based on LibStone e10ac69e and 6a7d52e8, Cephyr/docs/C11Interface.md and the
corresponding serializers. This describes a projection; it does not verify a
body, source origin, callable ABI, or safe admission. Unknown numeric fields
and absent names are None; status atoms such as ``unknown`` remain explicit.
"""
from __future__ import annotations

from dataclasses import asdict, dataclass
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
_PROOF_PARAMETER_LIMIT = 8
_PROOF_ACCESS_LIMIT = 24  # At most this many expression nodes were modeled.
_PROOF_DEFINITION_LIMIT = 128
_PROOF_WIDTHS = (8, 16, 31)
_PROOF_MODEL = "closed-unsigned-expression-v2"
_PROOF_SCOPE = "this-definition"
_PROOF_STATUS = ("proved", "disproved", "unknown")
_QUALIFIERS = ("const", "volatile", "restrict")


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
class AnalysisModel:
    version: int
    scope: tuple[str, ...]
    facts: str
    unknown: str


@dataclass(frozen=True, slots=True)
class TargetProperties:
    byte_bits: int
    byte_order: str
    pointer_bits: int
    integer_representation: str
    hosted: bool


@dataclass(frozen=True, slots=True)
class ValueRange:
    minimum: int
    maximum: int


@dataclass(frozen=True, slots=True)
class RecordTraits:
    const_member: bool
    volatile_member: bool
    contains_anonymous_member: bool
    contains_flexible_array: bool
    contains_bit_field: bool


@dataclass(frozen=True, slots=True)
class ArrayAccess:
    """Caller obligation from one array parameter declaration, not a buffer fact."""

    minimum_elements: int | None


@dataclass(frozen=True, slots=True)
class SiteParameter:
    index: int
    adjusted_from: str | None
    storage: str | None
    variable_bound: bool
    requires_array_access: ArrayAccess | None
    adjusted_pointer_qualifiers: tuple[str, ...]


@dataclass(frozen=True, slots=True)
class DeclarationSite:
    """Presumed file/line labels; these are not verified source paths."""

    file: str | None
    line: int | None
    definition: bool
    parameters: tuple[SiteParameter, ...]

    def portable_data(self) -> dict:
        """Describe the site structurally without its presumed location."""
        return {
            "definition": self.definition,
            "parameters": [
                {**asdict(parameter),
                 "adjusted_pointer_qualifiers": list(parameter.adjusted_pointer_qualifiers)}
                for parameter in self.parameters
            ],
        }


@dataclass(frozen=True, slots=True)
class CallContract:
    normal_return: str
    fixed_parameters: int | None
    variadic_tail: bool | None


@dataclass(frozen=True, slots=True)
class Behavior:
    general_body_analysis: str
    memory_read: str
    memory_write: str
    allocation: str
    deallocation: str
    throws_or_traps: str
    synchronization: str
    termination: str
    return_alias: str
    return_nullability: str
    taint_inputs: str
    taint_outputs: str
    taint_transfer: str


@dataclass(frozen=True, slots=True)
class MemorySafety:
    status: str
    model: str | None = None
    scope: str | None = None
    checked_accesses: int | None = None
    solver: str | None = None


@dataclass(frozen=True, slots=True)
class ReturnDependency:
    index: int
    status: str


@dataclass(frozen=True, slots=True)
class ReturnPolicy:
    width: int
    status: str


@dataclass(frozen=True, slots=True)
class BodyProof:
    """Bounded claims about this translation unit's definition only."""

    model: str
    scope: str
    memory_effects: str
    return_dependencies: tuple[ReturnDependency, ...]
    return_policies: tuple[ReturnPolicy, ...]
    solver: str


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
    value_range: ValueRange | None = None


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
    category: str | None = None


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
    declaration_sites: tuple[DeclarationSite, ...] = ()
    call_contract: CallContract | None = None
    behavior: Behavior | None = None
    memory_safety: MemorySafety | None = None
    body_proof: BodyProof | None = None

    def portable_analysis_data(self) -> dict | None:
        """JSON-recursive analysis data; callers bind the symbol, target and domain.

        This is separate from native representation identity and carries no
        presumed file/line labels, provenance, or authority.
        """
        if self.call_contract is None:
            return None
        proof = self.body_proof
        return {
            "declaration_sites": [site.portable_data() for site in self.declaration_sites],
            "call_contract": asdict(self.call_contract),
            "behavior": asdict(self.behavior),
            "memory_safety": asdict(self.memory_safety),
            "body_proof": None if proof is None else {
                **asdict(proof),
                "return_dependencies": [asdict(dep) for dep in proof.return_dependencies],
                "return_policies": [asdict(policy) for policy in proof.return_policies],
            },
        }


@dataclass(frozen=True, slots=True)
class Object:
    name: str
    type: CType
    linkage: str
    storage: tuple[str, ...]
    definition: str
    layout: Layout
    declaration_sites: tuple[DeclarationSite, ...] = ()
    storage_duration: str | None = None


@dataclass(frozen=True, slots=True)
class Typedef:
    name: str
    type: CType
    declaration_sites: tuple[DeclarationSite, ...] = ()


@dataclass(frozen=True, slots=True)
class BitField:
    width: int
    shift: int
    container_offset: int
    value_range: ValueRange | None = None


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
    traits: RecordTraits | None = None


@dataclass(frozen=True, slots=True)
class Interface:
    version: int
    target: str
    data_model: str
    abi: str
    declarations: tuple[Function | Object | Typedef, ...]
    tags: tuple[Tag, ...]
    analysis_model: AnalysisModel | None = None
    target_properties: TargetProperties | None = None

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


def _clauses(nodes, required, optional=(), *, repeat=None, repeat_limit=None,
             repeat_subject="tag member"):
    """Split a closed set of singleton clauses and one optional repeated kind."""
    found, repeated = {}, []
    for node in nodes:
        key = _head(node)
        if key == repeat:
            if len(repeated) >= (MAX_MEMBERS if repeat_limit is None else repeat_limit):
                raise _limit(f"{repeat_subject} count limit exceeded")
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


def _analysis_model(node) -> AnalysisModel:
    if len(node) < 2 or type(node[1]) is not int or node[1] != 1:
        raise InterfaceError("version", "unsupported C11Interface analysis-model version")
    clauses, _ = _clauses(node[2:], ("scope", "facts", "unknown"))
    scope = clauses["scope"]
    _require(len(scope) == 3 and _is_atom(scope[1], "declarations")
             and _is_atom(scope[2], "bounded-return-proof"), "unsupported analysis-model scope")
    return AnalysisModel(node[1], (str(scope[1]), str(scope[2])),
                         _atom(_single(clauses, "facts"), ("c11-frontend",), "fact model"),
                         _atom(_single(clauses, "unknown"), ("not-a-guarantee",), "unknown policy"))


def _target_properties(node) -> TargetProperties:
    clauses, _ = _clauses(node[1:], ("byte-bits", "byte-order", "pointer-bits",
                                    "integer-representation", "hosted"))
    byte_bits = _single(clauses, "byte-bits")
    pointer_bits = _single(clauses, "pointer-bits")
    byte_order = _single(clauses, "byte-order")
    representation = _single(clauses, "integer-representation")
    if (type(byte_bits) is not int or byte_bits != 8
            or type(pointer_bits) is not int or pointer_bits != 64
            or not _is_atom(byte_order, "little") or not _is_atom(representation, "twos-complement")):
        raise InterfaceError("target", "target properties do not match the interface target")
    return TargetProperties(byte_bits, str(byte_order), pointer_bits, str(representation),
                            _bool(_single(clauses, "hosted")))


def _value_range(node, bits: int, signed: bool) -> ValueRange | None:
    if bits == 0:
        _require(len(node) == 2 and _is_atom(node[1], "unknown"),
                 "zero-width bit-field range must be unknown")
        return None
    _require(len(node) == 3, "invalid value-range arity")
    minimum, maximum = _number(node[1], _I64_MIN), _number(node[2])
    magnitude = 1 << (bits - signed)
    _require((minimum, maximum) == (-magnitude if signed else 0, magnitude - 1),
             "value range does not match integer representation")
    return ValueRange(minimum, maximum)


def _qualifiers(node) -> tuple[str, ...]:
    _require(2 <= len(node) <= 4, "invalid qualifier count")
    qualifiers = tuple(_atom(n, _QUALIFIERS, "qualifier") for n in node[1:])
    _require(len(set(qualifiers)) == len(qualifiers), "duplicate qualifier")
    return qualifiers


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


def _type(node, target: str, analysis: bool) -> CType:
    # This recursion only follows syntactic children, at most MAX_DEPTH;
    # layout and tag-graph validation below never recursively follow IDs.
    if _is_atom(node, "void"):
        return VoidType()
    kind = _head(node)
    if kind == "integer":
        _require(len(node) >= 2, "integer type lacks a name")
        name = _atom(node[1], _INTEGERS, "integer type name")
        clauses, _ = _clauses(node[2:], ("rank", "bits", "signed", "size", "align")
                              + (("value-range",) if analysis else ()))
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
        range_ = _value_range(clauses["value-range"], bits, signed) if analysis else None
        return IntegerType(name, rank, bits, signed, layout.size, layout.align, range_)
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
        clauses, _ = _clauses(node[1:], ("bits", "size", "align", "pointee")
                              + (("category",) if analysis else ()))
        bits = _number(_single(clauses, "bits"))
        layout = _layout_values(clauses)
        _require(bits == 64 and layout == Layout(8, 8), "invalid LP64 pointer representation")
        pointee = _type(_single(clauses, "pointee"), target, analysis)
        category = None
        if analysis:
            expected = "function" if isinstance(pointee, FunctionType) else "object"
            category = _atom(_single(clauses, "category"), (expected,), "pointer category")
        return PointerType(bits, layout.size, layout.align, pointee, category)
    if kind == "array":
        clauses, _ = _clauses(node[1:], ("count", "size", "align", "element"))
        count = _known(_single(clauses, "count"))
        layout = _layout_values(clauses)
        return ArrayType(count, layout.size, layout.align,
                         _type(_single(clauses, "element"), target, analysis))
    if kind == "function-type":
        clauses, _ = _clauses(node[1:], ("prototype", "variadic", "result", "parameters"))
        prototype = _bool(_single(clauses, "prototype"))
        variadic = _bool(_single(clauses, "variadic"))
        params = clauses["parameters"]
        if len(params) - 1 > MAX_PARAMETERS:
            raise _limit("function parameter count limit exceeded")
        _require(prototype or len(params) == 1, "unprototyped function has a parameter list")
        _require(not variadic or (prototype and len(params) > 1), "invalid variadic function type")
        result = _type(_single(clauses, "result"), target, analysis)
        _require(not isinstance(result, (ArrayType, FunctionType, QualifiedType)),
                 "invalid function result shape")
        parameters = tuple(_type(param, target, analysis) for param in params[1:])
        _require(all(not isinstance(param, (VoidType, ArrayType, FunctionType, QualifiedType))
                     for param in parameters), "invalid adjusted parameter shape")
        return FunctionType(prototype, variadic, result, parameters)
    if kind == "qualified":
        _require(len(node) == 3 and _head(node[1]) == "qualifiers", "invalid qualified type shape")
        qualifiers = _qualifiers(node[1])
        child = _type(node[2], target, analysis)
        _require(not isinstance(child, (QualifiedType, FunctionType)), "invalid qualified type shape")
        if "restrict" in qualifiers:
            _require(isinstance(child, PointerType) and not isinstance(_base(child.pointee), FunctionType),
                     "invalid restrict-qualified type")
        return QualifiedType(qualifiers, child)
    if kind == "tag-ref":
        _require(len(node) == 2, "invalid tag-ref arity")
        return TagRef(_number(node[1]))
    raise InterfaceError("schema", "unknown type form")


def _field(node, target: str, analysis: bool) -> Field:
    _require(len(node) >= 2, "field lacks a name slot")
    name = _name(node[1])
    clauses, _ = _clauses(node[2:], ("offset", "type"), ("bit-field", "flexible-array"))
    offset = _number(_single(clauses, "offset"))
    type_ = _type(_single(clauses, "type"), target, analysis)
    bits = None
    if "bit-field" in clauses:
        bit, _ = _clauses(clauses["bit-field"][1:], ("width", "shift", "container-offset")
                          + (("value-range",) if analysis else ()))
        width = _number(_single(bit, "width"), high=64)
        range_ = None
        if analysis:
            base = _base(type_)
            _require(isinstance(base, IntegerType), "bit-field value range requires an integer type")
            range_ = _value_range(bit["value-range"], width, base.signed)
        bits = BitField(width, _number(_single(bit, "shift"), high=7),
                        _number(_single(bit, "container-offset")), range_)
    flexible = "flexible-array" in clauses
    if flexible:
        _require(_bool(_single(clauses, "flexible-array")), "flexible-array marker must be true")
    return Field(name, offset, type_, bits, flexible)


_TRAITS = ("const-member", "volatile-member", "contains-anonymous-member",
           "contains-flexible-array", "contains-bit-field")


def _tag(node, target: str, analysis: bool) -> Tag:
    _require(len(node) >= 4, "tag lacks ID, kind or name slot")
    tag_id = _number(node[1])
    kind = _atom(node[2], ("struct", "union", "enum"), "tag kind")
    name = _name(node[3])
    if kind == "enum":
        clauses, raw_members = _clauses(node[4:], ("complete",), ("underlying",), repeat="enumerator")
    else:
        clauses, raw_members = _clauses(node[4:], ("complete",),
                                        ("layout", "traits") if analysis else ("layout",),
                                        repeat="field")
    if not _bool(_single(clauses, "complete")):
        _require(len(clauses) == 1 and not raw_members and name is not None,
                 "incomplete tag has body data or no name")
        return Tag(tag_id, kind, name, False, None, (), None, ())
    _require(bool(raw_members), "complete tag lacks members")
    if kind == "enum":
        _require("underlying" in clauses, "complete enum lacks an underlying type")
        underlying = _type(_single(clauses, "underlying"), target, analysis)
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
    traits = None
    if analysis:
        _require("traits" in clauses, "complete record lacks traits")
        raw_traits, _ = _clauses(clauses["traits"][1:], _TRAITS)
        traits = RecordTraits(*(_bool(_single(raw_traits, key)) for key in _TRAITS))
    return Tag(tag_id, kind, name, True, layout,
               tuple(_field(field, target, analysis) for field in raw_members), None, (), traits)


def _site_parameter(node, parameters: tuple[CType, ...]) -> SiteParameter:
    _require(len(node) >= 3, "site parameter lacks index or facts")
    index = _number(node[1], high=len(parameters) - 1)
    clauses, _ = _clauses(node[2:], (), ("adjusted-from", "storage", "variable-bound",
                                        "requires-array-access", "adjusted-pointer-qualifiers"))
    adjusted, storage, bound, access, qualifiers = None, None, False, None, ()
    if "storage" in clauses:
        storage = _atom(_single(clauses, "storage"), ("register",), "parameter storage")
    if "adjusted-from" in clauses:
        adjusted = _atom(_single(clauses, "adjusted-from"), ("array",), "parameter adjustment")
        subject = parameters[index]
        _require(isinstance(subject, PointerType) and subject.category == "object"
                 and not isinstance(_base(subject.pointee), VoidType),
                 "array adjustment requires a pointer to an object type")
        if "variable-bound" in clauses:
            bound = _bool(_single(clauses, "variable-bound"))
            _require(bound, "variable-bound marker must be true")
        if "requires-array-access" in clauses:
            _require(not bound, "a star array bound cannot impose a static array minimum")
            access = ArrayAccess(_known(_single(clauses, "requires-array-access")))
        if "adjusted-pointer-qualifiers" in clauses:
            qualifiers = _qualifiers(clauses["adjusted-pointer-qualifiers"])
    else:
        _require(len(clauses) == 1 and storage is not None, "array facts lack array adjustment")
    return SiteParameter(index, adjusted, storage, bound, access, qualifiers)


def _declaration_sites(node, parameters: tuple[CType, ...] = (), *,
                       redeclarations: int | None = None, defined: bool = False
                       ) -> tuple[DeclarationSite, ...]:
    count = len(node) - 1
    if count > MAX_REDECLARATIONS:
        raise _limit("declaration site count limit exceeded")
    _require(count > 0 and (redeclarations is None or count == redeclarations),
             "declaration site count does not match declaration history")
    sites = []
    for raw_site in node[1:]:
        _require(_head(raw_site) == "site", "invalid declaration site")
        clauses, raw_params = _clauses(raw_site[1:], ("file", "line"), ("definition",),
                                       repeat="parameter", repeat_limit=MAX_PARAMETERS,
                                       repeat_subject="site parameter")
        raw_file = _single(clauses, "file")
        file = None if _is_atom(raw_file, "unknown") else raw_file
        line = _known(_single(clauses, "line"), _U32_MAX)
        _require((file is None and line is None) or (type(file) is str and line is not None),
                 "invalid presumed file and line pair")
        definition = "definition" in clauses
        if definition:
            _require(_bool(_single(clauses, "definition")), "site definition marker must be true")
        _require(len(raw_params) <= len(parameters), "too many site parameter facts for declaration")
        facts = tuple(_site_parameter(raw, parameters) for raw in raw_params)
        _require(all(a.index < b.index for a, b in zip(facts, facts[1:])),
                 "site parameter indices must be unique and in declaration order")
        sites.append(DeclarationSite(file, line, definition, facts))
    _require(sum(site.definition for site in sites) == int(defined),
             "definition sites do not match declaration definition")
    return tuple(sites)


def _call_contract(node, type_: FunctionType, noreturn: bool) -> CallContract:
    clauses, _ = _clauses(node[1:], ("normal-return", "fixed-parameters", "variadic-tail"))
    normal_return = _atom(_single(clauses, "normal-return"),
                          ("forbidden-by-declaration" if noreturn else "unknown",), "normal return")
    raw_count = _single(clauses, "fixed-parameters")
    raw_tail = _single(clauses, "variadic-tail")
    if type_.prototype:
        count = _number(raw_count, high=MAX_PARAMETERS)
        tail = _bool(raw_tail)
        _require(count == len(type_.parameters) and tail == type_.variadic,
                 "call contract does not match prototype")
    else:
        _require(_is_atom(raw_count, "unknown") and _is_atom(raw_tail, "unknown"),
                 "unprototyped call contract must leave count and variadic tail unknown")
        count = tail = None
    return CallContract(normal_return, count, tail)


_BEHAVIOR_FIELDS = ("memory-read", "memory-write", "allocation", "deallocation",
                    "throws-or-traps", "synchronization", "termination", "return-alias",
                    "return-nullability", "taint-inputs", "taint-outputs", "taint-transfer")


def _behavior(node) -> Behavior:
    clauses, _ = _clauses(node[1:], ("general-body-analysis", *_BEHAVIOR_FIELDS))
    general = _atom(_single(clauses, "general-body-analysis"), ("not-performed",), "body analysis")
    return Behavior(general, *(_atom(_single(clauses, key), ("unknown",), key)
                                for key in _BEHAVIOR_FIELDS))


def _proof_subject(clauses) -> tuple[str, str, str]:
    return (_atom(_single(clauses, "model"), (_PROOF_MODEL,), "bounded proof model"),
            _atom(_single(clauses, "scope"), (_PROOF_SCOPE,), "bounded proof scope"),
            _atom(_single(clauses, "solver"), ("satie",), "bounded proof solver"))


def _proof_type(type_: CType) -> bool:
    return isinstance(type_, IntegerType) and type_.name == "unsigned-int"


def _memory_safety(node, type_: FunctionType, can_model: bool,
                   names: tuple[str | None, ...]) -> MemorySafety:
    subject = ("model", "scope", "solver")
    clauses, _ = _clauses(node[1:], ("status",), (*subject, "checked-accesses"))
    status = _atom(_single(clauses, "status"), _PROOF_STATUS, "memory safety status")
    if len(clauses) == 1:
        _require(status == "unknown", "unmodeled memory safety must be unknown")
        return MemorySafety(status)
    _require(all(key in clauses for key in (*subject, "checked-accesses")),
             "incomplete modeled memory safety")
    model, scope, solver = _proof_subject(clauses)
    _require(can_model and type_.prototype and not type_.variadic
             and len(type_.parameters) <= _PROOF_PARAMETER_LIMIT and _proof_type(type_.result)
             and all(_proof_type(param) for param in type_.parameters)
             and None not in names and len(set(names)) == len(names),
             "invalid function subject for modeled memory safety")
    accesses = _number(_single(clauses, "checked-accesses"), high=_PROOF_ACCESS_LIMIT)
    _require(accesses != 0 or status == "proved",
             "modeled expression with no accesses must report proved memory safety")
    return MemorySafety(status, model, scope, accesses, solver)


def _body_proof(node, type_: FunctionType, memory: MemorySafety) -> BodyProof:
    _require(memory.status == "proved" and memory.model is not None,
             "body proof requires proved modeled memory safety")
    clauses, _ = _clauses(node[1:], ("model", "scope", "solver", "memory-effects",
                                    "return-dependencies", "return-policies"))
    model, scope, solver = _proof_subject(clauses)
    effects = _atom(_single(clauses, "memory-effects"), ("none-by-construction",), "proof memory effects")
    raw_dependencies = clauses["return-dependencies"]
    _require(len(raw_dependencies) == len(type_.parameters) + 1, "wrong return dependency count")
    dependencies = []
    for index, raw in enumerate(raw_dependencies[1:]):
        _require(_head(raw) == "parameter" and len(raw) == 3 and type(raw[1]) is int and raw[1] == index,
                 "wrong return dependency parameter index")
        status = _atom(raw[2], ("independent", "may-influence", "unknown"), "return dependency status")
        dependencies.append(ReturnDependency(index, status))
    raw_policies = clauses["return-policies"]
    _require(len(raw_policies) == len(_PROOF_WIDTHS) + 1, "wrong return policy count")
    policies = []
    proved_narrower = False
    for width, raw in zip(_PROOF_WIDTHS, raw_policies[1:]):
        _require(_head(raw) == "unsigned-width" and len(raw) == 3
                 and type(raw[1]) is int and raw[1] == width, "wrong return policy width")
        status = _atom(raw[2], _PROOF_STATUS, "return policy status")
        _require(not (proved_narrower and status == "disproved"), "contradictory return policy statuses")
        proved_narrower |= status == "proved"
        policies.append(ReturnPolicy(width, status))
    return BodyProof(model, scope, effects, tuple(dependencies), tuple(policies), solver)


def _declaration(node, target: str, analysis: bool, proof_budget: int) -> Function | Object | Typedef:
    kind = _head(node)
    _require(len(node) >= 2, "declaration lacks a name")
    name = _string(node[1])
    sites_clause = ("declarations",) if analysis else ()
    if kind == "typedef":
        clauses, _ = _clauses(node[2:], ("type", *sites_clause))
        sites = _declaration_sites(clauses["declarations"]) if analysis else ()
        return Typedef(name, _type(_single(clauses, "type"), target, analysis), sites)
    symbol = ("type", "linkage", "storage", "definition", *sites_clause)
    if kind == "object":
        clauses, _ = _clauses(node[2:], (*symbol, "layout") + (("storage-duration",) if analysis else ()))
        definitions = ("declaration", "tentative", "defined")
    else:
        required = (*symbol, "calling-convention", "parameter-names", "specifiers")
        clauses, _ = _clauses(node[2:], required + (("call-contract", "behavior", "memory-safety") if analysis else ()),
                              ("body-proof",) if analysis else ())
        definitions = ("declaration", "defined", "inline-only")
    type_ = _type(_single(clauses, "type"), target, analysis)
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
        sites, duration = (), None
        if analysis:
            sites = _declaration_sites(clauses["declarations"], redeclarations=len(storage),
                                       defined=definition == "defined")
            duration = _atom(_single(clauses, "storage-duration"), ("static",), "object storage duration")
        return Object(name, type_, linkage, storage, definition, _layout(clauses["layout"]), sites, duration)
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
    parameter_names = tuple(_name(n) for n in names[1:])
    sites, contract, behavior, memory, proof = (), None, None, None, None
    if analysis:
        defined = definition != "declaration"
        noreturn = "noreturn" in specifiers
        sites = _declaration_sites(clauses["declarations"], type_.parameters,
                                   redeclarations=len(storage), defined=defined)
        contract = _call_contract(clauses["call-contract"], type_, noreturn)
        behavior = _behavior(clauses["behavior"])
        memory = _memory_safety(clauses["memory-safety"], type_,
                                defined and not noreturn and proof_budget > 0, parameter_names)
        if memory.status == "proved":
            _require("body-proof" in clauses, "proved modeled memory safety lacks body proof")
        if "body-proof" in clauses:
            proof = _body_proof(clauses["body-proof"], type_, memory)
    return Function(name, type_, linkage, storage, definition, str(convention),
                    parameter_names, specifiers, sites, contract, behavior, memory, proof)


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


def _validate_record_traits(tag: Tag, tags: dict[int, Tag], has_flexible: set[int]) -> None:
    if tag.traits is None:
        return
    const = volatile = anonymous = bit = False
    for field in tag.fields:
        kind = field.type
        if field.bit_field is not None:
            bit = True
            if field.name is None:
                continue  # Unnamed bit-fields contribute no const/volatile member.
        elif field.name is None:
            anonymous = True
        while True:
            if isinstance(kind, QualifiedType):
                const |= "const" in kind.qualifiers
                volatile |= "volatile" in kind.qualifiers
                kind = kind.type
            elif isinstance(kind, ArrayType):
                kind = kind.element
            else:
                break
        if isinstance(kind, TagRef) and (child := tags[kind.id].traits) is not None:
            const |= child.const_member
            volatile |= child.volatile_member
            anonymous |= child.contains_anonymous_member
            bit |= child.contains_bit_field
    _require(tag.traits == RecordTraits(const, volatile, anonymous, tag.id in has_flexible, bit),
             "record traits do not match members")


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
        _validate_record_traits(tag, tags, has_flexible)

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
    and data model against one another. This validates the analysis wire
    grammar and its subjects, not the underlying proof or C source.
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
    clauses, _ = _clauses(envelope, ("target", "data-model", "abi"),
                          ("analysis-model", "target-properties"))
    target = _string(_single(clauses, "target"))
    abi = _string(_single(clauses, "abi"))
    if (target not in _TARGETS or (expected_target is not None and target != expected_target)
            or abi != _TARGETS[target][0] or not _is_atom(_single(clauses, "data-model"), "LP64")):
        raise InterfaceError("target", "interface target, data model or ABI mismatch")

    analysis = "analysis-model" in clauses
    model = _analysis_model(clauses["analysis-model"]) if analysis else None
    _require(analysis == ("target-properties" in clauses),
             "analysis-model and target-properties must occur together")
    properties = _target_properties(clauses["target-properties"]) if analysis else None
    tags = {}
    for raw in raw_tags:
        tag = _tag(raw, target, analysis)
        _require(tag.id not in tags, "duplicate tag ID")
        tags[tag.id] = tag
    declarations, names = [], set()
    proof_budget = _PROOF_DEFINITION_LIMIT
    for raw in raw_declarations:
        decl = _declaration(raw, target, analysis, proof_budget)
        _require(decl.name not in names, "duplicate ordinary declaration")
        names.add(decl.name)
        declarations.append(decl)
        if (analysis and isinstance(decl, Function) and decl.definition != "declaration"
                and "noreturn" not in decl.specifiers):
            proof_budget -= 1
    _validate_graph(declarations, tags, target)
    return Interface(1, target, "LP64", abi, tuple(declarations), tuple(tags.values()), model, properties)
