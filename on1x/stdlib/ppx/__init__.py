"""Bootstrap PPX authoring tools. This is Python-hosted, not self-hosted On1x."""
import json
import os
from pathlib import Path
import re
import sys
from sexpr import Atom, MAX_BYTES, MAX_DEPTH, MAX_NODES, loads, dumps
from visitor import rewrite, walk, tag

KEYWORDS = {'fn', 'let', 'var', 'if', 'else', 'while', 'return', 'true', 'false', 'extern', 'import', 'forall', 'macro', 'rule', 'quote', 'splice', 'lift'}


def name(s):
    return type(s) is str and bool(re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*', s)) and s not in KEYWORDS


def _unique(pairs):
    out = {}
    for k, v in pairs:
        if k in out:
            raise ValueError('duplicate schema JSON key: ' + k)
        out[k] = v
    return out


class Schema:
    def __init__(self):
        path = Path(__file__).resolve().parents[2] / 'grammar/PPX.schema.json'
        data = json.loads(path.read_text(), object_pairs_hook=_unique)
        if data['version'] != 1 or data['schema_format'] != 'on1x.sexpr-ast/v1':
            raise ValueError('unsupported schema version')
        self.parents, self.nodes = data['categories'], data['nodes']

    def extend(self, label, path):
        data = json.loads(Path(path).read_text(), object_pairs_hook=_unique)
        if data['version'] != 1:
            raise ValueError('unsupported extension version')
        for constructor, definition in data['nodes'].items():
            if not constructor.startswith(label + ':') or constructor in self.nodes:
                raise ValueError('invalid/redefined extension constructor')
            if definition['category'] not in self.parents or len(self.nodes) >= 1024:
                raise ValueError('invalid extension category/size')
            self.nodes[constructor] = definition

    def subtype(self, child, parent):
        for _ in range(len(self.parents) + 1):
            if child == parent:
                return True
            child = self.parents.get(child)
            if not child:
                return False
        return False

    def validate(self, tree):
        count = 0

        def scalar(n, kind):
            quoted = type(n) is str
            if not isinstance(n, str) or '\0' in n:
                return False
            if kind == 'Bytes':
                return quoted
            if kind == 'File':
                return quoted and len(n) <= 4096 and all(ord(c) >= 32 and ord(c) != 127 for c in n)
            if kind == 'Name':
                return name(n)
            if kind == 'Version':
                return not quoted and n == '1'
            if kind in ('Nat', 'Positive'):
                return not quoted and bool(re.fullmatch('[0-9]+', n)) and int(n) <= (1 << 64) - 1 and (kind != 'Positive' or int(n) > 0)
            if kind == 'Boolean':
                return not quoted and n in ('true', 'false')
            if kind == 'UnaryOperator':
                return not quoted and n in ('!', '-')
            if kind == 'BinaryOperator':
                return not quoted and n in ('+', '-', '*', '/', '%', '<', '>', '<=', '>=', '==', '!=', '&&', '||')
            if kind == 'DecimalSpelling':
                return quoted and bool(re.fullmatch('-?[0-9]+', n))
            if kind == 'HexSpelling':
                return quoted and bool(re.fullmatch('0[xX][0-9a-fA-F]+', n))
            return False

        def visit(n, kind, path, depth):
            nonlocal count
            count += 1
            if count > MAX_NODES or depth >= MAX_DEPTH:
                raise ValueError(path + ': tree limit exceeded')
            if kind.endswith('*'):
                if not isinstance(n, list):
                    raise ValueError(path + ': expected list')
                for i, child in enumerate(n):
                    visit(child, kind[:-1], f'{path}[{i}]', depth + 1)
            elif kind not in self.parents:
                if not scalar(n, kind):
                    raise ValueError(path + ': expected ' + kind)
            else:
                definition = self.nodes.get(tag(n))
                if definition is None or not self.subtype(definition['category'], kind):
                    raise ValueError(path + ': expected ' + kind + ' constructor')
                fields = definition['fields']
                if len(n) != len(fields) + 1:
                    raise ValueError(path + ': wrong field count')
                for child, (field, field_type) in zip(n[1:], fields):
                    visit(child, field_type, path + '.' + field, depth + 1)
        visit(tree, 'Unit', '$', 0)


def node(constructor, *fields):
    return [Atom(constructor), *fields]


def integer(value, location):
    return node('DecimalLiteral', str(value), location)


def ident(value, location):
    return node('Identifier', value, location)


def call(function, arguments, location):
    return node('Call', ident(function, location), arguments, location)


class FreshNames:
    def __init__(self, tree):
        self.used = {str(n) for n, _ in walk(tree) if isinstance(n, str)}
        self.serial = 0

    def new(self):
        while True:
            self.serial += 1
            candidate = f'on1x_ppx_{self.serial}'
            if candidate not in self.used:
                self.used.add(candidate)
                return candidate


def serve(transform, extension_schemas=()):
    """One request/one response. Diagnostics go only to stderr."""
    try:
        schema = Schema()
        for label, path in extension_schemas:
            schema.extend(label, path)
        data = sys.stdin.buffer.read(MAX_BYTES + 1)
        tree = loads(data)
        schema.validate(tree)
        label = os.environ.get('ON1X_PPX_LABEL', '')
        result = transform(tree, label)
        schema.validate(result)
        sys.stdout.write(dumps(result))
    except (ValueError, KeyError, IndexError, OSError, UnicodeError, RecursionError) as error:
        print('On1x PPX: ' + str(error), file=sys.stderr)
        raise SystemExit(1)
