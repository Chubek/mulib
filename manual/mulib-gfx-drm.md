# MuLIB graphics: DRM helpers and legacy backend

These Linux C11 headers use the kernel UAPI, not libdrm. They provide bounded
request construction, enumeration and a caller-configured legacy dumb-buffer
backend. They are **not** a complete atomic compositor or desktop session manager.
Do not use them to take over an active desktop. No test described here opens
`/dev/dri`, `/dev/input`, or a tty.

## Capabilities and return conventions

`mulib-drm-kms.h` detects complete headers in `<drm/...>` or `<libdrm/...>`.
`MULIB_GFX_DRM_UAPI` and `mulib_drm_kms_supported()` report compile-time UAPI
availability, not driver features, permissions or DRM master ownership.
Define `MULIB_GFX_DRM_UAPI=0` to disable it explicitly. Without UAPI, UAPI-typed
helpers are absent; the generic KMS ioctl, session-master and syncobj helpers
return `-1` with `errno=ENOTSUP`. `MULIB_GFX_DRM_BACKEND` reports whether the
legacy adapter was compiled.

New ioctl wrappers return `0/-1` and preserve OS `errno`. Builders, preferred-mode
selection, backend initialization and backend presentation retain their existing
`1/0` success conventions. The original `mulib-drm.h` retains its
`MULIB_DRM_OK`/`MULIB_DRM_E*` results. Define `MULIB_IMPL_DRM` in exactly one
translation unit when using that header's device/dumb-buffer implementation.

`mulib_drm_prime_export` uses the target's visible `O_CLOEXEC | O_RDWR`, never a
numeric architecture guess. Define `_POSIX_C_SOURCE=200809L` **before system
headers** for these flags on supported libc targets. Without `O_CLOEXEC` the
export returns `ENOTSUP`, rather than performing a racy export. The original
device/dumb-buffer API uses `O_CLOEXEC` and `F_DUPFD_CLOEXEC` when visible, retaining
its older `fcntl(FD_CLOEXEC)` fallback where they are unavailable.

Kernel errors such as `ENOTTY`, `EOPNOTSUPP`, `EACCES`, `EBUSY` and syncobj timeout
errors are not converted into successful unsupported operations.

## Ownership and output selection

Initialize resource/connector snapshots and atomic builders to zero. Free them
before fetching replacements; never copy owning objects. Snapshots leave their
output unchanged on failure, limit kernel-reported arrays to 65,536 entries,
and retry count growth up to four times (`EAGAIN` on continuing churn).

`mulib_drm_output_compatible(fd, resources, connector, crtc)` returns `1/0` for
compatibility, `-1` on errors. It uses encoder `possible_crtcs` bits as **indices
into `resources.crtcs`**, not object IDs or connector order.

`mulib_drm_output_find_crtc(fd, resources, connector, reserved, count, &crtc)`
returns `0/-1`; `ENOSPC` means no compatible free CRTC. It excludes CRTCs routed
to other connectors and caller-reserved CRTCs. The connector's existing CRTC
may be reused if not otherwise occupied. It never guesses a primary plane,
handles reservations across processes, or acquires a seat. The caller must hold
exclusive session ownership and re-enumerate after hotplug.

The framebuffer wrapper registers one plane with offset zero and no modifiers.
Packed ARGB/XRGB8888 pitches and integer bounds are checked. Other formats still
require real driver validation; multi-plane/modifier import needs another API.
Exported DMA-BUF fds, imported/opened GEM handles, syncobjs, framebuffer IDs and
property blobs remain caller-owned until their explicit cleanup succeeds.

## Atomic request preparation

`mulib_drm_atomic_add` replaces an existing object/property pair or appends it.
Commit groups property/value arrays by object without losing the builder's
insertion order within each object.

`mulib_drm_atomic_property_find` resolves actual kernel property names, returning
`ENOENT` for missing names and retaining output values on errors. It does not
invent property IDs or interpret property enums. `mulib_drm_atomic_blob` creates
a caller-owned property blob; retain its ID until `mulib_drm_atomic_blob_free`
succeeds.

`mulib_drm_atomic_test` submits
`DRM_MODE_ATOMIC_TEST_ONLY | DRM_MODE_ATOMIC_ALLOW_MODESET`. A successful test
does not commit a mode, reserve resources, or certify hardware support for a
later operation. Full atomic mode/plane selection, fence tracking and session
recovery remain integration work.

## Legacy backend lifecycle

