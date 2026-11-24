"""Select declaration-scoped Wetstone queries for the generated FFI inventory."""

from cephyr_interface import ArrayType, IntegerType, PointerType, QualifiedType, RealType, TagRef
from ffi_obligations import ArrayCheck, MAX_QUERIES, RangeCheck, closed_return_check, run_obligations


_CARRIER_RANGES = {"bool": (0, 1), "i64": (-(1 << 63), (1 << 63) - 1),
                   **{f"u{width}": (0, (1 << width) - 1) for width in (8, 16, 32, 64)}}


def _unqualified(type_):
    while isinstance(type_, QualifiedType):
        type_ = type_.type
    return type_


def _object_size(type_, tags):
    type_ = _unqualified(type_)
    if isinstance(type_, (ArrayType, IntegerType, PointerType, RealType)):
        return type_.size
    if isinstance(type_, TagRef):
        tag = tags[type_.id]
        if tag.kind == "enum":
            return tag.underlying.size if tag.underlying is not None else None
        return tag.layout.size if tag.layout is not None else None
    return None


def _queries(function, row, tags, size_t_max):
    """Return (report subject, genuine helper query or an explicitly unknown slot)."""
    surface = (row.get("surface") or {}).get("result")
    result = _unqualified(function.type.result)
    accepted = _CARRIER_RANGES.get(surface)
    if accepted is not None and isinstance(result, IntegerType) and result.value_range is not None:
        bounds = (result.value_range.minimum, result.value_range.maximum)
        yield {"kind": "result_carrier_domain", "carrier": surface, "scope": "declaration"}, RangeCheck(
            bounds, accepted)
    if function.body_proof is not None:
        proof = function.body_proof
        memory = function.memory_safety
        # The decoder also checks this exact policy/scope/solver/return domain.
        if (proof.model == "closed-unsigned-expression-v2" and proof.scope == "this-definition"
                and proof.solver == "satie" and memory is not None and memory.model == proof.model
                and isinstance(result, IntegerType) and result.name == "unsigned-int"
                and result.value_range is not None):
            bounds = (result.value_range.minimum, result.value_range.maximum)
            widths = {policy.width: policy.status for policy in proof.return_policies}
            for width in sorted(widths):
                yield {"kind": "return_upper_bound", "bits": width, "scope": "this_definition"}, closed_return_check(
                    bounds, (0, (1 << width) - 1), memory_status=memory.status, width_statuses=widths)
    for ordinal, site in enumerate(function.declaration_sites):
        for parameter in site.parameters:
            if parameter.requires_array_access is None:
                continue
            minimum = parameter.requires_array_access.minimum_elements
            pointer = _unqualified(function.type.parameters[parameter.index])
            stride = _object_size(pointer.pointee, tags) if isinstance(pointer, PointerType) else None
            # This is size_t representability, not the accessible bytes at any call site.
            query = (ArrayCheck(minimum, stride, (size_t_max, size_t_max))
                     if minimum is not None and minimum > 0 and stride is not None and stride > 0
                     else None)
            yield ({"kind": "array_minimum_size_t", "scope": "declaration", "site": ordinal,
                    "parameter": parameter.index, "minimum_elements": minimum, "element_bytes": stride}, query)


def numeric_obligation_report(provider, helper, interface, rows):
    if interface.analysis_model is None:
        return {"status": "unavailable", "reason": "cephyr_analysis_not_projected"}
    symbols = {row["symbol"]: row for row in rows if row["emitted"]}
    tags = {tag.id: tag for tag in interface.tags}
    limit = (1 << interface.target_properties.pointer_bits) - 1
    reports, queries = [], []
    for function in sorted(interface.functions, key=lambda fn: fn.name):
        if function.name not in symbols:
            continue
        for description, query in _queries(function, symbols[function.name], tags, limit):
            if len(queries) >= MAX_QUERIES:
                return {"status": "unavailable", "reason": "work_limit"}
            reports.append({"symbol": function.name, **description})
            queries.append(query)
    verdicts = run_obligations(provider, helper, queries)
    return {"status": "partial", "checks": [
        {**description, "status": result}
        for description, result in zip(reports, verdicts, strict=True)
    ]}
