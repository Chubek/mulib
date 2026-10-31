# mulib-container

## NAME

Owned process specifications and generation-tracked lifecycle metadata.

## SYNOPSIS

```c
#define MULIB_IMPL_CONTAINER
#include "mulib-container.h"
```

The only dependency is inline `include-mulib-container-core.h`.
Example: `examples/modules/container.c`.

## SPECIFICATIONS

`spec_init` initializes an uninitialized destination with a validated 1–63-byte
ID, working directory `/`, 256 total argv/environment strings and a 65536-byte
string budget. `spec_arg` copies arguments, including empty ones; tables are
NUL-pointer terminated. `spec_env` copies or replaces `NAME=value`, preserving
unique names. Names follow ASCII shell identifier syntax and are at most 255
bytes. Values may contain `=`. String quotas count terminators and full env pairs.

`spec_getenv` returns a borrowed value until replacement/free. `spec_cwd` accepts
canonical absolute paths up to 4095 bytes, rejecting empty/dot/dot-dot components,
controls, backslashes and trailing slash except `/`. `spec_validate` checks
names, counts, byte accounting, duplicate env keys and read-only flag. Set quotas
before adding data; lowering them below current use makes validation fail.

`spec_clone` deep-copies into an uninitialized output. `spec_free` releases all
strings. Resource fields describe requested memory, CPU, deadline, processes and
open files; zero means unspecified. UID/GID, argv and environment do not execute
or enforce anything by themselves.

## LIFECYCLE

`init` owns a cloned spec and starts CREATED at generation 1. `transition`
permits CREATED→RUNNING/DELETED, RUNNING→PAUSED/EXITED,
PAUSED→RUNNING/EXITED, EXITED→RUNNING/DELETED. Invalid edges do not mutate state.
Each committed edge increments generation. `can_transition(container,state)`
checks the same edge and counter limits without mutation. Entering RUNNING or
PAUSED reserves two remaining generations for EXITED and DELETED; entering EXITED
reserves one for DELETED. Exhaustion returns ELIMIT before changing anything,
so repeated restarts or pauses cannot consume the generations needed for cleanup.

Starting/restarting increments the saturating starts counter and records time.
Resume leaves the original start time/count intact. EXITED records caller time
and exit code. `state_name` returns a static name. `free` releases metadata only:
close a runtime borrowing the record first.

## ERRORS AND OWNERSHIP

All results use `MULIB_CT_*`. String mutations preserve owned data on allocation
failure. Clone/init outputs are unchanged on failure. Public owners must not be
shallow-copied or edited while borrowed. See [runtime](mulib-container-runtime.md)
for backend-acknowledged transitions and [stack](container-stack.md) for tests.
