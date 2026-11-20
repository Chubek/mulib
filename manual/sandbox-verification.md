# Sandbox kernel verification

## Host and commands

Native little-endian x86-64, Linux `6.18.52-1-lts`, GCC `16.2.1`, Clang
`22.1.8`, non-root supervisor. With `MULIB_TEST_CGROUP_PARENT` explicitly set to
the user's writable cgroup v2 delegation
(`/sys/fs/cgroup/user.slice/user-1000.slice/user@1000.service`), these strict
C11 runs passed:

```sh
CC=gcc SANITIZE=1 ./tests/run_sandbox.sh
CC=clang SANITIZE=1 ./tests/run_sandbox.sh
```

Each command runs the regular fixture and then the ASan/UBSan supervisor
fixture against the same unsanitized trusted worker. Both standalone examples,
`examples/modules/sandbox-process.c` and `examples/modules/capbroker.c`, also
compiled and ran with each compiler under strict warnings.

## Repository verification

With the same delegated parent:

```sh
CC=gcc BUILD_DIR=build/sandbox-verify-gcc JOBS=4 ./tests/run.sh
CC=clang BUILD_DIR=build/sandbox-verify-clang JOBS=4 ./tests/run.sh
```

Both full builds succeeded. Each CTest run passed **680 of 681** tests,
including the sandbox fixtures and the dependency and MuLL inventory checks.
Both exited 8 because `mulib_unit_profiles_embedded` reports stale embedded
unit profiles. The same mismatch exists against the checked-in `HEAD`
profiles and snapshot; those unrelated files were not changed.

The eight sandbox manual references, library index, dependency strata, and
MuLL inventory were regenerated. Repository-wide `tools/manuals.py --check`
still flags six existing broken links in the virtio/VMM manuals to
`../MULIB_VMMPROTO.md`; those unrelated manuals were not changed.

## Observed behavior

| Enforcement or behavior | What was observed |
| --- | --- |
| Strict worker | Real echo and allocation, address-space and CPU limit checks, production `SIGSYS` policy denials, a wall-deadline kill/reap, rejection of a forged ready frame, sanitized environment and closed inherited descriptor. |
| Framing and broker | Binary and empty frames, malformed/oversize/truncated frames, clean EOF, interrupted I/O with an unchanged deadline, sticky authorization failures and quotas, uncertain commit without abort, and rollback of a staged request after a real worker exited unsuccessfully. |
| Landlock | ABI 7 reported. A restricted child read its allowed file, failed to read outside its allowlist, and failed to open the allowed file writable with truncation. Launcher rejected a deliberately unachievable required ABI. |
| Namespaces | Actual separate user/network namespace, private mount propagation read back from `mountinfo`, PID 1 only for a new child in a standalone PID namespace. Launcher rejected a PID namespace request. |
| Cgroup v2 | Created a dedicated test leaf and verified CPU/memory/swap/PID limits, migrated a live child, saw populated state, refused early removal, issued group kill, reaped and removed the empty group. A separate launcher test enforced the cgroup requirement and verified membership and cleanup. |

## Scope and remaining verification

- Without `MULIB_TEST_CGROUP_PARENT`, the positive cgroup tests report a skip.
  Landlock and standalone namespace fixtures report host restrictions where
  detected; a skip is not enforcement evidence. There were no such skips with
  the supplied delegation on this host.
- Native AArch64, the root-to-dedicated-identity drop path, and additional
  kernels/ABI combinations were not verified in these runs. Standalone
  Landlock and namespace checks are not positive tests of every combination
  with the launcher.
- The launcher does not provide a PID-namespace init/reaper, rootfs isolation,
  support for executing an arbitrary untrusted ELF, or attestation of exact
  installed seccomp bytecode. Its bootstrap and dependencies must be trusted
  and pinned by the supervisor. An independent security review is required
  before admitting hostile third-party production plugins.
- Application worker orchestration and data-plane policy are separate from
  this C11 kernel verification.
