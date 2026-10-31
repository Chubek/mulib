# mulib-containerd-mount

## NAME AND INTEGRATION

Stable daemon filesystem exports with generation-tagged leases and bounded FUSE
sessions. Define `MULIB_IMPL_CONTAINERD_MOUNT` and transitive `container-vfs`/
`fuse` switches. Example: `examples/modules/containerd-mount.c`.

## EXPORT TABLE

`init(mounts,capacity)` allocates stable slots. `add(id,root,read_only,max_bindings,
max_transfer)` builds an owning container-VFS wrapper and FUSE session around a
borrowed root. IDs are unique. max_bindings includes `/`; max_transfer limits
READ/WRITE request bytes. No host mount or kernel FUSE transport is created.

The root must stay alive and at a stable address until removal. Slot addresses
remain stable while the table is live, so the session safely refers to its own
VFS router. Each new export receives a never-reused generation; generation
exhaustion rejects new exports rather than wrapping.

## LEASES AND DISPATCH

`acquire(id,&lease,&filesystem)` pins the export and returns its borrowed container
VFS. Add optional backend bindings through that filesystem while no dispatch is
active. Each acquisition requires one release. `release` rejects stale handles
or active session callbacks. Do not duplicate/reference-count leases manually.

`dispatch(lease,request,output,capacity,visit,user,response)` requires a valid
pinned slot/generation and delegates to the bounded FUSE session. Invalid leases
produce VFS ENOENT and initialize an available response including request ID.
This method uses VFS error codes, while table/lease APIs use CT errors.

`remove(id)` rejects borrowed or busy exports, then releases session/router
wrappers without freeing root. `free` first rejects any outstanding references or
active sessions; successful free releases every export/table. Pointers and leases
from removed exports are invalid and must not be reused.

## LIMITS

Session READDIR uses the underlying synchronous callback model, not kernel FUSE
cookies/open handles. File bytes live in borrowed storage; the table quota bounds
exports and each VFS quota bounds bindings. Externally serialize operations and
do not free a root while exported. Daemon deletion preflights leases before
destroying the corresponding container/rootfs.
