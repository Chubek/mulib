#!/usr/bin/env python3
"""Check generated WebIDL C99, CST byte roundtrips and AST/CodeYAML agreement."""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess
import tempfile


def sexpr(text):
    tokens = re.findall(r'"(?:\\.|[^"\\])*"|[()]|[^\s()]+', text)
    pos = 0

    def parse():
        nonlocal pos
        token = tokens[pos]
        pos += 1
        if token == '(':
            result = []
            while tokens[pos] != ')':
                result.append(parse())
            pos += 1
            return result
        return json.loads(token) if token.startswith('"') else token

    result = parse()
    assert pos == len(tokens)
    return result


def yaml_node(text):
    # Parse exactly the documented CodeYAML node schema, with no dependencies.
    lines = text.splitlines()
    pos = 0

    def parse(indent):
        nonlocal pos
        prefix = ' ' * indent
        assert lines[pos].startswith(prefix + 'kind: ')
        result = {'kind': lines[pos][indent + 6:], 'children': []}
        pos += 1
        if lines[pos].startswith(prefix + 'value: '):
            result['value'] = json.loads(lines[pos][indent + 7:])
            pos += 1
        child_line = lines[pos]
        pos += 1
        assert child_line in (prefix + 'children:', prefix + 'children: []')
        if child_line.endswith('[]'):
            return result
        while pos < len(lines) and lines[pos] == prefix + '  -':
            pos += 1
            result['children'].append(parse(indent + 4))
        assert result['children']
        return result

    result = parse(0)
    assert pos == len(lines)
    return result


def ast_node(node):
    result = {'kind': node[0], 'children': []}
    children = node[1:]
    if children and isinstance(children[0], str):
        result['value'] = children.pop(0)
    result['children'] = [ast_node(child) for child in children]
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emitter', required=True)
    parser.add_argument('--compiler', default='cc')
    args = parser.parse_args()

    def emit(flag):
        return subprocess.check_output([args.emitter, flag])

    source = emit('--source')
    cst = sexpr(emit('--emit-cst').decode('utf-8'))
    assert cst[0] == 'webidl-cst'
    offset, parts = 0, []
    for kind, start, raw in cst[1:]:
        assert kind in {'identifier', 'number', 'string', 'punctuation', 'comment', 'whitespace'}
        assert int(start) == offset
        encoded = raw.encode('utf-8')
        offset += len(encoded)
        parts.append(encoded)
    assert b''.join(parts) == source
    assert ast_node(sexpr(emit('--emit-ast').decode('utf-8'))) == yaml_node(
        emit('--emit-yaml').decode('utf-8'))

    with tempfile.TemporaryDirectory(prefix='webidl-', dir='/tmp/opencode') as tmp:
        path = Path(tmp) / 'generated.c'
        path.write_bytes(emit('--emit') + b'''
void webidl_generated_typecheck(void)
{
  webidl_Counts counts = webidl_List__webidl_counts__get((webidl_List *)0);
  webidl_List__webidl_counts__set((webidl_List *)0, counts);
  webidl_Options options = {0};
  webidl_Handler handler = options.webidl_handler;
  (void)handler;
  double pi = webidl_Sample__webidl_PI__constant;
  (void)pi;
  uint64_t maximum = webidl_Sample__webidl_MAX__constant;
  int64_t minimum = webidl_Sample__webidl_MIN__constant;
  double infinity = webidl_Sample__webidl_INF__constant;
  (void)maximum; (void)minimum; (void)infinity;
}
''')
        subprocess.run(shlex.split(args.compiler) + [
            '-std=c99', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-c', str(path),
            '-o', str(Path(tmp) / 'generated.o')], check=True)
    print('WebIDL: strict C99, CST roundtrip and AST/CodeYAML agreement passed')


if __name__ == '__main__':
    main()
