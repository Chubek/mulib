"""Run bounded Wetstone numeric queries on normalized Cephyr analysis-model 1 facts."""
from collections.abc import Mapping
from dataclasses import dataclass, replace
from itertools import islice
import tempfile

from project_interface import ProjectionError, _absolute, _capture


_PROTOCOL = b"wetstone-ffi-obligations 1\n"
MAX_QUERIES = 4096
_I64_MIN, _U64_MAX = -(1 << 63), (1 << 64) - 1
_UINT32_RANGE = (0, (1 << 32) - 1)
_WIDTHS = (8, 16, 31)
_STATUSES = {"proved", "disproved", "unknown"}


def _integer(value, low, high):
    if type(value) is not int or not low <= value <= high:
        raise ValueError("integer outside Wetstone obligation bounds")


def _range(values, low, high):
    if type(values) is not tuple or len(values) != 2:
        raise ValueError("expected an inclusive (min, max) tuple")
    _integer(values[0], low, high)
    _integer(values[1], values[0], high)


@dataclass(frozen=True, slots=True)
class RangeCheck:
    """Whether every value in an upper approximation fits an accepted range."""
    value_range: tuple[int, int]
    accepted_range: tuple[int, int]

    def __post_init__(self):
        _range(self.value_range, _I64_MIN, _U64_MAX)
        _range(self.accepted_range, _I64_MIN, _U64_MAX)


@dataclass(frozen=True, slots=True)
class ArrayCheck:
    """Numeric capacity for one known C parameter-array obligation at one site."""
    minimum_elements: int
    element_bytes: int
    capacity_bytes: tuple[int, int]

    def __post_init__(self):
        _integer(self.minimum_elements, 1, _U64_MAX)
        _integer(self.element_bytes, 1, _U64_MAX)
        _range(self.capacity_bytes, 0, _U64_MAX)


def closed_return_check(declared_range, accepted_range, *, memory_status, width_statuses):
    """Build a range query for Cephyr's exact closed-unsigned-expression-v2 body.

    Supply only normalized, same-definition/same-model Satie facts from a checked
    analysis-model 1 projection. None or no proved width yields no body query.
    Declaration-only containment is a separate RangeCheck.
    """
    if memory_status is not None and (type(memory_status) is not str or memory_status not in _STATUSES):
        raise ValueError("invalid Cephyr return policy")
    if width_statuses is not None:
        if (not isinstance(width_statuses, Mapping) or len(width_statuses) != len(_WIDTHS)
                or any(type(width) is not int or width not in _WIDTHS
                       or type(status) is not str or status not in _STATUSES
                       for width, status in width_statuses.items())):
            raise ValueError("invalid Cephyr return policy")
    if memory_status != "proved" or width_statuses is None:
        return None
    _range(declared_range, _I64_MIN, _U64_MAX)
    if declared_range != _UINT32_RANGE:
        raise ValueError("closed Cephyr return proof needs the unsigned-int declaration domain")
    proved = [width for width in _WIDTHS if width_statuses[width] == "proved"]
    if not proved:
        return None
    if any(width_statuses[width] == "disproved" for width in _WIDTHS if width > proved[0]):
        raise ValueError("inconsistent Cephyr return policies")
    return RangeCheck((0, (1 << proved[0]) - 1), accepted_range)


def run_obligations(provider, helper, checks):
    """Return verdicts in query order; None is an unknown slot, not a query.

    Inventory the selected helper file in CephyrProvider's ``tool_inputs``.
    A process/transport failure raises ProjectionError; discard the whole batch.
    Neither the subprocess budgets nor this private protocol are a sandbox.
    """
    checks = tuple(islice(checks, MAX_QUERIES + 1))
    if len(checks) > MAX_QUERIES:
        raise ProjectionError("wetstone_limit", "too many Wetstone obligations")
    results = ["unknown"] * len(checks)
    rows = []
    slots = []
    for slot, query in enumerate(checks):
        if query is None:
            continue
        if type(query) is RangeCheck:
            fields = ("range", *query.value_range, *query.accepted_range)
        elif type(query) is ArrayCheck:
            fields = ("array", query.minimum_elements, query.element_bytes, *query.capacity_bytes)
        else:
            raise TypeError("unsupported Wetstone obligation")
        rows.append(" ".join(map(str, fields)).encode("ascii") + b"\n")
        slots.append(slot)
    if not rows:
        return tuple(results)
    payload = _PROTOCOL + b"".join(rows)
    if len(payload) > provider.limits.input_bytes:
        raise ProjectionError("wetstone_limit", "Wetstone obligation byte budget exceeded")
    selected = _absolute(helper)
    if not any(label.startswith("tool:") and path == selected
               for label, path in provider._tool_files):
        raise ProjectionError("wetstone_input", "Wetstone helper is not a selected tool file")
    limits = replace(provider.limits, output_bytes=min(
        provider.limits.output_bytes, len(_PROTOCOL) + len(rows) * len(b"disproved\n")))
    provider._verify_tool_files()
    try:
        with tempfile.TemporaryDirectory(prefix="on1x-wetstone-obligations-", dir=provider.scratch_root) as cwd:
            status, output, diagnostics = _capture((
                str(provider.prlimit), f"--as={provider.limits.address_space_bytes}",
                "--core=0", "--", str(selected),
            ), payload, cwd, limits)
    except OSError as exc:
        raise ProjectionError("scratch_io", "could not prepare Wetstone scratch directory") from exc
    provider._verify_tool_files()
    lines = output.splitlines(keepends=True)
    if status != 0 or diagnostics or len(lines) != len(slots) + 1 or lines[0] != _PROTOCOL:
        raise ProjectionError("wetstone_failed", "invalid or incomplete Wetstone obligation output")
    for slot, row, line in zip(slots, rows, lines[1:], strict=True):
        if (line not in (b"proved\n", b"unknown\n")
                and not (row.startswith(b"array ") and line == b"disproved\n")):
            raise ProjectionError("wetstone_failed", "invalid Wetstone obligation verdict")
        results[slot] = line[:-1].decode("ascii")
    return tuple(results)
