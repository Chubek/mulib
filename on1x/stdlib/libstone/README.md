# LibStone node interchange

`std.libstone` represents a node as a nonempty S-expression list
`(Tag child ...)`. The tag is an S-expression symbol; each child may be any
S-expression, including an empty list or a nested node. These helpers do not
validate a LibStone schema, serialize the tree, or call a C++ ABI.

- `libstone_node(tag, children)` prepends a symbol to a proper child list.
- `libstone_tag(node)` validates the outer node and returns the tag text.
- `libstone_child(node, index)` validates the outer node and returns the child
  at a zero-based `i64` index. The tag is not a child.

All three enforce the same limit of **262,144 immediate children**, excluding
the tag. This is not a recursive bound. They fail with an On1x tool error
(exit 73 in the direct C11 runtime) for malformed input, invalid symbols or
out-of-range indices. Tag text is subject to the runtime's NUL-free symbol
rules and allocation limits.

From the repository root, compile and run the boundary and failure checks
with an existing On1x compiler and a C11 compiler:

```sh
python3 -B on1x/stdlib/libstone/tests/run.py /path/to/on1xc cc
```
