/* Deterministic native-backend tests against MuLIB-owned SDK test doubles.
 * These test ownership/control flow, not device/JNI binary compatibility. */
#define MULIB_EZJNI_NATIVE
#define MULIB_EZNDK_NATIVE
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_EZJNI
#define MULIB_IMPL_EZNDK
#define MULIB_IMPL_ANDROIDLIBS
#include "mulib-androidlibs.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static int pending, globals, local_strings, attached, attaches, detaches, pushes, pops;
static int lookup_fail, call_fail, push_fail, detach_fail;
static int class_cookie, context_cookie, service_cookie, method_ids[5];
typedef struct test_string { jsize length; jchar text[255]; } test_string;
static JNIEnv environment;
static jboolean exception_check (JNIEnv *e) { (void)e; return (jboolean)pending; }
static void exception_clear (JNIEnv *e) { (void)e; pending = 0; }
static jint push_frame (JNIEnv *e, jint n) { (void)e; assert (!pending && n >= 0); if (push_fail) { pending = 1; return -1; } ++pushes; return 0; }
static jobject pop_frame (JNIEnv *e, jobject keep) { (void)e; ++pops; return keep; }
static jobject global_ref (JNIEnv *e, jobject obj) { (void)e; assert (!pending); ++globals; return obj; }
static void delete_global (JNIEnv *e, jobject obj) { (void)e; assert (obj && globals > 0); --globals; }
static void delete_local (JNIEnv *e, jobject obj)
{ (void)e; if (obj && obj != &class_cookie && obj != &service_cookie && obj != &context_cookie) { --local_strings; free (obj); } }
static jclass find_class (JNIEnv *e, const char *name) { (void)e; assert (!pending && name); return &class_cookie; }
static jmethodID get_method (JNIEnv *e, jclass c, const char *name, const char *sig)
{
  (void)e; assert (!pending && c == &class_cookie && sig);
  if (lookup_fail) { pending = 1; return NULL; }
  if (!strcmp (name, "getSystemService")) return &method_ids[1];
  if (!strcmp (name, "getPackageName")) return &method_ids[2];
  if (!strcmp (name, "checkSelfPermission")) return &method_ids[3];
  return &method_ids[0];
}
static jint call_int (JNIEnv *e, jobject c, jmethodID id, const jvalue *args)
{
  (void)e; assert (!pending && c && id);
  if (call_fail) { pending = 1; return 123; }
  return id == &method_ids[3] ? -1 : args[0].i * 2;
}
static jstring new_string (JNIEnv *e, const jchar *text, jsize n)
{
  test_string *s; (void)e; assert (!pending && n >= 0 && n <= 255 && text);
  s = (test_string *)calloc (1, sizeof (*s)); assert (s); s->length = n;
  memcpy (s->text, text, (size_t)n * sizeof (*text)); ++local_strings; return s;
}
static jsize string_length (JNIEnv *e, jstring obj) { (void)e; assert (!pending); return ((test_string *)obj)->length; }
static void string_region (JNIEnv *e, jstring obj, jsize at, jsize n, jchar *out)
{ test_string *s = (test_string *)obj; (void)e; assert (!pending && at >= 0 && n >= 0 && at + n <= s->length); memcpy (out, s->text + at, (size_t)n * sizeof (*out)); }
static jobject call_object (JNIEnv *e, jobject c, jmethodID id, const jvalue *args)
{
  static const jchar package[] = {'t','e','s','t'};
  assert (!pending && c == &context_cookie);
  if (id == &method_ids[1]) { assert (args[0].l); return &service_cookie; }
  assert (id == &method_ids[2]); return new_string (e, package, 4);
}
static jfieldID get_field (JNIEnv *e, jclass c, const char *name, const char *sig)
{ (void)e; assert (!pending && c && !strcmp (name, "SDK_INT") && !strcmp (sig, "I")); return &method_ids[4]; }
static jint get_int (JNIEnv *e, jclass c, jfieldID id) { (void)e; assert (!pending && c && id); return 33; }
static jint get_env (JavaVM *vm, void **out, jint version)
{ (void)vm; assert (version == JNI_VERSION_1_6); if (!attached) return JNI_EDETACHED; *out = &environment; return 0; }
static jint attach_thread (JavaVM *vm, void **out, void *args)
{ (void)vm; (void)args; attached = 1; ++attaches; *out = &environment; return 0; }
static jint detach_thread (JavaVM *vm) { (void)vm; if (detach_fail) return -1; attached = 0; ++detaches; return 0; }
static const struct JNINativeInterface jni = {
  .ExceptionCheck = exception_check, .ExceptionClear = exception_clear,
  .PushLocalFrame = push_frame, .PopLocalFrame = pop_frame,
  .NewGlobalRef = global_ref, .DeleteGlobalRef = delete_global, .DeleteLocalRef = delete_local,
  .FindClass = find_class, .GetMethodID = get_method, .GetStaticMethodID = get_method,
  .CallIntMethodA = call_int, .CallStaticIntMethodA = call_int,
  .CallObjectMethodA = call_object, .CallStaticObjectMethodA = call_object,
  .NewString = new_string, .GetStringLength = string_length, .GetStringRegion = string_region,
  .GetStaticFieldID = get_field, .GetStaticIntField = get_int
};
static const struct JNIInvokeInterface invocation = {get_env, attach_thread, detach_thread};
static void test_jni (void)
{
  JavaVM vm = &invocation; mulib_ezjni_scope_t scope = {0}; mulib_ezjni_method_t method = {0};
  mulib_ezjni_value_t arg, result; mulib_androidlibs_context_t context = {0};
  const uint16_t unicode[] = {0, 0xd83d, 0xde00}; uint16_t copy[4]; size_t n = 999;
  void *string = NULL, *object = NULL; int granted = 7;
  environment = &jni;
  assert (mulib_ezjni_enter (&vm, 16, &scope) == MULIB_EZJNI_OK && scope.attached && scope.frame);
  assert (mulib_ezjni_leave (&scope, &service_cookie, &object) == MULIB_EZJNI_INVALID);
  assert (mulib_ezjni_leave (&scope, NULL, NULL) == MULIB_EZJNI_OK && !attached && attaches == 1 && detaches == 1 && pushes == pops);
  attached = 1;
  assert (mulib_ezjni_enter (&vm, 16, &scope) == MULIB_EZJNI_OK && !scope.attached);
  assert (mulib_ezjni_leave (&scope, &service_cookie, &object) == MULIB_EZJNI_OK && object == &service_cookie && attached && detaches == 1);
  assert (mulib_ezjni_method (&environment, "test/Api", "twice", "(I)I", 1, &method) == MULIB_EZJNI_OK && globals == 1);
  memset (&arg, 0, sizeof (arg)); arg.i = 7; result.i = -1;
  assert (mulib_ezjni_call (&environment, &method, NULL, &arg, 1, &result) == MULIB_EZJNI_OK && result.i == 14);
  call_fail = 1; result.i = 99;
  assert (mulib_ezjni_call (&environment, &method, NULL, &arg, 1, &result) == MULIB_EZJNI_EXCEPTION && result.i == 99 && pending);
  assert (mulib_ezjni_method_free (&environment, &method) == MULIB_EZJNI_OK && !globals && pending);
  assert (mulib_ezjni_string (&environment, unicode, 3, &string) == MULIB_EZJNI_EXCEPTION && !string);
  assert (mulib_ezjni_clear_exception (&environment) == MULIB_EZJNI_OK); call_fail = 0;
  lookup_fail = 1;
  assert (mulib_ezjni_method (&environment, "test/Api", "bad", "()I", 1, &method) == MULIB_EZJNI_EXCEPTION && !method.clazz && !globals);
  lookup_fail = 0; assert (mulib_ezjni_clear_exception (&environment) == MULIB_EZJNI_OK);
  assert (mulib_ezjni_string (&environment, unicode, 3, &string) == MULIB_EZJNI_OK && local_strings == 1);
  assert (mulib_ezjni_string_copy (&environment, string, NULL, 0, &n) == MULIB_EZJNI_OK && n == 3);
  assert (mulib_ezjni_string_copy (&environment, string, copy, 2, &n) == MULIB_EZJNI_RANGE);
  assert (mulib_ezjni_string_copy (&environment, string, copy, 4, &n) == MULIB_EZJNI_OK && !memcmp (copy, unicode, sizeof (unicode)));
  assert (mulib_ezjni_delete_local (&environment, &string) == MULIB_EZJNI_OK && !local_strings && !string);
  assert (mulib_androidlibs_context_init (&environment, &context_cookie, &context) == MULIB_EZJNI_OK && globals == 3 && context.sdk == 33);
  assert (mulib_androidlibs_service (&environment, &context, MULIB_ANDROIDLIBS_AUDIO, &object) == MULIB_EZJNI_OK && object == &service_cookie && !local_strings);
  assert (mulib_androidlibs_permission (&environment, &context, "android.permission.CAMERA", &granted) == MULIB_EZJNI_OK && !granted && globals == 3 && !local_strings);
  assert (mulib_androidlibs_package (&environment, &context, &string) == MULIB_EZJNI_OK && local_strings == 1);
  assert (mulib_ezjni_delete_local (&environment, &string) == MULIB_EZJNI_OK);
  pending = 1; assert (mulib_androidlibs_context_free (&environment, &context) == MULIB_EZJNI_OK && !globals);
  assert (mulib_ezjni_enter (&vm, 16, &scope) == MULIB_EZJNI_EXCEPTION && !scope.vm && detaches == 1);
  pending = 0; attached = 0; push_fail = detach_fail = 1;
  assert (mulib_ezjni_enter (&vm, 16, &scope) == MULIB_EZJNI_ERROR && scope.attached && !scope.frame);
  detach_fail = 0; assert (mulib_ezjni_leave (&scope, NULL, NULL) == MULIB_EZJNI_OK && !attached);
  push_fail = pending = 0;
}

