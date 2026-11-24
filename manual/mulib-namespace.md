# mulib-namespace

## Build
`MULIB_IMPL_NAMESPACE`, `MULIB_IMPL_SANDBOX_PROFILE`; Linux `-D_GNU_SOURCE`.

`mulib_namespace_enter(&spec,&applied)` validates a bit mask and creates
requested namespaces through unshare. If USER is selected it maps one current
host effective UID and GID to caller-selected IDs, writing UID map, disabling
setgroups, and writing GID map. No subordinate IDs/newuidmap helper are
assumed. Other namespaces are created after USER so namespace-scoped privileges
can enable operations where permitted. The MOUNT branch additionally makes
mount propagation recursively private. Return 0 only after every requested
step succeeds. On failure `applied` identifies completed steps: USER is
reported only after both maps and the setgroups restriction succeed; MOUNT
only after propagation becomes private. Unreported steps may still have
partially changed the process. Terminate child on error.

`mulib_namespace_mount_private()` is a privileged child-only helper to use
**after** creating a private mount namespace; do not run it in the host mount
namespace. This does not construct a rootfs, mount /proc, pivot_root, or close
paths. Network namespace begins isolated from host network (interfaces must be
configured explicitly by an authorized backend). UTS and IPC separation are
not sufficient alone. USER mapping is not identity isolation; drop host root
first in a launch configuration. Supplementary groups are cleared by the root
launcher's identity-drop path; this standalone module has no group guarantee.

PID (`CLONE_NEWPID`) affects future children only. A caller that requests PID
must create and supervise a new child in that namespace, including proper PID 1
reaping; it must not claim the calling process is PID-isolated. The strict
launcher intentionally rejects PID requests, because it promises one child
and no subsequent fork. Standalone namespace API implements the actual unshare;
its PID lifecycle integration and minimal rootfs setup are separate work.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-namespace.h`](../kernels/mulib-namespace.h) |
| Implementation switch | `MULIB_IMPL_NAMESPACE` |
| Repository-local includes | [mulib-sandbox-profile](mulib-sandbox-profile.md) |
| Complete example | [`examples/modules/sandbox-process.c`](../examples/modules/sandbox-process.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/modules/sandbox-process.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
