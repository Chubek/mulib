# mulib-androidlibs

[Manuals](README.md) · [JNI](mulib-ezjni.md) · [NDK](mulib-ezndk.md)

An abstraction above **public Android platform libraries**: a native-library/API
catalogue, checked typed dynamic loading, and convenience access to Context
services, package name, SDK level and permission checks. Reviewed against the
package/native API lists and JNI guidance in `standards/android`. It does not
attempt to reimplement the Android framework or expose private platform APIs.

## Integration and native libraries

Define `MULIB_IMPL_ANDROIDLIBS`, `MULIB_IMPL_EZJNI`, `MULIB_IMPL_EZNDK`,
`MULIB_IMPL_BUFFER`, and `MULIB_IMPL_DYNLOAD` once. Host metadata operations need
no Android SDK; link `-ldl` where dynload requires it. Real framework calls opt
into `MULIB_EZJNI_NATIVE` and NDK convenience calls into `MULIB_EZNDK_NATIVE` with
the [documented SDK headers/libraries](mulib-ezndk.md). Neither backend is enabled
implicitly. See the [complete example](../examples/modules/androidlibs.c).

`info` reports immutable sonames, representative headers and library introduction
levels for libc/libm/libdl, log, android, EGL, GLES1/2/3, Vulkan, jnigraphics, media,
camera2ndk, AAudio, binder_ndk, neuralnetworks, nativewindow, sync and zlib.
GLES3 later versions and individual symbols can require newer API levels than
their shared library; GPU capabilities and extensions must be probed separately.

`open` accepts the **actual device API level**, not compileSdk, and loads only a
catalogued public soname with NOW|LOCAL. An older API produces API_LEVEL before
loading. Non-Android hosts produce UNSUPPORTED, not a similarly named host libc
or graphics library. Runtime linker namespace policy can still reject a load.

`function` requires each symbol's introduction API explicitly and copies its
address into an exactly typed function-pointer object through dynload. It is
not a generic callable void* cast: pass `&function, sizeof function` with the
correct C prototype. The API level must be >=library minimum and <=device API.
Successful resolution is neither a permission grant nor hardware availability.
There is no implicit signature inference, private-symbol fallback or global cache.

Zero initialize native owners; close only after releasing callbacks/objects that
depend on their code. Symbols are borrowed until close. Loader errors retain
ownership if release fails. Applications requiring process-lifetime libraries
may retain handles rather than unloading them. Caller-supplied API levels and
prototypes must be truthful; MuLIB cannot infer these facts from raw symbols.

## Framework convenience API

`context_init(env,context,out)` reads public Build.VERSION.SDK_INT, owns a global
Context ref and caches public getSystemService/getPackageName method IDs with
global class refs. Prefer an **application** Context so an Activity is not kept
alive. No JNIEnv is stored: use this thread's attached env from the same VM for
every call. Setup is transactional; `context_free` releases all globals even
with a pending exception. Do not copy/edit live caches or race free with use.

The service catalogue includes activity, window, inflater, power, alarm,
notification, connectivity, Wi-Fi, location, sensor, audio, vibrator, clipboard,
input method, download, camera, job scheduler and battery manager, with minimum
API levels. `service` checks the level and returns a **local** reference; NULL
legitimately means the service is absent. `package` returns a local Java String,
which can be copied through EZJNI's UTF-16 helper. Release local refs explicitly
or enclose calls in a local-frame scope.

`permission` uses checkSelfPermission on API 23+ with an ASCII permission name
(at most 255 characters). It checks but never requests permission, never prompts
the user, and does not bypass Android's security model. JNI exceptions are
preserved; permission output changes only on success. UI threading, Activity
lifecycle, manifest permissions and runtime prompts remain application-owned.
Other framework packages can be accessed using EZJNI's typed cached methods;
the catalogue is not a claim that every Android package has a C wrapper.

## Failures, bounds and evidence

Metadata query outputs are unchanged for invalid IDs. Native loader errors and
API/state errors have separate Androidlibs statuses; JNI convenience functions
return EZJNI statuses, preserving Java exceptions and successful NULL results.
All references/caches need explicit cleanup. No arbitrary managed strings are
passed through NewStringUTF. Catalogue storage is static and allocation-free;
JNI/loader resources follow their backend ownership rules.

`tests/test_androidlibs.c` checks SDK-free catalogue/descriptor/API gating and
`tests/test_android_sdk.c` checks framework calls, temporary local cleanup and
global-cache ownership against explicitly labelled native test doubles. These
tests do not establish Android package/hardware availability on real devices.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-androidlibs.h`](../include/mulib-androidlibs.h) |
| Implementation switch | `MULIB_IMPL_ANDROIDLIBS` |
| Repository-local includes | [mulib-dynload](mulib-dynload.md), [mulib-ezjni](mulib-ezjni.md), [mulib-ezndk](mulib-ezndk.md) |
| Complete example | [`examples/modules/androidlibs.c`](../examples/modules/androidlibs.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/androidlibs.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
