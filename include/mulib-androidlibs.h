/** @file mulib-androidlibs.h
 * @brief Android public native-library catalogue and checked framework services.
 * Define MULIB_IMPL_ANDROIDLIBS plus EZJNI, EZNDK, BUFFER and DYNLOAD once.
 * Native SDK backends are optional; no private Android platform APIs are used. */
#ifndef MULIB_ANDROIDLIBS_H
#define MULIB_ANDROIDLIBS_H
#include "mulib-ezjni.h"
#include "mulib-ezndk.h"
#include "mulib-dynload.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_androidlibs_library {
  MULIB_ANDROIDLIBS_C, MULIB_ANDROIDLIBS_M, MULIB_ANDROIDLIBS_DL,
  MULIB_ANDROIDLIBS_LOG, MULIB_ANDROIDLIBS_ANDROID, MULIB_ANDROIDLIBS_EGL,
  MULIB_ANDROIDLIBS_GLES1, MULIB_ANDROIDLIBS_GLES2, MULIB_ANDROIDLIBS_GLES3,
  MULIB_ANDROIDLIBS_VULKAN, MULIB_ANDROIDLIBS_JNIGRAPHICS,
  MULIB_ANDROIDLIBS_MEDIA, MULIB_ANDROIDLIBS_CAMERA, MULIB_ANDROIDLIBS_AAUDIO,
  MULIB_ANDROIDLIBS_BINDER, MULIB_ANDROIDLIBS_NNAPI, MULIB_ANDROIDLIBS_NATIVEWINDOW,
  MULIB_ANDROIDLIBS_SYNC, MULIB_ANDROIDLIBS_Z, MULIB_ANDROIDLIBS_LIBRARY_COUNT
} mulib_androidlibs_library_t;
typedef struct mulib_androidlibs_info {
  const char *name, *soname, *header;
  int min_api;
} mulib_androidlibs_info_t;
/** Query static metadata, even without an Android SDK or device. GPU/library
 * presence does not imply feature/hardware support. Output unchanged on error. */
int mulib_androidlibs_info (mulib_androidlibs_library_t library, mulib_androidlibs_info_t *out);
typedef enum mulib_androidlibs_status {
  MULIB_ANDROIDLIBS_OK = 0, MULIB_ANDROIDLIBS_INVALID = -1,
  MULIB_ANDROIDLIBS_UNSUPPORTED = -2, MULIB_ANDROIDLIBS_API_LEVEL = -3,
  MULIB_ANDROIDLIBS_LOADER_ERROR = -4, MULIB_ANDROIDLIBS_STATE = -5
} mulib_androidlibs_status_t;
typedef struct mulib_androidlibs_native {
  mulib_dynload_t library;
  int device_api, min_api;
} mulib_androidlibs_native_t;
/** Load only public Android sonames, NOW|LOCAL. Zero initialize. Non-Android
 * hosts return UNSUPPORTED, not an unrelated host library with the same name.
 * device_api must be the actual device SDK level, not the compile SDK. */
mulib_androidlibs_status_t mulib_androidlibs_open (mulib_androidlibs_library_t library,
    int device_api, mulib_androidlibs_native_t *out, char *error, size_t capacity);
/** Resolve an exactly typed function pointer via dynload. required_api is this
 * SYMBOL's introduction level; must be >= the library's min_api. No function
 * prototype or GPU/permission guarantee follows from successful resolution. */
mulib_androidlibs_status_t mulib_androidlibs_function (const mulib_androidlibs_native_t *library,
    const char *name, int required_api, void *function, size_t function_size,
    char *error, size_t capacity);
/** Close and reset; live symbols/callbacks must no longer be used. Retains on
 * loader failure. Applications needing process lifetime may simply retain it. */
