"""Validate the LLSynth MIR policy parse tree and emit a static C++ database."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys


GROUPS = {"arithmetic": "Arithmetic", "boolean": "Boolean", "slots": "Slots"}
ACTIONS = {
    "rewrite": {"fold-identity": {"arithmetic", "boolean"}},
    "hint": {"kernel-hot": {"arithmetic"}, "canonicalized": {"boolean"},
             "local-memory": {"slots"}},
}
OPS = {
    "add.checked": "Add", "sub.checked": "Sub", "mul.checked": "Mul",
    "div.checked": "Div", "mod.checked": "Mod", "eq": "Eq", "ne": "Ne",
    "load": "Load",
}
PROOFS = {"identity": "Identity", "nonzero": "Nonzero", "local-slot": "LocalSlot"}
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


def prove(group, op, args, replacement, mode, proof):
    # Admit only these universally valid checked MIR theorems. A "requires"
    # annotation alone is not evidence. The two conditional theorems additionally
    # require runtime MIR analysis to establish the named proof at the use site.
    if op == "Load":
        return (group == "slots" and not args and replacement == (CAPTURE, "stored")
                and mode == "safe" and proof == "local-slot")
    if len(args) != 2:
        return False
    x, y = args
    z, one = (LITERAL, 0), (LITERAL, 1)
    if proof == "nonzero":
        return (group == "arithmetic" and mode == "speculative" and op == "Div"
                and y[0] == CAPTURE and ((x == y and replacement == one)
                                         or (x == z and replacement == z)))
    if mode != "safe" or proof != "identity":
        return False
    if group == "boolean":
        return (x == y and x[0] == CAPTURE
                and ((op == "Eq" and replacement == one) or (op == "Ne" and replacement == z)))
    if group != "arithmetic":
        return False
    return (
        (op == "Add" and ((y == z and replacement == x) or (x == z and replacement == y)))
        or (op == "Sub" and ((y == z and replacement == x) or (x == y and replacement == z)))
        or (op == "Mul" and ((y == one and replacement == x) or (x == one and replacement == y)))
        or (op == "Div" and y == one and replacement == x)
        or (op == "Mod" and y == one and replacement == z)
    )


def rule(node):
    _, name, when, _, group, _, mode, _, opcode, _, args, _, _, result, _, proof, _ = node.children
    group, mode, opcode, proof = spelling(group), spelling(mode), quoted(opcode), spelling(proof)
    if group not in GROUPS or opcode not in OPS or proof not in PROOFS:
        raise ValueError("unknown pattern class, opcode, or proof")
    op, args, result = OPS[opcode], arguments(args), atom(result)
    if not prove(group, op, args, result, mode, proof):
        raise ValueError("replacement is not justified by an admitted MIR proof")
    return spelling(name), condition(when), GROUPS[group], mode == "speculative", op, args, result, PROOFS[proof]


def activation(node):
    kind, action, when, _, group, _ = node.children
    kind, action, group = spelling(kind), quoted(action), spelling(group)
    if group not in ACTIONS[kind].get(action, ()):
        raise ValueError("unknown or unsupported action/class combination")
    return kind, action, condition(when), GROUPS[group]


def compile_tree(parser, path, content):
    root = parser.parse(content).children[0]
    if root.symbol != "PolicyDocument":
        raise ValueError("expected a MIR policy document, not a wire envelope")
    if spelling(root.children[1]) != "1":
        raise ValueError("unsupported MIR policy version")
    declarations = []
    chain = root.children[3]
    while chain.children:
        if len(declarations) == 128:
            raise ValueError("MIR policy exceeds 128 declarations")
        chain, item = chain.children
        declarations.append(item.children[0])
    activations, patterns = [], []
    names = set()
    for node in reversed(declarations):
        try:
            if node.symbol == "Activation":
                row = activation(node)
                if row in activations:
                    raise ValueError("duplicate activation")
                activations.append(row)
            else:
                row = rule(node)
                if row[0] in names:
                    raise ValueError("duplicate pattern name " + row[0])
                names.add(row[0])
                patterns.append(row)
        except ValueError as exc:
            raise ValueError(f"{path}:{node.token.line}:{node.token.column}: {exc}") from exc
    if not activations or not patterns:
        raise ValueError("MIR policy requires activations and patterns")
    return activations, patterns


def emit(stamp, activations, patterns):
    def selectors(when):
        return "{" + ", ".join(map(json.dumps, when)) + "}"

    out = [stamp, "#pragma once", "", "inline constexpr Activation activations[] = {"]
    for kind, action, when, group in activations:
        out.append(f'    {{"{kind}", "{action}", {selectors(when)}, Group::{group}}},')
    out.extend(["};", "", "inline constexpr Pattern patterns[] = {"])
    for name, when, group, speculative, opcode, args, result, proof in patterns:
        captures = {}

        def encode(value):
            kind, payload = value
            if kind == CAPTURE:
                payload = captures.setdefault(payload, len(captures))
            # This minimum literal needs its portable C++ spelling.
            number = "(-INT64_C(9223372036854775807) - 1)" if payload == -(1 << 63) else str(payload)
            return f"{{AtomKind::{kind}, {number}}}"

        operands = [encode(value) for value in args]
        operands.extend("{}" for _ in range(2 - len(args)))
        out.append(f'    // {name}')
        out.append(f'    {{{selectors(when)}, Group::{group}, {str(speculative).lower()}, Op::{opcode}, '
                   f'{len(args)}, {{{", ".join(operands)}}}, {encode(result)}, Proof::{proof}}},')
    return "\n".join([*out, "};", ""])


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--grammar", type=Path, required=True)
    cli.add_argument("--input", type=Path, required=True)
    mode = cli.add_mutually_exclusive_group(required=True)
    mode.add_argument("--parser", type=Path)
    mode.add_argument("--bootstrap", type=Path)
    cli.add_argument("--output", type=Path, required=True)
    args = cli.parse_args()
    path = args.input
    try:
        with path.open("rb") as source:
            content = source.read(16385)
        if len(content) > 16384:
            raise ValueError("MIR policy exceeds 16 KiB")
        stamp = "// LLSynth MIR policy database; grammar, policy, compiler SHA256: " + " ".join(
            hashlib.sha256(blob).hexdigest()
            for blob in (args.grammar.read_bytes(), content, Path(__file__).read_bytes()))
        if args.bootstrap:
            text = args.bootstrap.read_text(encoding="utf-8")
            if not text.startswith(stamp + "\n"):
                raise ValueError("stale MIR policy bootstrap table; regenerate with LibStone LLSynth")
        else:
            parser = parser_module(args.parser)
            try:
                text = emit(stamp, *compile_tree(parser, path, content.decode("utf-8")))
            except parser.LanguageError as exc:
                raise ValueError(str(exc)) from exc
        with args.output.open("w", encoding="utf-8", newline="\n") as output:
            output.write(text)
    except (OSError, ValueError) as exc:
        cli.exit(1, f"MIR policy compile failed ({path}): {exc}\n")


if __name__ == "__main__":
    main()
