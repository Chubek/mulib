"""Read Cephyr's Archon MachineIR v1 and verify evidence for scoped directives."""

from dataclasses import dataclass
import re

from generate_portable_recipe import DOCUMENT_LIMIT, require


MIR_TYPES = {"void", "i64", "ptr", "func_ptr", "f32", "f64", "f80", "f128"}
DECIMAL = re.compile(r"(?:0|[1-9][0-9]{0,9})\Z")
WORD = re.compile(r"(?:[0-9]{1,20}|0x[0-9A-Fa-f]{1,16})\Z")
SYMBOL = re.compile(r"[^\x00-\x20\x7f]{1,512}\Z")
# These are exact On1x selectors, not wildcard requests. The evidence comes
# from Archon's exported MIR, not from an arbitrary On1x backend.
TARGETS = {
    "x86_64-linux-gnu": "x86_64-unknown-linux-gnu",
    "aarch64-linux-gnu": "aarch64-unknown-linux-gnu",
    "riscv64-linux-gnu": "riscv64-unknown-linux-gnu",
}


def number(token, limit):
    require(DECIMAL.fullmatch(token) and int(token) <= limit, f"invalid MIR index or count: {token!r}")
    return int(token)


def word(token):
    require(WORD.fullmatch(token), f"invalid MIR word: {token!r}")
    value = int(token, 16 if token.startswith("0x") else 10)
    require(value < 1 << 64, "invalid MIR word size")
    return value


@dataclass(frozen=True)
class Block:
    params: tuple
    instructions: tuple
    terminator: tuple


@dataclass(frozen=True)
class Function:
    result: str
    arity: int
    types: tuple
    locals: tuple
    blocks: tuple


class Reader:
    def __init__(self, document):
        require(len(document) <= DOCUMENT_LIMIT and "\0" not in document, "invalid MIR document size or NUL")
        self.lines = iter(tuple(line.split()) for line in document.splitlines() if line.strip())

    def line(self, name=None, size=None):
        tokens = next(self.lines, ())
        require(tokens and (name is None or tokens[0] == name)
                and (size is None or len(tokens) == size), f"expected MIR record {name or 'instruction'}")
        return tokens

    def function(self):
        _, name, result, arity, values, blocks, locals_count = self.line("function", 7)
        require(SYMBOL.fullmatch(name) and result in MIR_TYPES, "invalid MIR function signature")
        arity, values, blocks, locals_count = (number(x, limit) for x, limit in
                                              zip((arity, values, blocks, locals_count), (128, 131072, 4096, 8192)))
        types = self.line("types", values + 1)[1:]
        require(arity <= values and blocks and set(types) <= MIR_TYPES - {"void"}, "invalid MIR value types")
        locals_ = tuple(tuple(number(x, 0xffffffff) for x in self.line("local", 3)[1:]) for _ in range(locals_count))
        body = []
        for _ in range(blocks):
            _, nparams, ninsts = self.line("block", 3)
            params = tuple(number(x, values - 1) for x in self.line("params", number(nparams, 4096) + 1)[1:])
            instructions = tuple(self.line() for _ in range(number(ninsts, 131072)))
            terminator = self.line()
            require(terminator[0] in ("br", "cbr", "ret", "trap"), "unknown MIR terminator")
            body.append(Block(params, instructions, terminator))
        return name, Function(result, arity, types, locals_, tuple(body))

    def read(self):
        self.line("archon-mir", 2)
        _, functions, imports, globals_ = self.line("module", 4)
        parsed = {}
        for _ in range(number(functions, 16384)):
            symbol, function = self.function()
            require(symbol not in parsed, f"duplicate MIR symbol {symbol}")
            parsed[symbol] = function
        other_symbols = set(parsed)
        for _ in range(number(imports, 16384)):
            record = self.line("import")
            require(len(record) >= 4, "truncated MIR import")
            _, symbol, result, count, *params = record
            require(SYMBOL.fullmatch(symbol) and symbol not in other_symbols and result in MIR_TYPES
                    and number(count, 128) == len(params) and set(params) <= MIR_TYPES - {"void"},
                    "invalid MIR import")
            other_symbols.add(symbol)
        for _ in range(number(globals_, 16384)):
            record = self.line("global")
            require(len(record) >= 4, "truncated MIR global")
            _, symbol, count, init, *values = record
            count = number(count, 131072)
            require(SYMBOL.fullmatch(symbol) and symbol not in other_symbols
                    and (init == "zero" and not values or init == "init" and len(values) == count),
                    "invalid MIR global")
            for value in values:
                word(value)
            other_symbols.add(symbol)
        self.line("end", 1)
        require(not next(self.lines, ()), "trailing MIR records")
        return parsed


def read_mir(document):
    require(document.splitlines()[:1] == ["archon-mir 1"], "unsupported Cephyr MIR schema")
    return Reader(document).read()