mulib_androidlibs_status_t mulib_androidlibs_close (mulib_androidlibs_native_t *library);
typedef enum mulib_androidlibs_service {
  MULIB_ANDROIDLIBS_ACTIVITY, MULIB_ANDROIDLIBS_WINDOW, MULIB_ANDROIDLIBS_LAYOUT,
  MULIB_ANDROIDLIBS_POWER, MULIB_ANDROIDLIBS_ALARM, MULIB_ANDROIDLIBS_NOTIFICATION,
  MULIB_ANDROIDLIBS_CONNECTIVITY, MULIB_ANDROIDLIBS_WIFI, MULIB_ANDROIDLIBS_LOCATION,
  MULIB_ANDROIDLIBS_SENSOR, MULIB_ANDROIDLIBS_AUDIO, MULIB_ANDROIDLIBS_VIBRATOR,
  MULIB_ANDROIDLIBS_CLIPBOARD, MULIB_ANDROIDLIBS_INPUT_METHOD,
  MULIB_ANDROIDLIBS_DOWNLOAD, MULIB_ANDROIDLIBS_CAMERA_SERVICE,
  MULIB_ANDROIDLIBS_JOB, MULIB_ANDROIDLIBS_BATTERY, MULIB_ANDROIDLIBS_SERVICE_COUNT
} mulib_androidlibs_service_t;
/** Query Context service name and introduction level without JNI. */
int mulib_androidlibs_service_info (mulib_androidlibs_service_t service,
    const char **name, int *min_api);
/** Owns a global Context ref and global method caches, never a JNIEnv. Use an
 * application Context to avoid keeping an Activity alive. Initialize to zero;
 * acquire/release with an attached env from the same VM. */
typedef struct mulib_androidlibs_context {
  void *object;
  mulib_ezjni_method_t service, package_name;
  int32_t sdk;
} mulib_androidlibs_context_t;
/** Obtain SDK_INT through the public Build.VERSION field. */
mulib_ezjni_status_t mulib_androidlibs_sdk (void *env, int32_t *out);
/** Cache public Context methods and own a global ref transactionally. */
mulib_ezjni_status_t mulib_androidlibs_context_init (void *env, void *context,
    mulib_androidlibs_context_t *out);
/** Release owned globals, including with a Java exception pending. */
mulib_ezjni_status_t mulib_androidlibs_context_free (void *env, mulib_androidlibs_context_t *context);
/** Return a local service ref. NULL is a valid unavailable service, not an
 * exception. Platform permission exceptions remain pending for the caller. */
mulib_ezjni_status_t mulib_androidlibs_service (void *env,
    const mulib_androidlibs_context_t *context, mulib_androidlibs_service_t service, void **out);
/** Return a local Java String containing the application's package name. */
mulib_ezjni_status_t mulib_androidlibs_package (void *env,
    const mulib_androidlibs_context_t *context, void **out);
/** Check, never request, an ASCII Android permission using checkSelfPermission
 * (API 23+). Runtime prompts/UI-thread/lifecycle management remain application
 * responsibilities. Output unchanged on error; never bypasses permissions. */
mulib_ezjni_status_t mulib_androidlibs_permission (void *env,
    const mulib_androidlibs_context_t *context, const char *permission, int *granted);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ANDROIDLIBS) && !defined(MULIB_ANDROIDLIBS_IMPLEMENTED)