struct AAsset { const unsigned char *data; size_t length, at; };
static AAsset asset;
static int assets_closed, asset_io;
AAsset *AAssetManager_open (AAssetManager *m, const char *name, int mode)
{ assert (m && mode == AASSET_MODE_STREAMING); asset.at = 0; return !strcmp (name, "missing") ? NULL : &asset; }
int AAsset_read (AAsset *a, void *out, size_t n)
{ size_t count = a->length - a->at; if (asset_io) return -1; if (count > n) count = n; if (count > 2) count = 2; memcpy (out, a->data + a->at, count); a->at += count; return (int)count; }
void AAsset_close (AAsset *a) { assert (a == &asset); ++assets_closed; }
struct ANativeWindow { int refs, locked, posts, format; unsigned char bytes[32]; };
void ANativeWindow_acquire (ANativeWindow *w) { ++w->refs; }
void ANativeWindow_release (ANativeWindow *w) { assert (w->refs && !w->locked); --w->refs; }
int32_t ANativeWindow_setBuffersGeometry (ANativeWindow *w, int32_t width, int32_t height, int32_t format)
{ assert (!w->locked && width >= 0 && height >= 0); if (format) w->format = format; return 0; }
int32_t ANativeWindow_lock (ANativeWindow *w, ANativeWindow_Buffer *b, ARect *r)
{ (void)r; assert (!w->locked); w->locked = 1; b->width = b->height = 2; b->stride = 3; b->format = w->format; b->bits = w->bytes; return 0; }
int32_t ANativeWindow_unlockAndPost (ANativeWindow *w) { assert (w->locked); w->locked = 0; ++w->posts; return 0; }
static int looper_cookie;
ALooper *ALooper_prepare (int flags) { assert (flags == ALOOPER_PREPARE_ALLOW_NON_CALLBACKS); return (ALooper *)&looper_cookie; }
int ALooper_addFd (ALooper *l, int fd, int id, int events, ALooper_callbackFunc cb, void *user)
{ assert (l && fd >= 0 && id >= 0 && events == 3 && !cb && user); return 1; }
int ALooper_removeFd (ALooper *l, int fd) { assert (l && fd >= 0); return 1; }
int ALooper_pollOnce (int timeout, int *fd, int *events, void **user)
{ assert (timeout == 0); *fd = 7; *events = 1; *user = &looper_cookie; return 42; }
int __android_log_write (int priority, const char *tag, const char *text)
{ assert (priority == 4 && !strcmp (tag, "MuLIB") && !strcmp (text, "%s literal")); return 10; }
static void test_ndk (void)
{
  static const unsigned char bytes[] = {0,1,0,3,255};
  mulib_buffer_t b = {0}; ANativeWindow window; mulib_ezndk_window_t owner = {0};
  mulib_ezndk_pixels_t pixels; mulib_ezndk_event_t event; void *looper = NULL;
  memset (&window, 0, sizeof (window)); memset (window.bytes, 0xa5, sizeof (window.bytes)); window.format = 1;
  asset.data = bytes; asset.length = sizeof (bytes);
  assert (mulib_ezndk_asset_load (&asset, "data", sizeof (bytes), &b) == MULIB_EZNDK_OK && b.length == sizeof (bytes) && !memcmp (b.data, bytes, sizeof (bytes)));
  assert (mulib_ezndk_asset_load (&asset, "data", sizeof (bytes) - 1, &b) == MULIB_EZNDK_LIMIT && b.length == sizeof (bytes));
  asset_io = 1; assert (mulib_ezndk_asset_load (&asset, "data", 99, &b) == MULIB_EZNDK_IO); asset_io = 0;
  assert (assets_closed == 3); asset.length = 0;
  assert (mulib_ezndk_asset_load (&asset, "empty", 0, &b) == MULIB_EZNDK_OK && b.length == sizeof (bytes));
  assert (mulib_ezndk_window_open (&window, &owner) == MULIB_EZNDK_OK && window.refs == 1);
  assert (mulib_ezndk_window_fill (&owner, UINT32_C (0x12345678)) == MULIB_EZNDK_OK && !owner.locked && window.posts == 1);
  assert (!memcmp (window.bytes, "\x12\x34\x56\x78\x12\x34\x56\x78", 8) && window.bytes[8] == 0xa5 && window.bytes[12] == 0x12);
  assert (mulib_ezndk_window_lock (&owner, &pixels) == MULIB_EZNDK_OK);
  assert (mulib_ezndk_window_close (&owner) == MULIB_EZNDK_STATE);
  assert (mulib_ezndk_window_lock (&owner, &pixels) == MULIB_EZNDK_STATE);
  assert (mulib_ezndk_window_post (&owner) == MULIB_EZNDK_OK);
  assert (mulib_ezndk_window_geometry (&owner, 0, 0, 4) == MULIB_EZNDK_OK);
  assert (mulib_ezndk_window_fill (&owner, UINT32_C (0xff0000ff)) == MULIB_EZNDK_OK && window.bytes[0] == 0 && window.bytes[1] == 0xf8);
  window.format = 99;
  assert (mulib_ezndk_window_lock (&owner, &pixels) == MULIB_EZNDK_UNSUPPORTED && !owner.locked && !window.locked);
  assert (mulib_ezndk_window_close (&owner) == MULIB_EZNDK_OK && !window.refs && !owner.handle);
  assert (mulib_ezndk_looper_prepare (&looper) == MULIB_EZNDK_OK);
  assert (mulib_ezndk_looper_add (looper, 7, 42, 3, &window) == MULIB_EZNDK_OK);
  assert (mulib_ezndk_looper_poll (0, &event) == MULIB_EZNDK_OK && event.identifier == 42 && event.fd == 7);
  assert (mulib_ezndk_looper_remove (looper, 7) == MULIB_EZNDK_OK);
  assert (mulib_ezndk_log (4, "MuLIB", "%s literal") == MULIB_EZNDK_OK);
  mulib_buffer_free (&b);
}
int main (void) { test_jni (); test_ndk (); return 0; }
