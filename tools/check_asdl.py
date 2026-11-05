#!/usr/bin/env python3
"""Compare ASDL backend schemas and execute generated standalone strict C99."""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess
import tempfile


def sexpr(text):
    tokens = iter(re.findall(r'"[^"\\]*"|[()]|[^\s()]+', text))

    def read(token):
        if token != '(':
            return json.loads(token) if token.startswith('"') else token
        result = []
        for token in tokens:
            if token == ')':
                return result
            result.append(read(token))
        raise AssertionError('unclosed list')

    value = read(next(tokens))
    assert next(tokens, None) is None
    return value


def schema(tree):
    def fields(node):
        assert node[0] == 'fields'
        result = []
        for tag, typ, name, cardinality in node[1:]:
            assert tag == 'field'
            result.append(dict(type=typ, name=None if name == [] else name,
                               cardinality=cardinality))
        return result

    assert tree[0] == 'asdl'
    types = []
    for node in tree[2:]:
        typ = dict(kind=node[0], name=node[1])
        if node[0] == 'product':
            typ['fields'] = fields(node[2])
        else:
            assert node[2][0] == 'constructors' and node[3][0] == 'attributes'
            typ['constructors'] = []
            for tag, name, number, fs in node[2][1:]:
                assert tag == 'constructor'
                typ['constructors'].append(dict(name=name, tag=int(number), fields=fields(fs)))
            typ['attributes'] = fields(node[3][1])
        types.append(typ)
    return dict(name=tree[1], types=types)


CLIENT = r'''
#include "generated.h"
#include "generated.h"
#include <assert.h>
int main(void) {
  test_opt_int none = {0, 0}, zero = {1, 0};
  test_opt_string file = {1, {"tree.asdl", 9}};
  test_t_pos pos = test_make_pos(file, 1, 2);
  test_t_exp num = test_make_exp_Num(INT64_MIN, zero);
  test_t_exp id = test_make_exp_Id((test_t_identifier){"x", 1}, none);
  test_t_exp op = test_make_exp_Op(num, test_make_binop_Plus(), id, none);
  test_t_exp empty = test_make_exp_Empty(none);
  test_t_exp items[] = {op, empty};
  test_seq_exp args = {2, items};
  test_t_stm print = test_make_stm_Print(args, pos);
  test_t_stm assign = test_make_stm_Assign((test_t_identifier){"result", 6}, op, pos);
  test_t_stm compound = test_make_stm_Compound(assign, print, pos);
  test_t_int values[] = {INT64_MAX, INT64_MIN};
  test_t_keyword keyword = test_make_keyword(42, (test_t_string){"a\0b", 3}, 7, 8,
      (test_opt_binop){1, test_make_binop_Minus()}, (test_seq_int){2, values});
  test_t_node_uname named = test_make_node_uname_With_uunder(keyword, file);
  test_t_node_uname nullary = test_make_node_uname_Withunder(file);
  test_t_node_uuname other = test_make_node_uuname_Other();
  assert(pos && num && id && op && empty && print && assign && compound && keyword && named && nullary);
  assert(compound->v.c_Compound.f_head == assign && compound->v.c_Compound.f_next == print);
  assert(num->kind == test_tag_exp_Num && num->v.c_Num.f_value == INT64_MIN);
  assert(num->a_line.present && num->a_line.value == 0 && !id->a_line.present);
  assert(op->v.c_Op.f_1 == num && op->v.c_Op.f_2 == test_tag_binop_Plus && op->v.c_Op.f_3 == id);
  assert(print->a_p == pos && print->v.c_Print.f_args.count == 2);
  assert(print->v.c_Print.f_args.items[1] == empty && empty->kind == test_tag_exp_Empty);
  assert(keyword->f_switch == 42 && keyword->f_inline.length == 3 && !keyword->f_inline.data[1]);
  assert(keyword->f_3 == 7 && keyword->f_int1 == 8 && keyword->f_operator.present);
  assert(keyword->f_values.items[0] == INT64_MAX && keyword->f_values.items[1] == INT64_MIN);
  assert(named->v.c_With_uunder.f_while == keyword && named->a_File.present);
  assert(other == test_tag_node_uuname_Other && test_tag_binop_Div == 4);
  free(named); free(nullary); free(keyword); free(compound); free(assign); free(print);
  free(pos); free(op); free(num); free(id); free(empty);
  return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emitter', required=True)
    parser.add_argument('--compiler', default='cc')
    parser.add_argument('--sanitizers', action='store_true')
    args = parser.parse_args()

    def emit(flag):
        return subprocess.check_output([args.emitter, flag]).decode()

    tree = schema(sexpr(emit('--emit-sexpr')))
    yantlr = emit('--emit-yantlr')
    assert yantlr.startswith('intermediary "Trees";\nset Schema ') and yantlr.endswith(';\n')
    assert tree == json.loads(yantlr.split('set Schema ', 1)[1][:-2])
    with tempfile.TemporaryDirectory(prefix='asdl-', dir='/tmp/opencode') as tmp:
        path = Path(tmp)
        (path / 'generated.h').write_text(emit('--emit-c'))
        (path / 'client.c').write_text(CLIENT)
        flags = ['-std=c99', '-Wall', '-Wextra', '-Wpedantic', '-Werror']
        if args.sanitizers:
            flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                      '-fno-omit-frame-pointer']
        subprocess.run(shlex.split(args.compiler) + flags + [str(path / 'client.c'),
                       '-o', str(path / 'client')], check=True)
        subprocess.run([str(path / 'client')], check=True)
        (path / 'generated.h').write_text(emit('--emit-empty'))
        (path / 'client.c').write_text('#include "generated.h"\nint main(void) { return 0; }\n')
        subprocess.run(shlex.split(args.compiler) + flags + [str(path / 'client.c'),
                       '-o', str(path / 'empty')], check=True)
        subprocess.run([str(path / 'empty')], check=True)
    print('ASDL: backend schema agreement and standalone strict C99 runtime passed')


if __name__ == '__main__':
    main()
