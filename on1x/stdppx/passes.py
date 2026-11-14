"""Six small, real AST processors for the current On1x source language."""
from copy import deepcopy
from sexpr import Atom
from ppx import node, integer, ident, call, rewrite, tag, FreshNames


def identity(tree, label):
    return tree


def _extension(n, label):
    return tag(n) == 'ExtensionExpr' and n[1] == label


def checks(tree, label):
    fresh = FreshNames(tree)
    wrapper, argument = fresh.new(), fresh.new()
    location = None

    def apply(n, path):
        nonlocal location
        if not _extension(n, label):
            return n
        _, _, operation, args, at = n
        if operation not in ('assert_eq', 'assert_ne') or len(args) != 2:
            raise ValueError('checks expects assert_eq/assert_ne with two expressions')
        comparison = node('Binary', Atom('==' if operation == 'assert_eq' else '!='), *args, at)
        location = at
        return call(wrapper, [comparison], at)
    result = rewrite(tree, apply)
    if location is not None:
        # Resolve the intrinsic in a fresh global wrapper, not in the caller's
        # potentially shadowing local scope. FreshNames reserves every input name.
        parameter = node('Parameter', argument, node('BoolType'), location)
        body = node('Block', [node('Do', call('assert', [ident(argument, location)], location), location)], location)
        result[2].append(node('Function', wrapper, [parameter], node('VoidType'), body, [], location))
    return result


def control(tree, label):
    def apply(n, path):
        if tag(n) != 'Do' or not _extension(n[1], label):
            return n
        _, _, operation, args, at = n[1]
        if operation not in ('when', 'unless') or len(args) != 2 or tag(args[1]) != 'Block':
            raise ValueError('control expects when/unless(condition, Block) in statement position')
        condition = args[0]
        if operation == 'unless':
            condition = node('Unary', Atom('!'), condition, at)
        return node('If', condition, args[1], node('Block', [], at), at)
    return rewrite(tree, apply)


def unroll(tree, label):
    def apply(n, path):
        if tag(n) != 'Do' or not _extension(n[1], label):
            return n
        _, _, operation, args, at = n[1]
        if operation != 'repeat' or len(args) != 2 or tag(args[0]) not in ('DecimalLiteral', 'HexLiteral') or tag(args[1]) != 'Block':
            raise ValueError('unroll expects repeat(IntegerLiteral, Block) in statement position')
        count = int(args[0][1], 16 if tag(args[0]) == 'HexLiteral' else 10)
        if not 0 <= count <= 256:
            raise ValueError('unroll count must be 0..256')
        body = [node('Scope', deepcopy(args[1]), at) for _ in range(count)]
        return node('Scope', node('Block', body, at), at)
    return rewrite(tree, apply)


def _type(t):
    k = tag(t)
    scalars = {'I64Type': 'i64', 'BoolType': 'bool', 'StringType': 'string', 'VoidType': 'void',
               'JsonType': 'json', 'JsonViewType': 'json_view', 'SExprType': 'sexpr',
               'BufferType': 'buffer', 'HoleType': '_', 'Inferred': '_'}
    if k in scalars:
        return scalars[k]
    if k == 'BoundType':
        return 'a' + t[1]
    if k == 'CodeType':
        return 'code<' + _type(t[1]) + '>'
    if k == 'ForallType':
        return 'forall(' + ', '.join('a' + str(i) for i in range(int(t[1]))) + ') ' + _type(t[2])
    if k == 'TupleType':
        return '(' + ', '.join(map(_type, t[1])) + ')'
    if k == 'FunctionType':
        return 'fn(' + ', '.join(map(_type, t[1])) + ') -> ' + _type(t[2])
    raise ValueError('unhandled signature type: ' + str(k))


def derive(tree, label):
    additions = []
    names = {function[1] for function in tree[2]}
    for function in tree[2]:
        attributes = function[4 if tag(function) == 'Extern' else 5]
        keep = []
        for attribute in attributes:
            if attribute[1] != label or attribute[2] != 'derive':
                keep.append(attribute)
                continue
            for request in attribute[3]:
                if tag(request) != 'StringLiteral' or request[1] not in ('arity', 'signature'):
                    raise ValueError('derive supports string requests arity/signature')
                derived_name = function[1] + '_' + request[1]
                if derived_name in names:
                    raise ValueError('derived function name collision: ' + derived_name)
                names.add(derived_name)
                at = attribute[4]
                if request[1] == 'arity':
                    result_type, result = node('I64Type'), integer(len(function[2]), at)
                else:
                    signature = 'fn(' + ', '.join(_type(p[2]) for p in function[2]) + ') -> ' + _type(function[3])
                    result_type, result = node('StringType'), node('StringLiteral', signature, at)
                additions.append(node('Function', derived_name, [], result_type,
                                      node('Block', [node('Return', result, at)], at), [], at))
        attributes[:] = keep
    tree[2].extend(additions)
    return tree


def fold(tree, label):
    low, high = -(1 << 63), (1 << 63) - 1

    def apply(n, path):
        if tag(n) != 'Binary' or n[1] not in ('+', '-', '*', '/', '%'):
            return n
        if any(tag(c) not in ('DecimalLiteral', 'HexLiteral') for c in n[2:4]):
            return n
        a, b = [int(c[1], 16 if tag(c) == 'HexLiteral' else 10) for c in n[2:4]]
        if not low <= a <= high or not low <= b <= high:
            return n
        if n[1] in ('/', '%'):
            if b == 0 or (a == low and b == -1):
                return n
            quotient = (abs(a) // abs(b)) * (-1 if (a < 0) != (b < 0) else 1)
            result = quotient if n[1] == '/' else a - quotient * b
        else:
            result = {'+': lambda: a + b, '-': lambda: a - b, '*': lambda: a * b}[n[1]]()
        return integer(result, n[4]) if low <= result <= high else n
    return rewrite(tree, apply)
