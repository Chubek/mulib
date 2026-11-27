"""Read the On1x companion to an annotated ExoML v2 contract."""

from __future__ import annotations

from dataclasses import dataclass
import json
from pathlib import Path
import re

from compile_recipes import DIRECTIVES, IDENT, KERNEL, MAX_EXPRESSION_NODES, OPS, SELECTOR, read, require


CONTRACTS_DIR = Path(__file__).resolve().parent / "contracts"
POSITION = re.compile(r"\$(0|[1-9][0-9]{0,2})")
REVISION = re.compile(r"sha256:[0-9a-f]{64}")


@dataclass(frozen=True)
class DirectiveRequest:
    kind: str
    action: str
    backend: str = ""
    target: str = ""
    speculative: bool = False

    def render(self):
        """Render a validated request as a single ffispec v1 directive."""
        fields = [self.kind, self.action]
        if self.backend:
            fields.append(f"(backend {self.backend})")
        if self.target:
            fields.append(f"(target {self.target})")
        if self.speculative:
            fields.append("(speculative)")
        return "(" + " ".join(fields) + ")"


@dataclass(frozen=True)
class ExportRecipe:
    guard: str | tuple
    directives: tuple[DirectiveRequest, ...]


def _unique_fields(pairs):
    fields = {}
    for key, value in pairs:
        require(key not in fields, f"duplicate JSON field {key!r}")
        fields[key] = value
    return fields


def _invalid_constant(value):
    raise ValueError(f"invalid JSON constant {value!r}")


def _fields(value, required, optional=()):
    require(isinstance(value, dict), "expected a JSON object")
    missing = required - value.keys()
    unknown = value.keys() - optional - required
    require(not missing, f"missing fields: {', '.join(sorted(missing))}")
    require(not unknown, f"unknown fields: {', '.join(sorted(unknown))}")


def _guard(value, arity, budget, expected="bool", depth=0):
    budget[0] -= 1
    require(depth <= 32 and budget[0] >= 0, "guard exceeds expression depth or node limit")
    if expected == "bool" and value == "true":
        return value
    if expected == "i64":
        if type(value) is int:
            require(-(1 << 63) <= value < 1 << 63, "guard literal outside i64")
            return value
        if isinstance(value, str):
            position = POSITION.fullmatch(value)
            require(position is not None and int(position[1]) < arity,
                    f"invalid guard parameter reference {value!r}")
            return value
    require(isinstance(value, list) and value and isinstance(value[0], str),
            f"expected a {expected} guard expression")
    operator, *operands = value
    require(operator in OPS and operator != "if", f"unsupported guard operator {operator!r}")
    _, result, *arguments = OPS[operator]
    require(result == expected and len(operands) == len(arguments),
            f"wrong guard arity or type for {operator!r}")
    return (operator, *(_guard(operand, arity, budget, kind, depth + 1)
                        for operand, kind in zip(operands, arguments)))


def _selector(spec, name):
    if name not in spec:
        return ""
    value = spec[name]
    require(isinstance(value, str) and (value == "*" or SELECTOR.fullmatch(value)),
            f"invalid directive {name} selector")
    return "" if value == "*" else value


def _directive(spec):
    _fields(spec, {"kind", "action"}, {"backend", "target", "speculative"})
    kind, action = spec["kind"], spec["action"]
    require(isinstance(kind, str) and kind in DIRECTIVES, f"unsupported directive kind {kind!r}")
    require(isinstance(action, str) and action in DIRECTIVES[kind],
            f"unsupported {kind} directive {action!r}")
    backend, target = _selector(spec, "backend"), _selector(spec, "target")
    required_backend = DIRECTIVES[kind][action]
    require(required_backend is None or backend == required_backend,
            f"{kind} {action} requires effective backend {required_backend}")
    speculative = spec.get("speculative", False)
    require(type(speculative) is bool, "directive speculative must be a JSON boolean")
    return DirectiveRequest(kind, action, backend, target, speculative)


def _export(spec, arity):
    _fields(spec, {"guard"}, {"directives"})
    guard = _guard(spec["guard"], arity, [MAX_EXPRESSION_NODES])
    requests = spec.get("directives", [])
    # The ffispec compiler allows 128 export fields; params, guard and body
    # occupy three. There are no inherited selectors in this format.
    require(isinstance(requests, list) and len(requests) <= 125,
            "directives must be an array of at most 125 requests")
    directives = tuple(map(_directive, requests))
    require(len(set(directives)) == len(directives), "duplicate effective directive request")
    return ExportRecipe(guard, directives)


def load_portable_recipe_contract(path, *, kernel, revision, arities) -> dict[str, ExportRecipe]:
    """Load a companion using the verified ExoML library, revision and C symbol arities.

    `arities` contains exactly the admitted exports, not all declarations.
    This validates format and linkage to those inputs; it does not grant
    admission or replace the independent review of the guard and body.
    """
    path = Path(path)
    try:
        document = json.loads(read(path).decode("utf-8"), object_pairs_hook=_unique_fields,
                              parse_constant=_invalid_constant)
        _fields(document, {"schema_version", "library", "revision", "functions"})
        version = document["schema_version"]
        require(type(version) is int and version == 1, "expected portable recipe companion schema_version 1")
        library, pinned = document["library"], document["revision"]
        require(isinstance(library, str) and KERNEL.fullmatch(library) and library == kernel,
                f"companion library does not match {kernel!r}")
        require(isinstance(pinned, str) and REVISION.fullmatch(pinned) and pinned == revision,
                "companion revision does not match the ExoML contract")
        functions = document["functions"]
        require(isinstance(functions, dict) and functions, "expected nonempty companion functions object")
        symbols = functions.keys()
        missing, extra = arities.keys() - symbols, symbols - arities.keys()
        require(not missing, f"missing portable companion export(s): {', '.join(sorted(missing))}")
        require(not extra, f"unexpected portable companion export(s): {', '.join(sorted(extra))}")
        result = {}
        for symbol, arity in arities.items():
            try:
                require(IDENT.fullmatch(symbol) and symbol not in {"true", "false"},
                        "invalid portable C symbol")
                require(type(arity) is int and 0 <= arity <= 128, "invalid portable export arity")
                result[symbol] = _export(functions[symbol], arity)
            except ValueError as error:
                raise ValueError(f"{symbol}: {error}") from error
        return result
    except RecursionError as error:
        raise ValueError(f"{path}: companion JSON exceeds nesting limit") from error
    except ValueError as error:
        raise ValueError(f"{path}: {error}") from error
