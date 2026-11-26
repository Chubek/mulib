#!/usr/bin/env python3
"""Read a LibStone LLSynth 0.7.0 Python artifact as data; emit an On1x table module."""

import argparse
import base64
import binascii
import json
from pathlib import Path
import re
import struct
import sys


SPEC = re.compile(rb'^SPEC = json\.loads\(base64\.b64decode\("([A-Za-z0-9+/=]+)"\)\)$',
                  re.MULTILINE)
MAX_ARTIFACT = 16 * 1024 * 1024
MAX_TABLE = 512 * 1024


def require(condition, message):
    if not condition:
        raise ValueError(message)


def integer(value, upper):
    require(type(value) is int and 0 <= value < upper, "invalid table integer")
    return value


def sequence(value):
    require(isinstance(value, list), "expected a table array")
    return value


def mapping(value):
    require(isinstance(value, dict), "expected a table object")
    return value


def unique_fields(pairs):
    result = dict(pairs)
    require(len(result) == len(pairs), "duplicate SPEC field")
    return result


def pack(spec):
    require(spec["parser_algorithm"] in ("lr1", "lalr1"), "unsupported parser")
    actions, gotos = sequence(spec["actions"]), sequence(spec["gotos"])
    productions = sequence(spec["productions"])
    dfa = mapping(spec["lexdfa"])
    states, rules = sequence(dfa["states"]), sequence(dfa["rules"])
    require(0 < len(states) < 4096 and 0 < len(actions) == len(gotos) <= 3000
            and 1 < len(productions) <= MAX_TABLE // 8, "invalid table dimensions")
    terminal_names = sequence(spec["terminals"])
    require(all(isinstance(s, str) and s for s in terminal_names) and
            len(set(terminal_names)) == len(terminal_names) and
            "EOF" in terminal_names and "$" not in terminal_names, "invalid terminal names")
    terminals = {"EOF": 1, "$": 2}
    for name in sorted(set(terminal_names) - {"EOF"}):
        terminals[name] = len(terminals) + 1
    nonterminals = sorted(mapping(spec["nonterminals"]))
    require(not set(nonterminals) & terminals.keys(), "terminal/nonterminal name collision")
    symbols = {name: len(terminals) + i + 1 for i, name in enumerate(nonterminals)}

    # Header, production pairs, two row directories, then both sets of rows.
    words = [int.from_bytes(b"LLS\x01", "little"), len(states), len(actions),
             len(productions), len(terminals)]
    for i, production in enumerate(productions):
        require(integer(production["id"], len(productions)) == i, "unordered productions")
        rhs = sequence(production["rhs"])
        require(all(name in terminals or name in symbols for name in rhs),
                "unknown production symbol")
        words.extend((symbols[production["lhs"]], len(rhs)))
    lex_rows = len(words)
    parse_rows = lex_rows + len(states) + 1
    words.extend([0] * (len(states) + len(actions) + 2))

    lexical_rules = []
    for rule in rules:
        skip, bol, eol = (integer(rule[flag], 2) for flag in ("skip", "bol", "eol"))
        token = 0 if skip else terminals[rule["name"]]
        require(token == 0 or token > 2, "lexer defines a reserved token")
        lexical_rules.append(token * 4 + bol + 2 * eol)
    for i, state in enumerate(states):
        words[lex_rows + i] = len(words)
        accepts = sequence(state["accept"])
        require(accepts == sorted(set(accepts)), "unordered lexer accepts")
        words.append(len(accepts))
        words.extend(lexical_rules[integer(a, len(rules))] for a in accepts)
        previous = -1
        for low, high, target in sequence(state["trans"]):
            integer(low, 256)
            integer(high, 256)
            require(previous < low <= high, "overlapping lexer ranges")
            previous = high
            words.extend((low + 256 * high, integer(target, len(states))))
        require(len(words) <= MAX_TABLE // 4, "packed tables exceed 512 KiB")
    words[lex_rows + len(states)] = len(words)

    accept_count = 0
    for i, (action_row, goto_row) in enumerate(zip(actions, gotos)):
        words[parse_rows + i] = len(words)
        row = {symbols[key]: 4 * integer(value, len(actions)) + 3
               for key, value in mapping(goto_row).items()}
        for name, (op, arg) in mapping(action_row).items():
            key = terminals[name]
            if op == "s":
                require(name != "$", "shift on end marker")
                row[key] = 4 * integer(arg, len(actions))
            elif op == "r":
                row[key] = 4 * integer(arg, len(productions) - 1) + 1
            elif op == "a":
                require(name == "$" and integer(arg, 1) == 0, "invalid parser accept")
                accept_count += 1
                row[key] = 2
            else:
                # LLSynth non-associative precedence produces a rejection entry.
                require(op == "x" and integer(arg, 1) == 0, "unknown parser action")
        for key, value in sorted(row.items()):
            words.extend((key, value))
        require(len(words) <= MAX_TABLE // 4, "packed tables exceed 512 KiB")
    require(accept_count == 1, "expected one parser accept")
    words[parse_rows + len(actions)] = len(words)
    return struct.pack(f"<{len(words)}I", *words)


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("parser", type=Path, help="Python source emitted by llsynth.pl")
    args = cli.parse_args()
    try:
        with args.parser.open("rb") as stream:
            source = stream.read(MAX_ARTIFACT + 1)
        require(len(source) <= MAX_ARTIFACT, "Python parser artifact exceeds 16 MiB")
        matches = SPEC.findall(source)
        require(len(matches) == 1, "expected one LLSynth 0.7.0 Python SPEC envelope")
        values = json.loads(base64.b64decode(matches[0], validate=True),
                            object_pairs_hook=unique_fields)
        image = pack(mapping(values))
    except (binascii.Error, KeyError, OSError, OverflowError, RecursionError,
            struct.error, TypeError, ValueError) as exc:
        cli.exit(1, f"pack_tables: {exc}\n")
    print("# Syntax-only tables packed from a LibStone LLSynth 0.7.0 artifact.")
    print('fn basis_llsynth_tables() -> buffer { return b!"'
          + "".join(f"\\x{byte:02x}" for byte in image) + '"; }')


if __name__ == "__main__":
    main()
