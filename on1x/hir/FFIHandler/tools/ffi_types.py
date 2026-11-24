"""Representation policy over W01's resolved Cephyr projection.

The scalar mapping describes an adapter, not callable or safe admission.
No typedef names, C source spellings, or host ABI queries resolve types here.
"""
from __future__ import annotations

from dataclasses import dataclass
import hashlib
import json

from cephyr_interface import (
    ArrayType, FunctionType, IntegerType, PointerType, QualifiedType, RealType,
    TagRef, VoidType,
)


TARGETS = {
    "x86_64-linux-gnu": ("LP64", "SysVAMD64", "sysv"),
    "aarch64-linux-gnu": ("LP64", "AAPCS64", "aapcs64"),
    "riscv64-linux-gnu": ("LP64", "LP64", "lp64"),
}
MAX_PARAMS = 16
INTEGRAL_SURFACES = frozenset(("i64", "u8", "u16", "u32", "u64"))
_SIGNED = {"signed-char": (1, 8), "short": (2, 16), "int": (3, 32),
           "long": (4, 64), "long-long": (5, 64)}
_UNSIGNED = {"unsigned-char": (1, 8), "unsigned-short": (2, 16),
             "unsigned-int": (3, 32), "unsigned-long": (4, 64),
             "unsigned-long-long": (5, 64)}


