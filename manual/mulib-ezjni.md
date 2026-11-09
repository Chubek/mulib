# mulib-ezjni

[Manuals](README.md) · [Android services](mulib-androidlibs.md)

A header-only simplification layer **above a real JNI implementation**, not a
replacement JNI ABI. Reviewed against `standards/android/android-jni.txt`.

## Integration

Define `MULIB_IMPL_EZJNI` once. Default builds need no SDK: descriptor parsing
works and JNI-dependent functions return UNSUPPORTED. Define
`MULIB_EZJNI_NATIVE` consistently for the implementation/client build to include
`<jni.h>` and enable native calls. Use your JDK's platform include directories or
the Android NDK sysroot. JNI calls go through supplied JNI function tables; the
header does not create a VM or require linking libjvm merely to use these APIs.
The implementation is C11; JNI-native implementation also accommodates Android's
and desktop C++ AttachCurrentThread pointer signatures. SDK-free declarations
are C++17 compatible and keep JNI's C/C++ type differences behind `void *` handles.

`env` arguments are actual **JNIEnv pointers**, `vm` actual **JavaVM pointers**,
and object/class/string handles actual JNI references, converted to/from void*.
They are not raw managed-object addresses. You must supply live references from
the same VM, correct object/array kinds, and the current thread's env. No wrapper
can make fabricated/dangling JNI handles safe.

## Scopes and references

`enter(vm,capacity,&scope)` gets this thread's env, attaches only if needed, and
pushes a local frame. Zero initialize the scope. `leave` pops the frame and
detaches **only** a thread attached by this scope. Calls must pair on the same
thread, in stack order, before thread exit; a scope is not copyable/reentrant.
It is not a C++ RAII destructor or automatic TLS destructor.

A borrowed-thread scope may preserve one local reference through PopLocalFrame.
An owned-attachment scope cannot return a surviving local reference after detach:
create a global ref first. Failed detach retains enough scope ownership to retry
leave; failed enter may similarly return a partial owned attachment if cleanup
detach fails. Pop consumes the frame even on a subsequent detach failure.

Local/global references have different release APIs. `global` creates an owned
global reference; `delete_global`/`delete_local` clear the supplied reference after
release and are allowed while a Java exception is pending. References are not
compared for identity by this library. Method IDs are never treated as references.

## Cached calls

`method` resolves a slash-separated class name and method descriptor, storing an
owned global class reference, ID, static/instance flag and parsed signature.
`method_class` instead accepts a class supplied by the correct ClassLoader. Use
JNI_OnLoad to resolve application classes: FindClass on an attached native worker
normally sees the system loader, not the app loader. The cache intentionally
stores no JNIEnv and may be used by attached threads from the same VM.

`signature` parses primitive/reference/array descriptors (255 dimensions and
argument-slot limits). Reference argument kinds normalize to L. Constructor/class
initializer calls are deliberately unsupported by this cache API.
`call` accepts exactly typed union values in descriptor order, checks argument
count/Boolean canonicality/static receiver use, and invokes the correct
Call[Static]TypeMethodA family. It never uses C varargs or assumes jvalue layout
matches MuLIB's union. Void results permit NULL output; reference results are
local and may legitimately be NULL. Free caches explicitly with `method_free`.

## Exceptions and data

No helper silently clears Java exceptions. Ordinary calls reject a pending
exception before making disallowed JNI calls, then check again after operations
that can throw. Method outputs remain unchanged on exceptions. Cleanup operations
are exception-safe. `clear_exception` is an explicit caller decision. Detaching
a newly attached thread after failed scope setup necessarily discards that
thread's pending exception; the failure status still reports it. Exceptions never
unwind native C/C++ frames automatically.

String APIs use **UTF-16 units**, including embedded NUL/surrogate units, not
arbitrary UTF-8 passed to NewStringUTF. `string_copy(NULL,0,&length)` queries size;
copying to an undersized buffer returns RANGE without a partial copy. Byte-array
APIs use JNI region operations and avoid pin/Release ownership traps. Lengths
are bounded by JNI's signed 32-bit jsize. Region buffers can be modified if JNI
itself reports an exception during copying; scalar/handle outputs stay unchanged.
`static_int` handles public static fields such as SDK_INT. `register` accepts a
real JNINativeMethod array; native prototypes/annotations/ABI remain caller-owned.

Status values distinguish invalid descriptors/state, range, exception, OOM,
native errors and disabled backends. Handles/caches/scopes must be initialized
and released explicitly; never copy live owners. No process-global caches exist.

[Example](../examples/modules/ezjni.c) is SDK-free. `tests/test_android_sdk.c`
uses clearly marked MuLIB-owned SDK test doubles to exercise native call paths,
pending-exception cleanup, attach/detach balance, global refs and UTF-16 copies.
These tests do not claim device ABI or Java compatibility certification.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ezjni.h`](../include/mulib-ezjni.h) |
| Implementation switch | `MULIB_IMPL_EZJNI` |
| Repository-local includes | None |
| Complete example | [`examples/modules/ezjni.c`](../examples/modules/ezjni.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ezjni.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
