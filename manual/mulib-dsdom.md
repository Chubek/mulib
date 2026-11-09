# mulib-dsdom

[Manuals](README.md) · [Library index](index.md)

Documentation DOM utilities over the existing [XML tree](mulib-xml.md).
`mulib_dsdom_t` is the XML node type, not a second incompatible DOM. This lets
CSS selectors, XML serialization and documentation adapters share node identity.
The semantic [DocstrIL schema](mulib-docstring.md#docstril) is defined separately.

## Integration and ownership

Define `MULIB_IMPL_DSDOM`, `MULIB_IMPL_XML`, `MULIB_IMPL_STRBUF` and
`MULIB_IMPL_UTF8` in one translation unit. No external libraries are required.
See the complete [example](../examples/modules/dsdom.c).

`mulib_dsdom_element(name, text)` copies the name and optional text. NULL text
means no child; empty text means an empty text child. Creation and cloning
return owned nodes, or NULL for invalid input, depth overflow or allocation
failure. Destroy with `mulib_xml_destroy`. XML append transfers ownership only
on success; detach transfers ownership back. Clone copies attributes and children
but not the original parent or siblings. Document wrappers are supported.

`mulib_dsdom_text` returns a malloc-owned, NUL-terminated concatenation of all
descendant text/CDATA, excluding comments. The optional length excludes NUL and
is zero on failure. Empty content returns an allocated empty string; NULL input
fails. Clone/text allow at most `MULIB_XML_DEPTH_LIMIT` nested elements, ignoring
document wrappers and the leaf text below the deepest element. Node
names, attribute values and text obey XML's ASCII-name/UTF-8 character profile.

`mulib_dsdom_find_id` performs a preorder search including the starting node.
Its result is borrowed; NULL means absent or invalid arguments. Mutations and
destruction can invalidate borrowed nodes/strings. Operations require external
synchronization. Utilities are linear in visited tree bytes/nodes, with clone
also copying attribute storage. Tests: [documentation suite](../tests/test_docstring.c)
and [allocation-failure suite](../tests/test_hardening.c).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-dsdom.h`](../include/mulib-dsdom.h) |
| Implementation switch | `MULIB_IMPL_DSDOM` |
| Repository-local includes | [mulib-xml](mulib-xml.md) |
| Complete example | [`examples/modules/dsdom.c`](../examples/modules/dsdom.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dsdom.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
