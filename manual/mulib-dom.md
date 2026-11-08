# Native DOM core

[Manuals](README.md) · [Library index](index.md)

`include/mulib-dom.h` implements a native, XML-style DOM tree and synchronous
event propagation. Define `MULIB_IMPL_DOM`, `MULIB_IMPL_BUFFER`, and
`MULIB_IMPL_UTF8` in one C99/C11 translation unit; include the header normally
elsewhere. Declarations have C linkage for C++ clients. No browser, scripting
engine, generated bindings, global registry, or external library is required.

## Reference profile

The API is an original C implementation informed by the declarations in
[`library/webidl/gecko`](../library/webidl/gecko/) and
[`library/webidl/webcore`](../library/webidl/webcore/). Those source files retain
their own license notices; their implementation code is not incorporated here.
In particular:

| Reference IDLs (both directories) | Native functionality |
| --- | --- |
| `Node`, `Document`, `DocumentFragment`, `DocumentType` | Node types and names, ownerDocument, root/parent/sibling access, document element, contains, document position, insertion/removal/replacement, fragment splicing, adoption, import, cloning, equality, textContent, normalize |
| `Element`, `Attr`, `NamedNodeMap` | Namespace/local/prefix names, attribute ownership, namespace-aware get/set/remove, insertion-order indexed attributes |
| `CharacterData`, `Text`, `Comment`, `CDATASection`, `ProcessingInstruction` | UTF-16 data, substring/replace, Text splitting, creation-time delimiter checks |
| `NodeList`, `ParentNode`, `NonElementParentNode` | Live indexed childNodes, descendant element snapshots, getElementById |
| `Event`, `EventTarget`, `CustomEvent` | Capture/target/bubble phases, cancellation and propagation control, listener identity/options, stable event paths, borrowed detail payload |

This is a defined native core, not the full browser API surface represented by
the corpus. There are no HTML document rules/case folding, parsers/serializers,
CSS selectors, DOMTokenList, Range, mutation observers, shadow trees, custom
element reactions, abort signals, layout, navigation, or ECMAScript bindings.
The separate [XML](mulib-xml.md) and [HTML](mulib-html.md) modules provide their
own parser/serializer document models. They cannot be cast to DOM nodes: this
model requires document ownership, detached-node identity, namespaces, and
arbitrary UTF-16 strings that the XML model does not provide.

## Ownership and results

`mulib_dom_document_new()` owns every node it creates, even after removal from
the tree, removal from an attribute map, normalization, or textContent replacement.
There is no individual node destructor. `mulib_dom_document_destroy()` releases
all those nodes and their listeners. It returns 0 if that document is participating
in a dispatch; otherwise 1, including for NULL. Detached nodes remain usable
until the owning document is destroyed or they are adopted into another document.

Inputs are copied unless explicitly described as borrowed. Accessors borrow
nodes, names, and strings. Node identity is stable until document destruction;
string/name views must be reacquired after editing the corresponding data or
attribute. A document node has no ownerDocument. An Attr has an ownerElement but
no parentNode or siblings in the tree. Snapshot buffers own only arrays of
borrowed node pointers. Listener userdata and CustomEvent detail remain
caller-owned.

Pointer-returning creation/edit functions return NULL on failure. Most integer
operations return 1 for success and 0 for failure. Their optional
`mulib_dom_error_t *` is cleared on success. Exceptions are:

* `dispatch_event`: 1 if not canceled, 0 if canceled, -1 on error.
* `lookup_namespace_uri`: 1 if found, 0 if absent/undeclared, -1 on error.
* Predicates/accessors return their value; absent borrowed objects are NULL.

Errors include DOMException-style hierarchy, index, namespace, not-found,
wrong-document and in-use-attribute codes, plus allocation and resource limits.
All mutating operations and buffer appends are atomic on failure, including
clone/import rollback and normalization. Aggregate output staging exhaustion is
reported as `OUT_OF_MEMORY`; explicit node/depth/string checks use `LIMIT_ERR`.
Memory allocation uses standard malloc/calloc/realloc/free. There is no mutable
global state. A document and any events operating on it require external
synchronization between threads.

## Strings and names

`mulib_dom_string_t {const uint16_t *data; size_t length;}` is a borrowed DOMString.
Lengths, CharacterData offsets, and counts are **UTF-16 code units**. Embedded
NUL and unpaired surrogates are preserved, including when splitting a surrogate
pair. NULL data is valid for zero length. `node_value` returns an empty span for
containers (the native representation of their null nodeValue).

`utf8_to_string` and `string_to_utf8` append without a terminator. The former
rejects invalid UTF-8; the latter substitutes U+FFFD for each unpaired surrogate,
as a USVString conversion does. `create_text_utf8` is a convenience constructor.
UTF-16 output buffers contain native-endian `uint16_t` units and count **bytes**
in their `length` field. Divide that length by `sizeof(uint16_t)` to obtain a
DOMString length. Conversion input may alias the output buffer; mutation input
may borrow the node's existing data.

Names and namespace identifiers use NUL-terminated C strings. Names support an
ASCII XML qualified-name subset: letter/underscore at the start of each part,
then letters/digits/underscore/dot/hyphen, with at most one colon separating
nonempty prefix and local name. Namespace NULL and empty string both mean no
namespace. Prefixed names require a namespace. The `xml` and `xmlns` bindings
are checked against their reserved URIs. These are namespace-aware creation and
attribute methods, including when their namespace argument is NULL.