Initialize `mulib_glx_backend_drm_t` to zero. The caller supplies a live
`mulib_drm_device_t`, connected connector, compatible CRTC and mode, and owns
the DRM session. The backend borrows the device and allocates/maps two buffers
through the original dumb-buffer API. It saves the old CRTC, including x/y, and
**all** connectors routed to that CRTC before modesetting. A selected connector
already routed to a different CRTC is rejected with `EBUSY`: restoring multiple
CRTCs safely requires broader session/atomic management. This path does not
negotiate big-endian scanout and explicitly rejects known big-endian targets.

Presentation copies the complete input surface, then requests a real page-flip
event. A pending flip rejects another present or release with `EBUSY`.
`mulib_glx_backend_drm_dispatch` reads events; only a flip-complete event with
the backend's cookie clears `pending` and changes `front`. Vblank notifications
are not flip completions. Event buffers are validated in full before callbacks,
so malformed trailers cannot partially acknowledge presentation.

Set `MGX_CAP_ASYNC_PRESENT` on the caller's platform adapter. After a real event
clears `pending`, acknowledge completion using the shared window/present
completion API. The backend does not automatically drive that API or an event
loop. Do not free/move the backend while its cookie can still be returned.

Prefer `mulib_glx_backend_drm_release_checked`, which returns `0/-1`. The legacy
`void` release wrapper is retained, but cannot return errors. Release restores
old routing first, then removes each framebuffer before destroying its buffer.
On failure, remaining resources and IDs stay in the backend for retry; a restore
failure does not destroy live scanout resources. After successful restoration,
retries do not restore again. The device must remain open for all retries.

Initialization also attempts rollback. It preserves the initiating error in
`errno`; if rollback fails, `backend.device` remains nonnull and
`release_checked` must be retried. Reinitializing an owning backend returns
`EBUSY` instead of overwriting resources. Do not close the device or discard
the object merely because init returned zero.

## Mocking and validation

Define `MULIB_DRM_IOCTL(fd, request, pointer)` before including the new headers
and the original DRM implementation. It replaces native ioctl calls without
global mutable dispatch state; both paths retry `EINTR`. Use the same definition
in each translation unit. Scalar VT requests use the separate
`MULIB_DRM_VT_IOCTL(fd, request, integer)` hook. VT wrappers do not discover a tty,
change permissions, implement logind, or manage VT process-mode signals.

Run from the repository root:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude \
  tests/test-gfx-drm.c -lm -o /tmp/opencode/test-gfx-drm
/tmp/opencode/test-gfx-drm

cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer -g -Iinclude tests/test-gfx-drm.c -lm \
  -o /tmp/opencode/test-gfx-drm-asan
ASAN_OPTIONS=detect_leaks=1 /tmp/opencode/test-gfx-drm-asan

cc -std=c11 -Wall -Wextra -Werror -DMULIB_GFX_DRM_UAPI=0 -Iinclude \
  tests/test-gfx-drm.c -lm -o /tmp/opencode/test-gfx-drm-no-uapi
/tmp/opencode/test-gfx-drm-no-uapi

cc -std=c11 -Wall -Wextra -Werror -Iinclude \
  tests/gfx-drm-prime-no-cloexec.c -o /tmp/opencode/test-gfx-drm-prime-no-cloexec
/tmp/opencode/test-gfx-drm-prime-no-cloexec
```

The tests use real target UAPI declarations, mock ioctls, pipes and a temporary
regular backing file under `/tmp/opencode`. They check native ABI sizes,
alignments, pointer offsets and request codes; close-on-exec fds; integer/pitch
bounds; enumeration churn; CRTC masks; property lookup and atomic grouping;
DMA-BUF/GEM/syncobj arguments and errors; unaligned/malformed event data; complete
legacy routing restoration; and retryable init/restore/RMFB/destroy failures.
Mock success does **not** demonstrate actual driver, KMS, VT or GPU behavior.

The local run used x86-64 GCC 16.2.1 and installed `<drm/...>` UAPI headers.
All ten added DRM headers also passed standalone strict-C11 syntax checks with
UAPI enabled and explicitly disabled. A 32-bit compile probe could not run:
the environment lacks `gnu/stubs-32.h` (multilib libc development headers).

Outstanding gates: 32-bit/native-architecture CI, allocation-failure injection,
VKMS modeset/page-flip/hotplug tests, real session suspend/resume and VT ownership,
multi-output/plane and modifier negotiation, atomic fence/event integration,
and finally controlled hardware tests outside an active desktop.