def digest(value) -> str:
    """SHA-256 of compact canonical JSON (never an admission decision)."""
    raw = json.dumps(value, ensure_ascii=True, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(raw.encode("ascii")).hexdigest()


def target_identity(target: str) -> tuple[str, str, str, str]:
    if target not in TARGETS:
        raise ValueError(f"unsupported generation target: {target}")
    return (target, *TARGETS[target])


def require_target(interface, target: tuple[str, str, str, str]) -> None:
    if (interface.version != 1 or
            (interface.target, interface.data_model, interface.abi) != target[:3]):
        raise ValueError("projection target/data model/ABI does not match generation target")


def native_primitive(typ) -> list | None:
    """W05's exact compact primitive encoding; None means a nonprimitive."""
    if isinstance(typ, VoidType):
        return ["void"]
    if isinstance(typ, IntegerType):
        return ["integer", typ.name, typ.rank, typ.bits,
                "signed" if typ.signed else "unsigned", typ.size, typ.align]
    if isinstance(typ, RealType):
        return ["real", typ.name, typ.format, typ.bits, typ.size, typ.align]
    return None


class NativeGraph:
    """Normalize tag references in deterministic first-use order.

    Call type() for the selected function roots in native symbol order, then
    tags(). References are module-local ordinals, *not* persisted Cephyr IDs.
    A tag has a single ordinal irrespective of its name or structure. Deferred
    traversal through tags avoids recursive expansion along pointer cycles.
    """

    def __init__(self, tags):
        self._tags = {tag.id: tag for tag in tags}
        self._order = []
        self._ids = {}

    def type(self, typ) -> list:
        primitive = native_primitive(typ)
        if primitive is not None:
            return primitive
        if isinstance(typ, PointerType):
            return ["pointer", typ.bits, typ.size, typ.align, self.type(typ.pointee)]
        if isinstance(typ, ArrayType):
            return ["array", typ.count, typ.size, typ.align, self.type(typ.element)]
        if isinstance(typ, FunctionType):
            return ["function-type", typ.prototype, typ.variadic, self.type(typ.result),
                    [self.type(parameter) for parameter in typ.parameters]]
        if isinstance(typ, QualifiedType):
            return ["qualified", sorted(typ.qualifiers), self.type(typ.type)]
        if isinstance(typ, TagRef):
            if typ.id not in self._ids:
                self._ids[typ.id] = len(self._order)
                self._order.append(typ.id)
            return ["tag-ref", self._ids[typ.id]]
        raise ValueError("unrecognized typed projection node")

    def tags(self) -> list:
        result = []
        cursor = 0
        while cursor < len(self._order):
            tag = self._tags[self._order[cursor]]
            layout = tag.layout
            fields = []
            for field in tag.fields:
                bit = field.bit_field
                fields.append({
                    "name": field.name, "offset": field.offset, "type": self.type(field.type),
                    "bit_field": [bit.width, bit.shift, bit.container_offset] if bit else None,
                    "flexible_array": field.flexible_array,
                })
            result.append({
                "id": cursor, "kind": tag.kind, "name": tag.name, "complete": tag.complete,
                "layout": [layout.size, layout.align] if layout is not None else None,
                "fields": fields,
                "underlying": self.type(tag.underlying) if tag.underlying is not None else None,
                "enumerators": [[item.name, item.value] for item in tag.enumerators],
            })
            cursor += 1
        return result


def reachable_native_graph(function, tags) -> tuple[list, list]:
    """The symbol-local graph for the proposed rich schema, not a v2 identity.

    Start with a fresh ordinal map so unrelated declarations cannot shift tag
    IDs. The root visits its result before its arguments; definitions are
    then visited in first-reference order, including through recursive pointers.
    """
    graph = NativeGraph(tags)
    root = graph.type(function.type)
    return root, graph.tags()


def _surface(typ, *, result: bool) -> tuple[str | None, str | None]:
    if isinstance(typ, VoidType):
        return ("void", None) if result else (None, "void_parameter")
    if isinstance(typ, IntegerType):
        if typ.name == "char":
            return None, "plain_char"
        if typ.name == "bool":
            if (typ.rank, typ.bits, typ.signed, typ.size, typ.align) == (0, 1, False, 1, 1):
                return "bool", None
        else:
            widths = (_SIGNED if typ.signed else _UNSIGNED).get(typ.name)
            if widths == (typ.rank, typ.bits) and typ.size == typ.align == typ.bits // 8:
                # Checked native narrowing happens in the adapter for signed args;
                # sign extension happens on signed results. Unsigned is exact width.
                return ("i64" if typ.signed else f"u{typ.bits}"), None
        return None, "integer_representation"
    if isinstance(typ, RealType):
        if typ.name == "long-double":
            return None, "long_double"
        if (typ.name, typ.format, typ.bits, typ.size, typ.align) == ("float", "binary32", 32, 4, 4):
            return "f32", None
        if (typ.name, typ.format, typ.bits, typ.size, typ.align) == ("double", "binary64", 64, 8, 8):
            return "f64", None
        return None, "floating_representation"
    if isinstance(typ, QualifiedType):
        return None, "qualified_type"
    if isinstance(typ, TagRef):
        return None, "aggregate_or_enum"
    if isinstance(typ, PointerType):
        return None, "pointer"
    if isinstance(typ, ArrayType):
        return None, "array"
    if isinstance(typ, FunctionType):
        return None, "function_type"
    raise ValueError("unrecognized typed projection node")


@dataclass(frozen=True)
class Representation:
    placement: str | None
    surface_parameters: tuple[str, ...] = ()
    surface_result: str | None = None
    reason: str | None = None


def represent(function, target: tuple[str, str, str, str]) -> Representation:
    """Return scalar eligibility or the first stable unsupported reason."""
    typ = function.type
    if function.calling_convention != target[3]:
        raise ValueError("projected calling convention does not match generation target")
    if not typ.prototype:
        return Representation(None, reason="no_prototype")
    if typ.variadic:
        return Representation(None, reason="variadic")
    if "noreturn" in function.specifiers:
        return Representation(None, reason="noreturn")
    if function.linkage == "external":
        if function.definition == "inline-only":
            return Representation(None, reason="external_inline_only")
        if function.definition not in ("declaration", "defined"):
            return Representation(None, reason="external_definition")
        placement = "external"
    elif (function.linkage == "internal" and function.definition == "defined" and
          "inline" in function.specifiers):
        placement = "static_inline"
    else:
        return Representation(None, reason="internal_linkage")
    if len(typ.parameters) > MAX_PARAMS:
        return Representation(placement, reason="parameter_limit")
    result, reason = _surface(typ.result, result=True)
    if reason is not None:
        return Representation(placement, reason=reason)
    params = []
    for parameter in typ.parameters:
        surface, reason = _surface(parameter, result=False)
        if reason is not None:
            return Representation(placement, reason=reason)
        params.append(surface)
    return Representation(placement, tuple(params), result)