CharacterData creation accepts Text, CDATA, Comment, and ProcessingInstruction.
PI requires a valid target and rejects `?>` in its initial data; CDATA rejects
`]]>` in initial data. Subsequent CharacterData edits preserve arbitrary units.
Cloning preserves edited data. `split_text` accepts Text/CDATA and creates a
**Text** sibling; splitting at length produces an empty node.

## Trees and collections

An empty document is valid. It can contain comments and processing instructions,
at most one element, and at most one doctype preceding that element. Text and
CDATA cannot be direct document children. Elements and fragments can contain
elements, Text/CDATA, comments, and processing instructions. Attributes and
documents cannot be inserted as children. Insertion rejects cycles and invalid
reference children before changing anything.

`insert_before(parent, node, NULL, error)` appends. Inserting a fragment moves its
children in order and leaves it empty. Inserting an existing node moves it;
cross-document insertion adopts its subtree and attributes. `replace_child`
returns the detached old child. Invalid fragment insertion leaves both trees
intact. Adoption detaches a node (or an attribute from its owner) and transfers
the whole subtree's storage to the destination document. Cross-document adoption
is rejected while either document is dispatching, ensuring event-path lifetimes.

Import/clone always copy attributes and optionally copy descendants. They omit
listeners. Import selects the destination document; clone uses the source's
document. Document nodes cannot be imported, adopted, or cloned by this API.
`is_equal_node` compares data and tree shape with attribute order ignored.
`compare_document_position` uses DOM bit values; disconnected ordering is stable
for live node identities within a process, not a portable serialized ordering.

`child_count`/`child_at` are live views, recomputed on each call.
`get_elements_by_tag_name` appends a tree-order array of `mulib_dom_node_t *` to
the output buffer. It excludes the receiver, and supports `"*"` independently
for namespace and local name. A snapshot remains unchanged after later edits.
`get_element_by_id` likewise searches descendants only, matching the nonempty,
no-namespace `id` attribute exactly as UTF-16; it returns the first match.

`set_attribute` preserves an existing Attr's identity and position, even when
the prefix changes. `set_attribute_node` replaces by namespace/local name and
detaches the previous Attr; it rejects an Attr owned by another element or
document. Removing a missing attribute succeeds. Namespace lookup considers
the element's own binding, XMLNS attributes, ancestors, and reserved bindings.
An empty XMLNS value undeclares a binding. Its UTF-8 result has no terminator.

`text_content` concatenates Text/CDATA descendants of elements/fragments,
excluding comments and PI. CharacterData/Attr return their own value;
documents/doctypes produce no output. The setter replaces element/fragment
children with one Text node (none for empty input), changes CharacterData/Attr
values, and does nothing for documents/doctypes. `normalize` recursively removes
empty Text nodes and merges adjacent Text nodes into the first nonempty Text;
CDATA forms a boundary. Detached nodes retain their identities and ownership.

## Events

All nodes can be event targets. `event_new(type, bubbles, cancelable, detail)`
copies the type (an empty string is allowed) and borrows detail. Events are
synthetic and untrusted. There is no shadow-tree retargeting or default action.
`event_destroy` returns 0 during dispatch, otherwise frees the event.

Listener identity is `(type, callback, userdata, capture)`. Adding the same
listener twice does nothing, including preserving its original once/passive
options. NULL callback is a no-op. Removal matches capture, not once/passive.
Listeners execute in registration order. A once listener is removed **before**
invocation, including nested dispatch. Passive listeners cannot cancel events.

Dispatch snapshots the ancestor path, so moving/removing nodes during a listener
does not change that event's route. Capture runs root-to-parent; target capture
precedes target noncapture; bubbling runs parent-to-root when enabled.
`stopPropagation` prevents reaching another node but allows remaining listeners
on the current node, including target noncapture after target capture.
`stopImmediatePropagation` also suppresses remaining current-node listeners.
Listener removal takes effect immediately; additions do not run in the current
invocation's listener snapshot, but can run in a subsequent phase/invocation.

Nested dispatch of a different event is supported. Redispatching the same active
event fails. After dispatch, currentTarget is NULL and phase is NONE; target and
defaultPrevented remain observable. `event_composed_path` appends target-to-root
node pointers during dispatch, and an empty span afterwards. Retained event
target pointers require the document to remain alive.

## Bounds and verification

Each document supports 65,536 nodes (including its document node and detached
nodes), at most 256 tree levels, and each DOMString supports 16,777,216 units.
Each target supports 65,536 listener records, including pending removals during
dispatch. Nested event dispatch is limited to 256 levels per document. The
constants in the header describe fixed implementation limits. Tree queries use
linear traversals; child/attribute lookup is linear. Cloning and equality can
take quadratic time in the number of attributes on a single element. Insertion checks
subtree depth before mutation. No filesystem or network access occurs.

[`examples/modules/dom.c`](../examples/modules/dom.c) builds a document, dispatches
an event, and prints UTF-8 text. [`tests/test_dom.c`](../tests/test_dom.c) exercises
hierarchy failures, fragment rollback, moving/adopting/cloning nodes, attribute
identity, UTF-16 offsets and surrogate splitting, namespace lookup, normalization,
listener mutation/reentrancy, depth limits, and allocation-failure rollback.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-dom.h`](../include/mulib-dom.h) |
| Implementation switch | `MULIB_IMPL_DOM` |
| Repository-local includes | [mulib-buffer](mulib-buffer.md), [mulib-utf8](mulib-utf8.md) |
| Complete example | [`examples/modules/dom.c`](../examples/modules/dom.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dom.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
