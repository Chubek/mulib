"""Bounded, postorder, persistent walkers for bootstrap PPX syntax trees."""
from sexpr import MAX_DEPTH, MAX_NODES, Atom


def tag(node):
    return str(node[0]) if isinstance(node, list) and node and isinstance(node[0], Atom) else None


def rewrite(tree, transform):
    count = 0

    def visit(node, path, depth):
        nonlocal count
        count += 1
        if count > MAX_NODES or depth >= MAX_DEPTH:
            raise ValueError('visitor tree limit exceeded')
        if isinstance(node, list):
            node = [visit(child, path + (i,), depth + 1) for i, child in enumerate(node)]
        return transform(node, path)
    return visit(tree, (), 0)


def walk(tree):
    pending = [(tree, (), 0)]
    count = 0
    while pending:
        node, path, depth = pending.pop()
        count += 1
        if count > MAX_NODES or depth >= MAX_DEPTH:
            raise ValueError('visitor tree limit exceeded')
        yield node, path
        if isinstance(node, list):
            pending.extend((child, path + (i,), depth + 1) for i, child in reversed(list(enumerate(node))))
