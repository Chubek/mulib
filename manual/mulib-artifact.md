# MuLIB artifact

**Header:** `include/mulib-artifact.h`  
**Switch:** `MULIB_IMPL_ARTIFACT` · **Dependencies:** none

Artifacts are immutable envelopes for application values. They carry a required
application identity and type, an optional media type and locator, and one of:

* `MULIB_ARTIFACT_BYTES`: an owned copy of arbitrary bytes, including embedded NULs;
* `MULIB_ARTIFACT_REFERENCE`: an external reference with a required nonempty locator;
* `MULIB_ARTIFACT_OBJECT`: an opaque application object with an optional destructor.

Identities and types are uninterpreted strings. They need not be filenames,
content hashes, build products, or globally unique. Messages, documents, tool
results and agent-session objects can use the same API as a build graph.
References are descriptive: creation does not resolve a URI or open a file.

## Ownership

Zero-initialize `mulib_artifact_desc_t`, fill its fields, and call
`mulib_artifact_create`. All strings and byte payloads are copied. An object
destructor transfers ownership **only when creation succeeds**; without a
destructor, the application keeps ownership and must keep the object alive.
The descriptor's object fields must be empty for other kinds, and its byte fields
must be empty except for BYTES. An OBJECT requires a non-NULL object.

`mulib_artifact_view` borrows a read-only descriptor until the last release.
The descriptor cannot be mutated. An opaque object's internal mutability remains
the application's responsibility; build inputs should be immutable snapshots.
`retain` adds a reference, `release` removes one, and the final release frees
the copied storage and invokes the object destructor exactly once. Reference
counts are not atomic; synchronize shared access externally. `retain` returns
NULL on NULL or count overflow; `release(NULL)` is harmless.

`create` returns NULL for invalid descriptors, byte lengths greater than
`PTRDIFF_MAX`, or allocation failure. It has no partial ownership side effects.
Zero-length bytes are valid, including a NULL data pointer. There is no automatic
serialization of opaque objects, content equality, or URI canonicalization.

## Example and verification

```sh
cc -std=c11 -Iinclude examples/modules/artifact.c -o /tmp/opencode/artifact
/tmp/opencode/artifact
```

`tests/test_buildgraph.c` checks binary payload copying, empty values, references,
destructor ownership, invalid descriptors and allocation failures, together with
retained graph inputs and outputs.