#define MULIB_ANDROIDLIBS_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_androidlibs_info (mulib_androidlibs_library_t id, mulib_androidlibs_info_t *out)
{
  static const mulib_androidlibs_info_t table[] = {
    {"C", "libc.so", "stdlib.h", 1}, {"Math", "libm.so", "math.h", 1},
    {"Dynamic linker", "libdl.so", "dlfcn.h", 1}, {"Logging", "liblog.so", "android/log.h", 3},
    {"Native application", "libandroid.so", "android/native_activity.h", 9},
    {"EGL", "libEGL.so", "EGL/egl.h", 9}, {"OpenGL ES 1", "libGLESv1_CM.so", "GLES/gl.h", 4},
    {"OpenGL ES 2", "libGLESv2.so", "GLES2/gl2.h", 5}, {"OpenGL ES 3", "libGLESv3.so", "GLES3/gl3.h", 18},
    {"Vulkan", "libvulkan.so", "vulkan/vulkan.h", 24}, {"Bitmap", "libjnigraphics.so", "android/bitmap.h", 8},
    {"Media", "libmediandk.so", "media/NdkMediaCodec.h", 21}, {"Camera", "libcamera2ndk.so", "camera/NdkCameraManager.h", 24},
    {"AAudio", "libaaudio.so", "aaudio/AAudio.h", 26}, {"Binder", "libbinder_ndk.so", "android/binder_ibinder.h", 29},
    {"Neural Networks", "libneuralnetworks.so", "android/neuralnetworks.h", 27},
    {"Native window", "libnativewindow.so", "android/native_window.h", 26},
    {"Synchronization", "libsync.so", "android/sync.h", 26}, {"zlib", "libz.so", "zlib.h", 3}
  };
  if (!out || (unsigned)id >= sizeof (table) / sizeof (*table)) return 0;
  *out = table[(unsigned)id]; return 1;
}
mulib_androidlibs_status_t mulib_androidlibs_open (mulib_androidlibs_library_t id,
    int api, mulib_androidlibs_native_t *out, char *error, size_t cap)
{
  mulib_androidlibs_info_t info;
  if (!out || !mulib_androidlibs_info (id, &info) || api <= 0) return MULIB_ANDROIDLIBS_INVALID;
  if (out->library.handle || out->library.owned || out->device_api || out->min_api) return MULIB_ANDROIDLIBS_STATE;
  if (api < info.min_api) return MULIB_ANDROIDLIBS_API_LEVEL;
#ifdef __ANDROID__
  if (mulib_dynload_open (&out->library, info.soname, error, cap) != MULIB_DYNLOAD_OK) return MULIB_ANDROIDLIBS_LOADER_ERROR;
  out->device_api = api; out->min_api = info.min_api; return MULIB_ANDROIDLIBS_OK;
#else
  (void)error; (void)cap; return MULIB_ANDROIDLIBS_UNSUPPORTED;
#endif
}
mulib_androidlibs_status_t mulib_androidlibs_function (const mulib_androidlibs_native_t *l,
    const char *name, int api, void *fn, size_t size, char *error, size_t cap)
{
  if (!l || !name || !*name || !fn || !size || api <= 0) return MULIB_ANDROIDLIBS_INVALID;
  if (!l->library.handle || l->min_api <= 0 || l->device_api < l->min_api) return MULIB_ANDROIDLIBS_STATE;
  if (api < l->min_api) return MULIB_ANDROIDLIBS_INVALID;
  if (l->device_api < api) return MULIB_ANDROIDLIBS_API_LEVEL;
  return mulib_dynload_function (&l->library, name, fn, size, error, cap) == MULIB_DYNLOAD_OK
    ? MULIB_ANDROIDLIBS_OK : MULIB_ANDROIDLIBS_LOADER_ERROR;
}
mulib_androidlibs_status_t mulib_androidlibs_close (mulib_androidlibs_native_t *l)
{
  if (!l) return MULIB_ANDROIDLIBS_INVALID;
  if (mulib_dynload_close (&l->library, NULL, 0) != MULIB_DYNLOAD_OK) return MULIB_ANDROIDLIBS_LOADER_ERROR;
  memset (l, 0, sizeof (*l)); return MULIB_ANDROIDLIBS_OK;
}
int mulib_androidlibs_service_info (mulib_androidlibs_service_t id, const char **out, int *api)
{
  static const char *const names[] = {"activity", "window", "layout_inflater", "power", "alarm", "notification",
    "connectivity", "wifi", "location", "sensor", "audio", "vibrator", "clipboard", "input_method", "download", "camera", "jobscheduler", "batterymanager"};
  static const int levels[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 9, 21, 21, 21};
  if (!out || !api || (unsigned)id >= sizeof (names) / sizeof (*names)) return 0;
  *out = names[(unsigned)id]; *api = levels[(unsigned)id]; return 1;
}
mulib_ezjni_status_t mulib_androidlibs_sdk (void *env, int32_t *out)
{ return mulib_ezjni_static_int (env, "android/os/Build$VERSION", "SDK_INT", out); }
mulib_ezjni_status_t mulib_androidlibs_context_free (void *env, mulib_androidlibs_context_t *c)
{
  mulib_ezjni_status_t s;
  if (!c) return MULIB_EZJNI_INVALID;
  s = mulib_ezjni_method_free (env, &c->service); if (s != MULIB_EZJNI_OK) return s;
  s = mulib_ezjni_method_free (env, &c->package_name); if (s != MULIB_EZJNI_OK) return s;
  if (c->object) { s = mulib_ezjni_delete_global (env, &c->object); if (s != MULIB_EZJNI_OK) return s; }
  memset (c, 0, sizeof (*c)); return MULIB_EZJNI_OK;
}
mulib_ezjni_status_t mulib_androidlibs_context_init (void *env, void *object, mulib_androidlibs_context_t *out)
{
  mulib_androidlibs_context_t c; mulib_ezjni_status_t s;
  memset (&c, 0, sizeof (c));
  if (!out || !object || out->object || out->service.clazz || out->service.id
      || out->package_name.clazz || out->package_name.id || out->sdk) return MULIB_EZJNI_INVALID;
  s = mulib_androidlibs_sdk (env, &c.sdk);
  if (s == MULIB_EZJNI_OK && c.sdk < 1) s = MULIB_EZJNI_ERROR;
  if (s == MULIB_EZJNI_OK) s = mulib_ezjni_global (env, object, &c.object);
  if (s == MULIB_EZJNI_OK) s = mulib_ezjni_method (env, "android/content/Context", "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;", 0, &c.service);
  if (s == MULIB_EZJNI_OK) s = mulib_ezjni_method (env, "android/content/Context", "getPackageName", "()Ljava/lang/String;", 0, &c.package_name);
  if (s == MULIB_EZJNI_OK) *out = c; else (void)mulib_androidlibs_context_free (env, &c);
  return s;
}
static mulib_ezjni_status_t mulib_androidlibs_ascii_ (void *env, const char *text, void **out)
{
  uint16_t units[255]; size_t n;
  if (!text) return MULIB_EZJNI_INVALID;
  for (n = 0; text[n]; ++n) { if (n == 255 || (unsigned char)text[n] < 32 || (unsigned char)text[n] > 126) return MULIB_EZJNI_INVALID; units[n] = (unsigned char)text[n]; }
  return mulib_ezjni_string (env, units, n, out);
}
mulib_ezjni_status_t mulib_androidlibs_service (void *env, const mulib_androidlibs_context_t *c,
    mulib_androidlibs_service_t service, void **out)
{
  const char *name; int api; void *str = NULL; mulib_ezjni_value_t arg, result; mulib_ezjni_status_t s;
  if (!c || !c->object || !out || !mulib_androidlibs_service_info (service, &name, &api)) return MULIB_EZJNI_INVALID;
  if (c->sdk < api) return MULIB_EZJNI_UNSUPPORTED;
  s = mulib_androidlibs_ascii_ (env, name, &str); if (s != MULIB_EZJNI_OK) return s;
  memset (&arg, 0, sizeof (arg)); arg.l = str;
  s = mulib_ezjni_call (env, &c->service, c->object, &arg, 1, &result);
  (void)mulib_ezjni_delete_local (env, &str); if (s == MULIB_EZJNI_OK) *out = result.l; return s;
}
mulib_ezjni_status_t mulib_androidlibs_package (void *env, const mulib_androidlibs_context_t *c, void **out)
{
  mulib_ezjni_value_t result; mulib_ezjni_status_t s;
  if (!c || !c->object || !out) return MULIB_EZJNI_INVALID;
  s = mulib_ezjni_call (env, &c->package_name, c->object, NULL, 0, &result);
  if (s == MULIB_EZJNI_OK) *out = result.l;
  return s;
}
mulib_ezjni_status_t mulib_androidlibs_permission (void *env, const mulib_androidlibs_context_t *c,
    const char *permission, int *granted)
{
  mulib_ezjni_method_t method; mulib_ezjni_value_t arg, result; void *str = NULL; mulib_ezjni_status_t s;
  memset (&method, 0, sizeof (method)); memset (&arg, 0, sizeof (arg));
  if (!c || !c->object || !permission || !*permission || !granted) return MULIB_EZJNI_INVALID;
  if (c->sdk < 23) return MULIB_EZJNI_UNSUPPORTED;
  s = mulib_androidlibs_ascii_ (env, permission, &str); if (s != MULIB_EZJNI_OK) return s;
  s = mulib_ezjni_method (env, "android/content/Context", "checkSelfPermission", "(Ljava/lang/String;)I", 0, &method);
  if (s == MULIB_EZJNI_OK) { arg.l = str; s = mulib_ezjni_call (env, &method, c->object, &arg, 1, &result); }
  (void)mulib_ezjni_method_free (env, &method); (void)mulib_ezjni_delete_local (env, &str);
  if (s == MULIB_EZJNI_OK) *granted = result.i == 0;
  return s;
}
#ifdef __cplusplus
}
#endif
#endif
