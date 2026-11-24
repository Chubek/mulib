# ArchVirt guest artifact cache

The private `on1xc` cache stores the **final guest ELF**, not source,
intermediate objects, host executables, or a source-to-artifact index.
The exact content key is also the VM ID. `--vm-run` builds from the current
inputs every time; emission modes can register the finalized image without
running. `--vm-uncached` reports its ID without writing the cache.
`--vm-rerun KEY` uses only that explicit key, without loading source or
rebuilding. It still performs the runner's normal guest ELF and runtime
validation.

## Identity and integration

`on1x/archvirt_cache.hpp` exposes `identify(identity, elf)`, `publish`, `load`,
optional `find`, and `clear()` to the CLI. The guest
identity consists of a versioned target/boot profile (a nonempty string of
at most 256 bytes without NUL) and the lowercase SHA-256 digest of the
**exact archive contents used to link** the guest. For an archive given
by a mutable path, the caller should hash and link the same immutable
snapshot. A path, modification time, archive name, or version string is
not an archive fingerprint. Profile names must change when the guest
target or replay ABI becomes incompatible.

The key is lowercase hexadecimal SHA-256 of these bytes in order:

```
"on1x.archvirt.guest/v1" NUL profile NUL archive-digest-hex ELF-bytes
```

The profile has a NUL delimiter, and the archive digest occupies exactly
64 ASCII bytes. Each entry is one raw artifact file with a maximum size of
64 MiB. The cache checks the file's full bytes against its name and the
current identity before returning it. Changing either the runtime archive
or the guest profile makes an old key incompatible even when the ELF bytes
happen to be identical. Nothing is recompiled or substituted on a miss or
identity mismatch.

`load()` returns a binary string snapshot, **not** a cache pathname. The
runner can provide those bytes through a sealed file descriptor if it needs
a path-shaped input. A concurrent clear cannot invalidate a returned
snapshot. Digest validation is not ELF/ABI verification, signing, or
sandboxing.

## Filesystem and concurrency

The base is an absolute `$XDG_CACHE_HOME` when set; an empty or relative
value falls back to `$HOME/.cache`. The isolated namespace is
`BASE/on1x/archvirt`. Cache access requires POSIX filesystem operations with
directory `flock` and atomic same-directory rename. No cache component may
be a symlink; cache paths with `.` or `..` components are rejected.
Ancestors must be owned by the current user or root and cannot be writable
by other users unless sticky (for example, `/tmp`). The base and `on1x`
parent must be owned by the current user and not writable by others. The
ArchVirt namespace has mode `0700`; artifact files must be user-owned,
single-link regular files with mode `0600`. An unsafe cache fails closed;
the implementation does not fix permissions on existing paths. The cache
does not isolate processes that already have the same OS user authority.

Reads take a shared namespace lock. Publication and clearing take an
exclusive lock. Publication syncs a staging file, atomically renames it,
and syncs the namespace; a failed or interrupted process cannot expose a
partial artifact as a completed entry. The next publication can replace an
abandoned staging name. A published key is immutable to cooperating cache
users: an existing byte-for-byte match is reused, and a mismatch fails.

`--vm-cache-clear` removes only cache entry names (64 lowercase hex
characters) and the publication stage inside the ArchVirt namespace.
It does not remove other `on1x` caches, explicit compiler outputs, or the
namespace itself (its inode is the lock). It neither follows links nor
recurses into directories, and leaves unrelated filenames alone. A missing
namespace is already clear. Clearing can remove a key published before the
clear; a concurrent job that publishes afterward can repopulate it.

## Standalone verification

From the repository root, choose an absolute, privately owned scratch path
below trusted ancestors and run:

```sh
scratch=$(mktemp -d "$HOME/on1x-archvirt-cache-test-XXXXXX")
export TMPDIR="$scratch"
cc -std=c11 -Wall -Wextra -Werror -Iinclude \
  -c on1x/src/extensions_hash.c -o "$scratch/sha.o"
c++ -std=c++20 -Wall -Wextra -Werror -Wpedantic -Ion1x/include -Iinclude \
  on1x/src/archvirt_cache.cpp on1x/tests/test_archvirt_cache.cpp \
  "$scratch/sha.o" -o "$scratch/test_archvirt_cache"
"$scratch/test_archvirt_cache" "$scratch"
```

The standalone suite exercises exact byte replay, identity changes,
corruption, malformed keys, missing entries, permissive paths, symlinks,
hard links, nonregular and oversized entries, interrupted staging,
namespace-only clear, and concurrent process publication/replay/clear.
