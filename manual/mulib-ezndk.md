# mulib-ezndk

[Manuals](README.md) · [Android libraries](mulib-androidlibs.md)

A small header-only convenience layer for Android NDK assets, software native
windows, non-callback looper FDs, and literal logcat output. Reviewed against
`standards/android/android-native.txt`; it is not the entire NDK.

## Integration

Define `MULIB_IMPL_EZNDK` and `MULIB_IMPL_BUFFER` once. Without configuration the
module compiles on ordinary hosts and native operations return UNSUPPORTED.
Define `MULIB_EZNDK_NATIVE` to include real NDK headers and enable its API-9+
backend. Build with the NDK sysroot, target a supported Android ABI and link
`-landroid -llog`. This exception to MuLIB's default external-header restriction
is opt-in. No private platform functions, external implementation libraries or
implicit feature-test macros are used.

## Assets and windows

`asset_load(manager,path,max_bytes,out)` borrows AAssetManager for the call,
reads through AAsset's streaming interface (compressed assets work), closes on
every path, and atomically appends owned bytes. Empty assets and embedded NULs
are supported. A one-byte probe distinguishes an exact-limit/empty asset from
an over-limit one; no partial output appears on failure. No seekable/64-bit
asset-length API is assumed and no native buffer pointer escapes.

Zero initialize `mulib_ezndk_window_t`. `window_open` acquires one reference to
a borrowed ANativeWindow. Geometry changes require an unlocked window; width
and height are both zero (base size) or both positive. Supported software formats
are RGBA8888=1, RGBX8888=2, RGB565=4; zero geometry format retains the base format.

`window_lock` validates positive dimensions, stride>=width, format, buffer pointer
and size multiplication. On success the borrowed pixel view lives only through
`window_post`, and row_bytes includes stride padding. Never free/store that view
after posting. No nested locks or close-while-locked are allowed. A failed native
post still consumes the lock. Invalid/unsupported native buffers are unlocked
without exposing them. `window_fill` writes only visible pixels and posts;
its color is 0xRRGGBBAA independent of host endianness. RGBX sets X opaque;
RGB565 quantizes channels and writes Android little-endian pixels. Close releases
our acquired reference, not the caller's borrowed reference.

## Looper and logging

`looper_prepare` returns this thread's **borrowed** ALooper, with non-callback FDs
enabled; do not release it unless separately acquired. Add/remove do not own FDs
or callback/user pointers. Registration IDs are >=0 and events use INPUT=1 /
OUTPUT=2. `looper_poll` uses PollOnce on the current thread, never PollAll, with
timeout -1 blocking or >=0 milliseconds. It preserves native wake/callback/timeout
IDs (-1/-2/-3); -4 reports IO. Polling must occur on the looper's thread.

`log(priority,tag,text)` uses __android_log_write, so percent signs are literal.
Priorities are 2..7; empty tags and NULL text are invalid. There is no hidden stderr
fallback on hosts without Android. This module does not promise audio, camera,
Vulkan, Binder, native-activity lifecycle, input dispatch, hardware rendering or
thread marshalling; use [androidlibs](mulib-androidlibs.md) for checked access to
other public native libraries and JNI for framework functionality.

## Ownership, failures and evidence

Live window owners are not copyable; handles and pixel data must remain valid
and operations must obey the producer's threading/lifecycle requirements. There
is no implicit synchronization or UI-thread dispatch. Scalar/owner outputs remain
unchanged on setup failure. Asset errors distinguish missing asset, IO, limits,
OOM and disabled backend. Invalid fake NDK handles cannot be made safe by wrappers.
`window_close` is idempotent when empty; release locked buffers first.

The [example](../examples/modules/ezndk.c) runs without a SDK; with a native backend
it writes logcat. `tests/test_android_sdk.c` uses explicit SDK test doubles for
compressed-style short reads, exact/over-limit and IO paths, reference/lock balance,
stride-aware pixel formats, looper events and logging. It is not an on-device
compatibility or hardware test.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ezndk.h`](../include/mulib-ezndk.h) |
| Implementation switch | `MULIB_IMPL_EZNDK` |
| Repository-local includes | [mulib-buffer](mulib-buffer.md) |
| Complete example | [`examples/modules/ezndk.c`](../examples/modules/ezndk.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ezndk.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
