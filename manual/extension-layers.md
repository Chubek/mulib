# Storage, binary, document and container layers

These are focused C11 APIs, not implementations of every technology implied by
their filenames. Each header's public Doxygen contracts describe its entire API.
Every corresponding `examples/modules/*.c` lists the complete implementation
switch closure. Define those switches in one C translation unit, not globally.
Initialize owning objects as documented; do not copy them by assignment. Borrowed
results expire on owner mutation/free unless an individual API says otherwise.
There are no implicit dependency switches, external libraries or background jobs.

## Database storage

`memtable` owns sorted binary keys/values, sequences and tombstones. Lookup is
logarithmic; insertion moves a vector. `wal` encodes CRC-framed operations;
recovery distinguishes a torn final frame from corruption. `sstfile` snapshots
sorted records with per-frame and whole-file checksums. Formats are MuLIB-specific,
little-endian, versioned byte formats, not RocksDB/LevelDB interchange formats.
`lsmtree` composes these as a memory-backed mutable table and immutable runs.
Flush/compaction are explicit. WAL publication precedes logical mutation, but
there is **no disk I/O, fsync, locking or crash-durability guarantee**. For durable
atomic whole-file snapshots use the existing `kvdb` API instead.

`dbtransaction` clones a table and checks its revision at commit (single-threaded
optimistic conflicts). Target must outlive transaction; changes made directly to
public structure fields bypass conflict detection and are unsupported. `cowb+tree`
shares B+ snapshots, rebuilding the entire tree on first shared mutation rather
than doing page-level copy-on-write. `heaptable` allocates stable positive row IDs.
`index` provides unique integer keys; `dbindex` binary key-to-row mappings;
`invindex` unique sorted posting lists; `blockidx` greatest-minimum fence lookup;
`blockcache` copies blocks and evicts by LRU under byte/entry limits. None pin or
lock borrowed values. `spatialtree` is an immutable median-balanced 2D k-d tree.

`dbsummary` checks aggregate byte counts; `dbquery` scans half-open key ranges.
`sql` accepts only the documented single-table `kv` SELECT/INSERT/DELETE grammar.
It rejects joins, numeric expressions, arbitrary columns, multiple statements,
comments and unsupported clauses; INSERT upserts. String literals use SQL doubled
quotes. Keywords are ASCII case-insensitive; whitespace is space/tab/CR/LF.

## Binary files and ELF

`bycutils`, `binfile`, reader/writer/schema/builder modules use subtraction-based
bounds checks, explicit little-endian integers and failure-atomic appends.
Schemas cover fixed-size nonoverlapping unsigned fields only. They are not
native structure serializers. `elfutils` composes the existing ELF reader;
`objfile` requires ELF ET_REL. Object writer/builder emit ELF64 little-endian
PROGBITS sections and `.shstrtab`, without symbols/relocations/program headers.
`objfileinjector` replaces same-sized PROGBITS content, not running processes.
`objfilepacker`/`objfilelibsearch` handle short-name Unix ar archives without a
symbol index, GNU/BSD long names, thin members or external file lookup.

Relocation helpers apply numeric ABS32/ABS64/PC32 values with overflow checking;
callers supply resolved symbols. PC32 uses S+A-P (P is field address).
`objfilestub` emits a 14-byte x86-64 indirect jump as data. `objfilesymloc` locates
file-backed defined symbol bytes. The historical `objfileleader` name denotes a
PT_LOAD copy/zero-fill helper; it does not map executable memory, set permissions,
link dynamic libraries or execute an entry point. Its source/destination must
not overlap. `flatbuffers` reads tables, unsigned scalars and strings with local
bounds checks; it is not whole-schema verification, a serializer, UTF-8 validator
or a FlatBuffers schema compiler. Do not modify validated borrowed views.

## Documents and layout

`tex` scans control words, braces, text, whitespace and percent comments; it has
fixed lexical rules, no TeX execution/catcodes/macros/font shaping/math layout.
Its text-mode escape helper supports UTF-8 but does not make arbitrary input
safe as raw TeX commands. `latex` builds escaped article titles/sections/paragraphs
and explicitly closes documents. It never launches a TeX toolchain.
`layout` divides one row/column by finite nonnegative weights, with gaps. It does
not render or implement CSS. `layoutml` accepts only a flat layout element with
direction/gap attributes and empty item elements with weight attributes; numeric
attributes are integers 0..1000000. Unknown constructs fail, not disappear.

## Containers and security-sensitive adapters

`rootfs` is bounded **in-memory** storage; USTAR import creates parents, rejects
duplicate explicit entries and `.wh.*` components, and never extracts on the host.
Only regular files/directories are supported. Rootfs export normalizes metadata;
no permissions, symlinks, special files, OCI whiteout layering or compressed layers.
OCI image JSON helpers validate/build version-2 manifests and sha256 descriptor
syntax; hashes/signatures/blob sizes are **not verified against content**. JSON
descriptor sizes are limited to exactly representable integers through 2^53-1.
`imgtrackerd` keeps in-memory reference counts, not a running daemon or blob GC.

Container identity/state is shared across metadata, runtime, stub and server
dispatcher. Backend callbacks perform actual lifecycle operations and must enforce
isolation, privileges, namespaces, cgroups, mounts, signal handling and policy.
The built-in modules do none of these implicitly. VFS mounts and injection only
affect memory storage; volume/network objects are validated plans, not host changes.
`ocicontainernetwrork` forwards to the canonical `ocicontainernetwork` spelling.
Protocol frames have a 1 MiB payload limit and CRC32, **not authentication**.
Caller must authenticate/authorize before server dispatch and supply transport.
Replies use request opcode|128 and JSON state, request IDs are correlation tokens.
No implicit listening sockets, threads or native daemon processes are created.

`initproc` reaps children on POSIX, but does not become PID 1 or forward signals.
`kvmcontainer` owns a Linux KVM device/VM/vCPU bundle; guest setup is caller-owned.
Tests never require /dev/kvm or privileges. `ebpf` executes a forward-jump-only,
register-only MOV/ADD/JEQ/JA/EXIT subset; no helpers, memory, packet filtering or
kernel loading. It is not a Linux verifier or a security boundary. `socket`
offers connected POSIX stream-pair ownership, nonblocking flags and short I/O.
`tsl` (historical spelling) is an authenticated TLS **backend adapter**, not TLS
cryptography. No plaintext fallback exists. Backend must enforce real peer
identity verification and report it; test doubles demonstrate adapter state only.

## Validation

`test_database_layers.c`, `test_database_failures.c`, `test_binary_layers.c`,
`test_document_layout.c`, `test_container_layers.c`, `test_security_adapters.c`
and `test_layer_hostile_inputs.c`
cover composition, malformed/truncated bytes, bounds, ownership, callback failures,
optimistic conflicts, COW isolation, allocation failure, and native failure paths.
The normal inventory checker also tests all headers individually and combined,
declaration-only C/C++, reverse inclusion order and implementations after declarations.
These tests are not a standards-conformance or security certification.
