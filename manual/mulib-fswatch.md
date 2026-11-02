# MuLIB fswatch

**Header:** `include/mulib-fswatch.h` · **Switch:** `MULIB_IMPL_FSWATCH`  
**Dependencies:** daemon, glob, select, vfs and their transitive dependencies.
C11 / C-linkage declarations. See the integration section below.

## Watch programs and filesystem backends

FSWatch also provides an owned DSL program, a snapshot-diff runtime, native POSIX
and MuLIB VFS scanners, and daemon launching. The low-level inotify API below
continues to expose raw Linux events independently of the snapshot model.

```text
# Paths and patterns are double-quoted; handlers are application identifiers.
watch "/srv/project/src" recursive {
  on create|modify "*.c" -> rebuild;
  on delete "*" -> invalidate;
}
watch "object://repository" {
  on any "*" -> notify;
}
```

Grammar: `watch STRING [recursive] { (on EVENTS STRING -> IDENTIFIER ;)* }`.
EVENTS is one or more names separated by `|`: `create`, `modify`, `delete`, or
`any` (all three). Identifiers start with an ASCII letter or underscore and
continue with letters, digits, underscores or hyphens (max 63 bytes). `#` starts
a line comment outside strings. Strings support only escaped quote/backslash;
control bytes and embedded NULs are rejected. Unsupported syntax is diagnosed
with a static message and zero-based source byte offset. Empty programs and empty
rule blocks are valid. Limits: 64 KiB source, 64 roots, 256 rules, 4095-byte strings.

`mulib_fswatch_parse` copies the complete program and performs no I/O. Program
roots are available through `program_count`/`program_root`; keep a program alive
while runtimes borrow it. Root identifiers
are interpreted by the chosen backend. `mulib_glob_match` supplies byte-oriented
whole-string `*`/`?` matching; `*` crosses slash separators and includes dotfiles.
There are no character classes or special `**` rules. Handlers are names dispatched
to C callbacks, not shell commands or arbitrary inline Lua.

`mulib_fswatch_runtime_create` borrows a program, scanner and filesystem context
and accepts a 1..1000000-entry snapshot limit, including root entries. A scanner
enumerates canonical relative paths (empty means the root), a 64-bit change
version and directory flag through the supplied emitter. Duplicate paths, path
traversal components, absolute paths, backslashes and control bytes are rejected.
The emitter copies each path and enforces the entry bound even if a backend ignores
its errors. Scanners must return complete snapshots or report failure, honor
recursion, and propagate emitter failures. Missing roots yield empty snapshots.
Any object store, remote filesystem or custom VFS can implement this interface.

`mulib_fswatch_poll` establishes a silent baseline on its first successful call.
Subsequent polls compare sorted snapshots and dispatch CREATE, MODIFY or DELETE.
A rename is observed as deletion/creation, with no cookie pairing. A failed scan
or invalid/oversized snapshot preserves the previous baseline and emits nothing.
A complete snapshot commits before callbacks; all matching handlers are attempted
even after one fails, and the first callback failure is returned. Repeating a poll
does not redeliver already committed changes. A type change at an existing path
is MODIFY. Dispatch order is watch declaration order, then bytewise path order,
then rule declaration order. Event strings are borrowed only for the callback.

Native POSIX scans recursively enumerate directories, fingerprint regular-file
contents and permission/type bits, and fingerprint symlink targets without following
them. Directory timestamps are not fingerprints, avoiding spurious parent events.
Special-file payloads are not opened. VFS scans use `stat`, `read` and `list`, and
fingerprint file contents. Both emit the watched root itself as the empty path,
limit recursion to 128 levels and joined paths to 4095 bytes. Fingerprints use
64-bit FNV-1a, not a cryptographic identity. Polling can miss transient changes,
hash collisions or concurrent modifications; scans are not atomic filesystem
snapshots. Reading content costs proportional to the total scanned file bytes;
use a custom version-based scanner for large/remote stores.

Objects require external synchronization. Nested polls fail EBUSY. Do not free
the program/runtime from a handler; runtime destruction while polling is ignored.
Parse/create functions return NULL on error. Internal polling errors return
-1/errno; scanner and handler nonzero codes propagate (native/VFS scanners use
-1/errno). No implicit threads or global signal handlers are created.

## Daemonized programs

`mulib_fswatch_launch` combines a daemon specification, parsed watch program,
scanner, named-handler callback and positive polling interval in milliseconds.
It calls `mulib_daemon_launch`; the child creates its runtime and repeatedly polls
and waits. Backend context is fork-copied, so a memory-only VFS does not observe
later parent writes; native/shared/remote backends are suitable for live daemons.
Relative native roots resolve against the daemon specification's working directory.
Scan/handler failure exits the daemon with status 1. Successful launch reports
process setup, not completion of the first scan. Stop through `mulib_daemon_stop`
and reap through `mulib_process_wait`. The application owns callback dispatch and
can connect events to `mulib_buildgraph_execute` or another subsystem.

For systemd, configure a **foreground** application runner with
`library/ldaemon/fswatch.lua`, then compile its spec using
`library/ldaemon/systemd.lua`. Systemd owns supervision; the foreground runner
uses `mulib_fswatch_poll` in its event loop. See [daemon](mulib-daemon.md) for the
Mune configuration API and child-process contract.

## Integration

Define these switches before including FSWatch in the implementation TU:

```
MULIB_IMPL_FSWATCH MULIB_IMPL_SELECT MULIB_IMPL_VFS MULIB_IMPL_DAEMON
MULIB_IMPL_PROCESS MULIB_IMPL_MUNE MULIB_IMPL_DYNLOAD MULIB_IMPL_MIR
MULIB_IMPL_VMWEAVE MULIB_IMPL_EGRAPH MULIB_IMPL_REWRITE MULIB_IMPL_DYNCALL
MULIB_IMPL_JIT MULIB_IMPL_PARSEC MULIB_IMPL_PARSER MULIB_IMPL_PEG
MULIB_IMPL_AST MULIB_IMPL_STRBUF MULIB_IMPL_GLOB
```

These are explicit link dependencies even when an application uses only native
inotify functions from the combined header. No switches are enabled implicitly.
The standalone example contains the complete list. Link threads, math and the
platform dynamic loader as for Mune.

## Supported native subset

```c
typedef struct mulib_fswatch_event {
    int32_t watch;
    uint32_t mask, cookie;
    const char *name;
    size_t name_length;
} mulib_fswatch_event;
int mulib_fswatch_create(void);
int mulib_fswatch_add(int fd, const char *path, unsigned mask);
int mulib_fswatch_remove(int fd, int watch);
int mulib_fswatch_read(int fd, void *buffer, size_t capacity, size_t *count);
int mulib_fswatch_next(const void *buffer, size_t length, size_t *offset,
                       mulib_fswatch_event *event);
int mulib_fswatch_close(int fd);
```

`create()` calls `inotify_init1(IN_NONBLOCK | IN_CLOEXEC)` and returns a newly
owned fd. `add()` returns a native watch id, replacing interest if the same inode
is already watched on that fd. Watches are **not recursive**. Symlinks follow
native default behavior. No fanotify, polling emulation, Windows/macOS backend,
recursive directory management, only-directory/no-follow/one-shot options, or
automatic rescan is provided. Native operations on non-Linux hosts return
`ENOSYS`; the bounded decoder itself is platform-independent. No late feature
macros, allocations, or worker threads.

## Watch masks and events

The accepted nonempty interest subset uses these `MULIB_FSWATCH_` flags:
`MODIFY`, `ATTRIB`, `CLOSE_WRITE`, `MOVED_FROM`, `MOVED_TO`, `CREATE`, `DELETE`,
`DELETE_SELF`, `MOVE_SELF`. `WATCH_MASK` is their union. No unknown input bits
are silently ignored. Decoder masks retain raw Linux inotify bits, including
`ISDIR`, `UNMOUNT`, `OVERFLOW`, `IGNORED`, and future bits.

Names are directory-relative bytes, not validated Unicode or safe filesystem
paths. No name is supplied for some self/overflow events (`name == NULL`,
`name_length == 0`). Rename matching uses native nonzero cookies, but pairs can
be separated, lost, or refer to locations outside watched directories.
Events coalesce: this is not an audit log. `OVERFLOW` (watch -1) means the
application must rescan/rebuild its state. `IGNORED` means the watch was removed,
explicitly or automatically. Removal leaves its notification queued. Watch ids
are kernel-managed and can eventually be reused; track lifetimes externally.

## Batch reads and alignment-safe decoding

`read()` performs one native read into caller-owned storage. Capacity must be
`16..PTRDIFF_MAX`; use 4096 bytes or more for normal filename-bearing events.
If the next event cannot fit, the OS reports `EINVAL` rather than splitting it.
An empty nonblocking queue reports `EAGAIN`. Output count is unchanged on error.

Initialize `offset = 0` and call `next()` until it returns zero. It returns 1
for one decoded event, 0 only at exact end, or -1/`EINVAL` on malformed input.
It reads the native-endian Linux wire layout (16-byte header followed by native
name length) with memcpy; unaligned input is supported. Length cannot exceed
`PTRDIFF_MAX`. It rejects truncated headers/names, out-of-range offsets, watch
numbers below -1, and nonempty name spans without a NUL. It does not require
padding zeroes or known output mask bits. This is a native event decoder, not a
cross-endian serialized format validator.

On decoding failure, both offset and output remain unchanged; exact end also
does not modify output. The name points into the borrowed input batch and is
valid only until that storage is overwritten/freed. `name_length` excludes the
terminator and native padding. Do not cast arbitrary input to `struct
inotify_event` or retain the returned name after the next batch read.

## Ownership and errors

The fd owns all native watches; closing it releases them without deleting
filesystem objects. `remove()`, `read()`, and `close()` return 0/-1. Invalid
arguments set `EINVAL`; native limits/permissions produce native errno
(`ENOSPC`, `EACCES`, etc.). EINTR propagates without retry. Errno is unspecified
on success. Close once and relinquish ownership even on interruption. The
wrapper cannot check fd provenance. Unwatch a borrowed select/epoll/reactor fd
before closing it. No mandatory third-party library or elevated privilege.

## Example and verification

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/fswatch.c -pthread -lm -ldl -o /tmp/opencode/fswatch-example
/tmp/opencode/fswatch-example
```

The example first runs a DSL program against a MuLIB in-memory VFS, then creates its own exclusive
local directory/file, waits at most 1000 ms, decodes the create event, and removes
only its own objects. Platform, permission, and watch-limit restrictions print
skips. `tests/test_os_events.c` adds unaligned/malformed batches, transactional
outputs, overflow records, real create/modify/rename/delete/cookie/ignored
events, fd flags, and reactor integration.
`tests/test_daemon_fswatch.c` covers DSL errors, native recursive changes, VFS
same-size content changes, snapshot limits, failed scans, duplicate entries,
handler failures, daemon execution and Mune/systemd profiles.
