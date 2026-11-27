"""Validate the LLSynth MIR policy parse tree and emit a static C++ database."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys


BANKS = {
    "additive-identities": "AdditiveIdentities",
    "multiplicative-identities": "MultiplicativeIdentities",
    "division-units": "DivisionUnits",
    "guarded-division": "GuardedDivision",
    "scalar-equality": "ScalarEquality",
    "ordered-reflexivity": "OrderedReflexivity",
    "boolean-negation": "BooleanNegation",
    "boolean-comparisons": "BooleanComparisons",
    "store-forwarding": "StoreForwarding",
    "load-reuse": "LoadReuse",
}
# Independently generated profiles. Their activations are scoped and
# mode-exact in the runtime, unlike the legacy core activation table.
PROFILES = {"memgraph-local": "MemgraphLocal", "meta-canonical": "MetaCanonical"}
ARITHMETIC = {"additive-identities", "multiplicative-identities", "division-units", "guarded-division"}
PREDICATES = {"scalar-equality", "ordered-reflexivity", "boolean-negation", "boolean-comparisons"}
ACTIONS = {
    "rewrite": {"fold-identity": ARITHMETIC | PREDICATES},
    "hint": {"kernel-hot": ARITHMETIC, "canonicalized": PREDICATES,
             "local-memory": {"store-forwarding", "load-reuse"}},
}
OPS = {
    "add.checked": "Add", "sub.checked": "Sub", "mul.checked": "Mul",
    "div.checked": "Div", "mod.checked": "Mod", "load": "Load", "store": "Store",
    "eq": "Eq", "ne": "Ne", "lt": "Lt", "le": "Le", "gt": "Gt", "ge": "Ge", "not": "Not",
    "neg.checked": "Neg",
}
TYPED_SLOT_PROOFS = {
    "local-store-word": "LocalStoreWord", "local-load-word": "LocalLoadWord",
    "local-store-f32": "LocalStoreF32", "local-load-f32": "LocalLoadF32",
    "local-store-f64": "LocalStoreF64", "local-load-f64": "LocalLoadF64",
    "local-store-tuple": "LocalStoreTuple", "local-load-tuple": "LocalLoadTuple",
}
PROOFS = {
    "identity": "Identity", "nonzero": "Nonzero", "local-store": "LocalStore",
    "local-load": "LocalLoad", "double-not": "DoubleNot", "bool-operands": "BoolOperands",
    "double-checked-neg": "DoubleCheckedNeg", "bool-complement": "BoolComplement",
    "nonzero-comparison": "NonzeroComparison", "redundant-store": "RedundantStore",
    **TYPED_SLOT_PROOFS,
}
CAPTURE = "Capture"
LITERAL = "Literal"


def parser_module(path):
    spec = importlib.util.spec_from_file_location("on1x_mir_policy_parser", path)
    parser = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = parser
    spec.loader.exec_module(parser)
    return parser


def spelling(node):
    return node.token.lexeme


def quoted(node):
    value = spelling(node)[1:-1]
    if len(value) > 128:
        raise ValueError("quoted name exceeds 128 characters")
    if not re.fullmatch(r"[A-Za-z_0-9][A-Za-z_0-9.-]*", value):
        raise ValueError("expected an unescaped policy name")
    return value


def condition(node):
    return tuple("" if spelling(part) == "*" else quoted(part)
                 for part in (node.children[1], node.children[3]))


def atom(node):
    if len(node.children) == 2:
        return CAPTURE, spelling(node.children[1])
    text = spelling(node)
    if len(text) > 20:
        raise ValueError("integer is not a canonical i64")
    value = int(text)
    if not -(1 << 63) <= value < (1 << 63) or text != str(value):
        raise ValueError("integer is not a canonical i64")
    return LITERAL, value


def arguments(node):
    if not node.children:
        return []
    first, tail = node.children
    args = [atom(first)]
    while tail.children:
        if len(args) == 2:
            raise ValueError("patterns support at most two operands")
        _, item, tail = tail.children
        args.append(atom(item))
    return args


def prove(bank, op, args, replacement, mode, proof):
    # Admit only these verified MIR theorems. A "requires" annotation alone
    # is not evidence: every conditional theorem has a corresponding check
    # in the specializer, using the verified instruction and local facts.
    if bank in {"store-forwarding", "load-reuse"}:
        origin = {"store-forwarding": ("local-store", "stored"), "load-reuse": ("local-load", "loaded")}
        expected_proof, capture = origin[bank]
        return (op == "Load" and not args and replacement == (CAPTURE, capture)
                and mode == "safe" and proof == expected_proof)

    if bank == "memgraph-local":
        if proof == "redundant-store":
            return (op == "Store" and len(args) == 1 and args[0][0] == CAPTURE
                    and replacement == args[0] and mode == "speculative")
        if proof not in TYPED_SLOT_PROOFS:
            return False
        capture = "stored" if proof.startswith("local-store") else "loaded"
        return (op == "Load" and not args and replacement == (CAPTURE, capture)
                and mode == ("speculative" if proof.endswith("-tuple") else "safe"))

    z, one = (LITERAL, 0), (LITERAL, 1)
    if bank == "meta-canonical":
        if op == "Neg" and len(args) == 1:
            x, = args
            return ((mode == "safe" and proof == "identity" and x == replacement == z)
                    or (mode == "speculative" and proof == "double-checked-neg"
                        and x[0] == CAPTURE and replacement == (CAPTURE, "original") and x != replacement))
        if len(args) != 2:
            return False
        x, y = args
        if op in {"Eq", "Ne"} and replacement == (z if op == "Eq" else one):
            if proof == "bool-complement":
                return mode == "safe" and x[0] == y[0] == CAPTURE and x != y
            if proof == "nonzero-comparison":
                return (mode == "speculative" and
                        ((x == z and y[0] == CAPTURE) or (y == z and x[0] == CAPTURE)))
        if op in {"Lt", "Gt", "Ge"}:
            bound = (LITERAL, (1 << 63) - 1 if op == "Gt" else -(1 << 63))
            return (mode == "safe" and proof == "identity" and x[0] == CAPTURE
                    and y == bound and replacement == (one if op == "Ge" else z))
        return False

    if bank == "boolean-negation":
        if mode != "safe" or op != "Not" or len(args) != 1:
            return False
        x, = args
        return ((proof == "identity" and ((x == z and replacement == one)
                                          or (x == one and replacement == z)))
                or (proof == "double-not" and x[0] == CAPTURE
                    and replacement == (CAPTURE, "original") and x != replacement))

    if len(args) != 2:
        return False
    x, y = args
    if bank == "guarded-division":
        return (proof == "nonzero" and mode == "speculative" and op in {"Div", "Mod"}
                and y[0] == CAPTURE and ((x == y and replacement == (one if op == "Div" else z))
                                         or (x == z and replacement == z)))

    def given(literal):
        return ((x == literal and y[0] == CAPTURE and replacement == y)
                or (y == literal and x[0] == CAPTURE and replacement == x))

    if bank == "boolean-comparisons":
        return (mode == "safe" and proof == "bool-operands"
                and ((op == "Eq" and given(one)) or (op == "Ne" and given(z))))
    if mode != "safe" or proof != "identity":
        return False
    reflexive = x == y and x[0] == CAPTURE
    if bank == "scalar-equality":
        return reflexive and ((op == "Eq" and replacement == one) or (op == "Ne" and replacement == z))
    if bank == "ordered-reflexivity":
        return reflexive and ((op in {"Le", "Ge"} and replacement == one)
                              or (op in {"Lt", "Gt"} and replacement == z))
    if bank == "additive-identities":
        return ((op == "Add" and given(z))
                or (op == "Sub" and ((x[0] == CAPTURE and y == z and replacement == x)
                                     or (reflexive and replacement == z))))
    if bank == "multiplicative-identities":
        return (op == "Mul" and (given(one)
                                 or (replacement == z and ((x == z and y[0] == CAPTURE)
                                                           or (y == z and x[0] == CAPTURE)))))
    if bank == "division-units":
        return (y == one and x[0] == CAPTURE
                and ((op == "Div" and replacement == x) or (op == "Mod" and replacement == z)))
    return False


def normalized_atoms(args, result):
    captures = {}

    def encode(atom):
        kind, payload = atom
        return kind, captures.setdefault(payload, len(captures)) if kind == CAPTURE else payload

    return tuple(map(encode, args)), encode(result)


def rule(node, bank):
    _, name, when, _, mode, _, opcode, _, args, _, _, result, _, proof, _ = node.children
    mode, opcode, proof = spelling(mode), quoted(opcode), spelling(proof)
    if opcode not in OPS or proof not in PROOFS:
        raise ValueError("unknown pattern opcode or proof")
    op, args, result = OPS[opcode], arguments(args), atom(result)
    if not prove(bank, op, args, result, mode, proof):
        raise ValueError(f"replacement is not justified by an admitted MIR proof for {bank}")
    args, result = normalized_atoms(args, result)
    return spelling(name), condition(when), mode == "speculative", op, args, result, PROOFS[proof]


def activation(node):
    kind, action, when, _, bank, _ = node.children
    kind, action, bank = spelling(kind), quoted(action), spelling(bank)
    if bank not in ACTIONS[kind].get(action, ()):
        raise ValueError("unknown or unsupported action/bank combination")
    return kind, action, condition(when), bank


def parse_document(parser, path, content, kind):
    try:
        root = parser.parse(content.decode("utf-8")).children[0]
    except (parser.LanguageError, UnicodeError) as exc:
        raise ValueError(f"{path}: {exc}") from exc
    if root.symbol != kind:
        raise ValueError(f"{path}: expected a MIR {'policy' if kind == 'PolicyDocument' else 'bank'} document")
    if spelling(root.children[1]) != "1":
        raise ValueError(f"{path}: unsupported MIR policy/bank version")
    return root


def declarations(path, chain):
    declarations = []
    while chain.children:
        if len(declarations) == 128:
            raise ValueError(f"{path}: MIR policy/bank exceeds 128 declarations")
        chain, item = chain.children
        declarations.append(item)
    return reversed(declarations)


def located(path, node, fn):
    try:
        return fn()
    except ValueError as exc:
        raise ValueError(f"{path}:{node.token.line}:{node.token.column}: {exc}") from exc


def compile_sources(parser, source, banks):
    path, content = source
    root = parse_document(parser, path, content, "PolicyDocument")
    activations = []
    for node in declarations(path, root.children[3]):
        row = located(path, node, lambda: activation(node))
        if row in activations:
            raise ValueError(f"{path}: duplicate activation of {row[-1]}")
        activations.append(row)
    if missing := BANKS.keys() - {row[-1] for row in activations}:
        raise ValueError(f"{path}: no activation for MIR bank(s): {', '.join(sorted(missing))}")

    patterns, names, signatures = [], set(), set()
    for bank, (path, content) in banks.items():
        patterns.extend(compile_bank(parser, bank, path, content, names, signatures))
    return activations, patterns


def compile_bank(parser, bank, path, content, names, signatures):
    root = parse_document(parser, path, content, "BankDocument")
    if spelling(root.children[2]) != bank:
        raise ValueError(f"{path}: bank declaration must name {bank}")
    if not root.children[4].children:
        raise ValueError(f"{path}: MIR bank has no patterns")
    for node in declarations(path, root.children[4]):
        row = located(path, node, lambda: rule(node, bank))
        name, *signature = row
        signature = tuple(signature)
        if name in names:
            raise ValueError(f"{path}: duplicate pattern name {name}")
        if signature in signatures:
            raise ValueError(f"{path}: duplicate pattern match and replacement in {name}")
        names.add(name)
        signatures.add(signature)
        yield bank, *row


def cpp_atom(value):
    kind, payload = value
    # This minimum literal needs its portable C++ spelling.
    number = "(-INT64_C(9223372036854775807) - 1)" if payload == -(1 << 63) else str(payload)
    return f"{{AtomKind::{kind}, {number}}}"


def selectors(when):
    return "{" + ", ".join(map(json.dumps, when)) + "}"


def emit_patterns(out, table, patterns):
    out.append(f"inline constexpr Pattern {table}[] = {{")
    for bank, name, when, speculative, opcode, args, result, proof in patterns:
        operands = list(map(cpp_atom, args))
        operands.extend("{}" for _ in range(2 - len(args)))
        out.append(f'    // {bank}: {name}')
        out.append(f'    {{{selectors(when)}, Bank::{(BANKS | PROFILES)[bank]}, {str(speculative).lower()}, Op::{opcode}, '
                   f'{len(args)}, {{{", ".join(operands)}}}, {cpp_atom(result)}, Proof::{proof}}},')
    return "\n".join([*out, "};", ""])


def emit(stamp, activations, patterns):
    out = [stamp, "#pragma once", "", "enum class Bank : unsigned {"]
    for index, name in enumerate((*BANKS.values(), *PROFILES.values())):
        out.append(f"    {name} = 1u << {index},")
    out.extend(["};", "", "inline constexpr Activation activations[] = {"])
    for kind, action, when, bank in activations:
        out.append(f'    {{"{kind}", "{action}", {selectors(when)}, Bank::{BANKS[bank]}}},')
    out.extend(["};", ""])
    return emit_patterns(out, "patterns", patterns)


def read_source(path):
    with path.open("rb") as source:
        content = source.read(16385)
    if len(content) > 16384:
        raise ValueError(f"{path}: MIR policy/bank exceeds 16 KiB")
    return path, content


def read_banks(paths):
    banks = {}
    for path in paths:
        bank = path.stem
        if path.suffix != ".mirp" or bank not in BANKS:
            raise ValueError(f"{path}: unknown MIR bank")
        if bank in banks:
            raise ValueError(f"{path}: duplicate MIR bank {bank}")
        banks[bank] = path
    if missing := BANKS.keys() - banks.keys():
        raise ValueError(f"missing MIR bank(s): {', '.join(sorted(missing))}")
    # An explicit order shared by runtime and bootstrap, independent of argument
    # ordering, mtime, and the directory in which the source was found.
    return {bank: read_source(banks[bank]) for bank in BANKS}


def fingerprint(grammar, source, banks, profile=None):
    policy = hashlib.sha256()
    for name, (_, blob) in [(profile or "core", source), *banks.items()]:
        policy.update(name.encode("ascii") + b"\0")
        policy.update(len(blob).to_bytes(8, "big"))
        policy.update(blob)
    return ("// LLSynth MIR policy database; grammar, policy banks, compiler SHA256: "
            + " ".join((hashlib.sha256(grammar.read_bytes()).hexdigest(), policy.hexdigest(),
                        hashlib.sha256(Path(__file__).read_bytes()).hexdigest())))


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--grammar", type=Path, required=True)
    cli.add_argument("--input", type=Path, required=True)
    cli.add_argument("--bank", type=Path, action="append", default=[])
    cli.add_argument("--profile", choices=PROFILES)
    mode = cli.add_mutually_exclusive_group(required=True)
    mode.add_argument("--parser", type=Path)
    mode.add_argument("--bootstrap", type=Path)
    cli.add_argument("--output", type=Path, required=True)
    args = cli.parse_args()
    try:
        source = read_source(args.input)
        if args.profile and args.bank:
            raise ValueError("--profile cannot be combined with --bank")
        banks = {} if args.profile else read_banks(args.bank)
        stamp = fingerprint(args.grammar, source, banks, args.profile)
        if args.bootstrap:
            text = args.bootstrap.read_text(encoding="utf-8")
            if not text.startswith(stamp + "\n"):
                raise ValueError("stale MIR policy bootstrap table; regenerate with LibStone LLSynth")
        elif args.profile:
            rows = compile_bank(parser_module(args.parser), args.profile, *source, set(), set())
            text = emit_patterns([stamp, "#pragma once", ""], PROFILES[args.profile] + "Patterns", rows)
        else:
            text = emit(stamp, *compile_sources(parser_module(args.parser), source, banks))
        with args.output.open("w", encoding="utf-8", newline="\n") as output:
            output.write(text)
    except (OSError, ValueError) as exc:
        cli.exit(1, f"MIR policy compile failed ({args.input}): {exc}\n")


if __name__ == "__main__":
    main()
