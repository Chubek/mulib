"""Bootstrap byte-string S-expression codec for On1x PPX protocol v1.

Wire strings are ASCII with hex-escaped bytes. Python str values represent
those bytes one-to-one (Latin-1), not an implicit Unicode normalization.
"""
MAX_BYTES = 4 * 1024 * 1024
MAX_NODES = 262144
MAX_DEPTH = 256


class Atom(str):
    """An unquoted symbol, distinct from a quoted string."""


def loads(text):
    if isinstance(text, bytes):
        text = text.decode('ascii')
    if len(text) > MAX_BYTES:
        raise ValueError('S-expression byte limit exceeded')
    pos = count = 0

    def error(message):
        line = text.count('\n', 0, pos) + 1
        column = pos - text.rfind('\n', 0, pos)
        raise ValueError(f'S-expression {line}:{column}: {message}')

    def space():
        nonlocal pos
        while pos < len(text) and text[pos] in ' \t\r\n':
            pos += 1

    def read(depth):
        nonlocal pos, count
        space()
        count += 1
        if depth >= MAX_DEPTH or count > MAX_NODES:
            error('tree limit exceeded')
        if pos == len(text):
            error('unexpected end')
        c = text[pos]
        pos += 1
        if c == '(':
            out = []
            while True:
                space()
                if pos == len(text):
                    error('unterminated list')
                if text[pos] == ')':
                    pos += 1
                    return out
                out.append(read(depth + 1))
        if c == ')':
            error("unexpected ')'")
        if c == '"':
            out = []
            while pos < len(text):
                c = text[pos]
                pos += 1
                if c == '"':
                    return ''.join(out)
                if c == '\\':
                    if pos == len(text):
                        error('unterminated escape')
                    c = text[pos]
                    pos += 1
                    if c == 'x':
                        digits = text[pos:pos + 2]
                        if len(digits) != 2 or any(d not in '0123456789abcdefABCDEF' for d in digits):
                            error('expected two hex digits')
                        c = chr(int(digits, 16))
                        pos += 2
                    elif c in 'nrt':
                        c = {'n': '\n', 'r': '\r', 't': '\t'}[c]
                    elif c not in '\\"':
                        error('unknown escape')
                elif not 32 <= ord(c) <= 126:
                    error('strings must escape control/non-ASCII bytes')
                out.append(c)
            error('unterminated string')
        start = pos - 1
        while pos < len(text) and text[pos] not in '() \t\r\n':
            pos += 1
        value = text[start:pos]
        if any(not 33 <= ord(c) <= 126 or c == '"' for c in value):
            error('invalid symbol byte')
        return Atom(value)

    tree = read(0)
    space()
    if pos != len(text):
        error('trailing input')
    return tree


def dumps(tree):
    chunks = []
    size = count = 0

    def put(s):
        nonlocal size
        size += len(s)
        if size >= MAX_BYTES:
            raise ValueError('S-expression output byte limit exceeded')
        chunks.append(s)

    def visit(node, depth):
        nonlocal count
        count += 1
        if depth >= MAX_DEPTH or count > MAX_NODES:
            raise ValueError('S-expression output tree limit exceeded')
        if isinstance(node, list):
            put('(')
            for i, child in enumerate(node):
                if i:
                    put(' ')
                visit(child, depth + 1)
            put(')')
        elif isinstance(node, Atom):
            if not node or any(not 33 <= ord(c) <= 126 or c in '()"' for c in node):
                raise ValueError('invalid symbol')
            put(node)
        elif isinstance(node, str):
            put('"')
            for c in node:
                if ord(c) > 255:
                    raise ValueError('wire strings are bytes; encode Unicode explicitly')
                put('\\' + c if c in '\\"' else c if 32 <= ord(c) <= 126 else f'\\x{ord(c):02x}')
            put('"')
        else:
            raise ValueError('expected list, Atom, or byte string')
    visit(tree, 0)
    put('\n')
    return ''.join(chunks)