def verified_evidence(fn, arity):
    """Verify the closed scalar MIR subset before interpreting its features.

    This checks wire format, SSA dominance and edge types, and only admits
    scalar operations or bounded, definitely initialized stack slots. It is
    deliberately not a proof of the C body's arithmetic totality or a claim
    about performance, source-level ownership, or any different definition.
    """
    require(fn.result == "i64" and fn.arity == arity and fn.types[:arity] == ("i64",) * arity,
            "C/Archon MIR function signatures differ")
    require(len(fn.types) <= 4096 and len(fn.blocks) <= 256 and len(fn.locals) <= 128
            and sum(len(block.instructions) for block in fn.blocks) <= 4096,
            "MIR function exceeds evidence budget")
    require(all(size == align == 8 for size, align in fn.locals), "unsupported MIR local object")
    require(not fn.blocks[0].params, "unexpected MIR entry block parameters")

    defs = set(range(arity))
    addresses, constants = {}, {}
    incoming = [[] for _ in fn.blocks]
    outgoing = [None for _ in fn.blocks]
    predecessors = [set() for _ in fn.blocks]
    load_count = store_count = identities = 0

    def value(token, want="i64"):
        index = number(token, len(fn.types) - 1)
        require(fn.types[index] == want, "invalid MIR operand type")
        return index

    def edge(tokens):
        require(len(tokens) >= 2, "truncated MIR edge")
        target = number(tokens[0], len(fn.blocks) - 1)
        count = number(tokens[1], len(fn.types))
        params = fn.blocks[target].params
        require(len(tokens) >= 2 + count and count == len(params) and target != 0
                and all(fn.types[p] == "i64" for p in params), "invalid MIR edge arguments")
        return target, tuple(value(x) for x in tokens[2:2 + count]), tokens[2 + count:]

    for i, block in enumerate(fn.blocks):
        name, *tokens = block.terminator
        condition = None
        edges = []
        if name == "ret":
            require(len(tokens) == 1, "invalid MIR return")
            condition = value(tokens[0])
        elif name in ("cbr", "br"):
            if name == "cbr":
                require(tokens, "missing MIR branch condition")
                condition = value(tokens.pop(0))
            for _ in range(2 if name == "cbr" else 1):
                target, args, tokens = edge(tokens)
                edges.append((target, args))
                predecessors[target].add(i)
            require(not tokens, "trailing MIR branch operands")
        else:
            require(False, "MIR terminator is not a branch or return")
        outgoing[i] = (condition, edges)

    pending = [set(items) for items in predecessors]
    queue = [0]
    visited = set()
    for index in queue:
        visited.add(index)
        block = fn.blocks[index]
        available, initialized = (set.intersection(*(v for v, _ in incoming[index])),
                                  set.intersection(*(s for _, s in incoming[index]))) if incoming[index] else (
                                      set(range(arity)), set())

        def define(dst):
            require(dst not in defs, "duplicate MIR SSA definition")
            defs.add(dst)
            available.add(dst)

        def operand(token, want="i64"):
            index = value(token, want)
            require(index in available, "MIR operand is not dominated by its SSA definition")
            return index

        def slot(token):
            index = operand(token, "ptr")
            require(index in addresses, "MIR memory access does not use a local address")
            return addresses[index]

        for param in block.params:
            define(param)
        for op, *args in block.instructions:
            if op == "const" and len(args) == 2:
                dst = value(args[0])
                constants[dst] = word(args[1])
            elif op == "frame_addr" and len(args) == 2:
                dst = value(args[0], "ptr")
                addresses[dst] = number(args[1], len(fn.locals) - 1)
            elif op == "copy" and len(args) == 2:
                dst = value(args[0])
                src = operand(args[1])
                if src in constants:
                    constants[dst] = constants[src]
            elif op in {"add", "sub", "add_checked_s", "sub_checked_s", "eq", "slt", "ult",
                        "and", "or", "xor"} and len(args) == 3:
                dst = value(args[0])
                a, b = operand(args[1]), operand(args[2])
                identities += int((op in {"add", "sub", "add_checked_s", "sub_checked_s"} and constants.get(b) == 0)
                                  or (op in {"add", "add_checked_s"} and constants.get(a) == 0)
                                  or (op in {"eq", "slt"} and a == b))
            elif op in {"trunc", "sext", "zext"} and len(args) == 3:
                dst = value(args[0])
                src = operand(args[1])
                width = number(args[2], 64)
                require(width >= 1, "invalid MIR integer conversion width")
                if src in constants:
                    mask = (1 << width) - 1
                    low = constants[src] & mask
                    if op == "sext" and low & (1 << (width - 1)):
                        low |= ((1 << 64) - 1) ^ mask
                    constants[dst] = low
            elif op == "load" and len(args) == 3:
                dst = value(args[0])
                require(slot(args[1]) in initialized and word(args[2]) == 0,
                        "MIR load is not an initialized, in-bounds i64 local access")
                load_count += 1
            elif op == "store" and len(args) == 3:
                require(word(args[2]) == 0, "MIR store displacement is not zero")
                initialized.add(slot(args[0]))
                operand(args[1])
                store_count += 1
                continue
            else:
                require(False, f"unsupported MIR instruction {op!r}")
            define(dst)
        test, edges = outgoing[index]
        require(test is None or test in available, "MIR return or branch operand is not dominated")
        for target, args in edges:
            require(set(args) <= available, "MIR edge argument is not dominated")
        for target in {target for target, _ in edges}:
            incoming[target].append((available, initialized))
            pending[target].remove(index)
            if not pending[target]:
                queue.append(target)
    require(len(visited) == len(fn.blocks) and len(defs) == len(fn.types),
            "cyclic, disconnected, or incomplete MIR SSA")
    return {"local_loads": load_count, "local_stores": store_count, "identity_sites": identities}


def directives(target, evidence):
    if target not in TARGETS:
        return []
    selector = f"(backend mir-archon) (target {TARGETS[target]}) (speculative)"
    forms = []
    if evidence["local_loads"] and evidence["local_stores"]:
        forms.append(f"(hint local-memory {selector})")
    if evidence["identity_sites"]:
        forms.append(f"(rewrite fold-identity {selector})")
    return forms
