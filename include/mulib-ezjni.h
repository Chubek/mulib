/** @file mulib-ezjni.h
 * @brief Checked JNI scopes, cached methods, references and UTF-16/byte copies.
 * Define MULIB_IMPL_EZJNI once. Opt in to <jni.h> with MULIB_EZJNI_NATIVE.
 * SDK-free builds retain signature parsing and return UNSUPPORTED for JNI work. */
#ifndef MULIB_EZJNI_H
#define MULIB_EZJNI_H
#include <stddef.h>
#include <stdint.h>
#ifdef MULIB_EZJNI_NATIVE
#include <jni.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_ezjni_status {
  MULIB_EZJNI_OK = 0, MULIB_EZJNI_INVALID = -1, MULIB_EZJNI_UNSUPPORTED = -2,
  MULIB_EZJNI_EXCEPTION = -3, MULIB_EZJNI_NOMEM = -4, MULIB_EZJNI_ERROR = -5,
  MULIB_EZJNI_RANGE = -6, MULIB_EZJNI_STATE = -7
} mulib_ezjni_status_t;
/** ABI-neutral handles are the actual JNI pointers, not object addresses or IDs
 * invented by MuLIB. env is JNIEnv*, vm is JavaVM*. Never share env across threads. */
typedef union mulib_ezjni_value {
  uint8_t z; int8_t b; uint16_t c; int16_t s; int32_t i; int64_t j;
  float f; double d; void *l;
} mulib_ezjni_value_t;
typedef struct mulib_ezjni_signature {
  unsigned char parameters[255], result;
  size_t count;
} mulib_ezjni_signature_t;
/** Parse a full JNI descriptor, including arrays/references and 255-slot limit.
 * Reference kinds normalize to L. Output unchanged on failure. */
mulib_ezjni_status_t mulib_ezjni_signature (const char *descriptor,
    mulib_ezjni_signature_t *out);
/** True when this translation unit was built with the native JNI backend. */
int mulib_ezjni_available (void);
typedef struct mulib_ezjni_scope {
  void *vm, *env;
  int attached, frame;
} mulib_ezjni_scope_t;
/** Get current env or attach, then push a local frame. Zero initialize scope.
 * No pending exception is cleared; on failure only a newly attached thread is
 * detached. Must be paired on this same thread, before thread exit. */
mulib_ezjni_status_t mulib_ezjni_enter (void *vm, size_t local_capacity,
    mulib_ezjni_scope_t *scope);
/** Pop frame, optionally preserving one local ref in *out. Preserving a local
 * ref when this scope must detach is invalid: globalize it first. Detaches ONLY
 * if enter attached. Pending exceptions remain pending. Retains detach ownership
 * on failure; retry leave. NULL keep permits NULL out. */
mulib_ezjni_status_t mulib_ezjni_leave (mulib_ezjni_scope_t *scope, void *keep, void **out);
/** EXCEPTION if pending, OK otherwise; never clears. */
mulib_ezjni_status_t mulib_ezjni_exception (void *env);
/** Explicitly clear a pending Java exception; this is never done implicitly. */
mulib_ezjni_status_t mulib_ezjni_clear_exception (void *env);
/** Create an owned global reference; NULL input maps to NULL output. */
mulib_ezjni_status_t mulib_ezjni_global (void *env, void *object, void **out);
/** Delete a global/local reference (including during exception unwinding).
 * *reference is zeroed. These are NOT interchangeable ownership operations. */
mulib_ezjni_status_t mulib_ezjni_delete_global (void *env, void **reference);
mulib_ezjni_status_t mulib_ezjni_delete_local (void *env, void **reference);
typedef struct mulib_ezjni_method {
  void *clazz, *id;
  mulib_ezjni_signature_t signature;
  int is_static;
} mulib_ezjni_method_t;
/** Resolve and own a global class ref + method ID. Initialize cache to zero.
 * Call during JNI_OnLoad for application classes, or use method_class with a
 * class supplied by the right ClassLoader. Cache never stores a thread's env. */
mulib_ezjni_status_t mulib_ezjni_method (void *env, const char *class_name,
    const char *name, const char *descriptor, int is_static, mulib_ezjni_method_t *out);
/** Resolve from a borrowed jclass; cache holds its own global ref. */
mulib_ezjni_status_t mulib_ezjni_method_class (void *env, void *clazz,
    const char *name, const char *descriptor, int is_static, mulib_ezjni_method_t *out);
/** Delete the cache's global class reference; IDs are not object references. */
mulib_ezjni_status_t mulib_ezjni_method_free (void *env, mulib_ezjni_method_t *method);
/** Invoke using the cached descriptor and JNI's ...MethodA family, never C
 * varargs. receiver=NULL for static methods. Reference results are local and
 * may legitimately be NULL. Output unchanged on exception; void permits NULL.
 * Caller supplies union members matching descriptor kinds. */
mulib_ezjni_status_t mulib_ezjni_call (void *env, const mulib_ezjni_method_t *method,
    void *receiver, const mulib_ezjni_value_t *arguments, size_t count,
    mulib_ezjni_value_t *out);
/** Construct/copy UTF-16, including embedded NULs and surrogate code units.
 * Never passes arbitrary UTF-8 to NewStringUTF. Lengths are UTF-16 units.
 * copy(NULL,0,&length) queries size. Results are local references. */
mulib_ezjni_status_t mulib_ezjni_string (void *env, const uint16_t *text, size_t length, void **out);
mulib_ezjni_status_t mulib_ezjni_string_copy (void *env, void *string,
    uint16_t *text, size_t capacity, size_t *length);
/** Copy byte arrays with region calls; never pins Java heap storage. */
mulib_ezjni_status_t mulib_ezjni_bytes (void *env, const void *bytes, size_t length, void **out);
mulib_ezjni_status_t mulib_ezjni_bytes_copy (void *env, void *array,
    size_t offset, void *bytes, size_t length);
/** Read a static int field, useful for Android Build.VERSION.SDK_INT. */
mulib_ezjni_status_t mulib_ezjni_static_int (void *env, const char *class_name,
    const char *field_name, int32_t *out);
/** Register native methods using JNI's JNINativeMethod array, passed as void*
 * to keep default declarations SDK-free. VM validates descriptors/prototypes.
 * Function pointer ABIs remain caller responsibility. */
mulib_ezjni_status_t mulib_ezjni_register (void *env, void *clazz,
    const void *methods, size_t count);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EZJNI) && !defined(MULIB_EZJNI_IMPLEMENTED)
#define MULIB_EZJNI_IMPLEMENTED
#include <limits.h>
#include <string.h>
#if defined(MULIB_EZJNI_NATIVE) && defined(__cplusplus)
/* Android and desktop JNI headers differ in AttachCurrentThread's second
 * argument. Deduce its pointee type rather than casting function pointers. */
template <class E> static jint mulib_ezjni_attach_ (JavaVM *vm,
    jint (JNICALL *fn)(JavaVM *, E **, void *), void **out)
{ E *env = NULL; jint s = fn (vm, &env, NULL); *out = (void *)env; return s; }
#endif
#ifdef MULIB_EZJNI_NATIVE
#ifdef __cplusplus
#define MULIB_EZJNI_CALL_(e,f,...) (e)->f(__VA_ARGS__)
#define MULIB_EZJNI_CALL0_(e,f) (e)->f()
#else
#define MULIB_EZJNI_CALL_(e,f,...) (*(e))->f((e), __VA_ARGS__)
#define MULIB_EZJNI_CALL0_(e,f) (*(e))->f((e))
#endif
#endif
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_ezjni_type_ (const char **cursor, int allow_void, unsigned char *kind, unsigned *slots)
{
  const char *s = *cursor; unsigned arrays = 0; unsigned char k;
  while (*s == '[') { if (++arrays > 255) return 0; ++s; }
  k = (unsigned char)*s++;
  if (!k) return 0;
  if (k == 'L')
    {
      int part = 0;
      while (*s && *s != ';')
        { unsigned char c = (unsigned char)*s++; if (c == '/') { if (!part) return 0; part = 0; }
          else { if (c <= 32 || c == '.' || c == '[' || c == '(' || c == ')') return 0; part = 1; } }
      if (*s++ != ';' || !part) return 0;
    }
  else if (!strchr ("ZBCSIJFD", k) && !(k == 'V' && allow_void && !arrays)) return 0;
  *kind = arrays ? 'L' : k; *slots = !arrays && (k == 'J' || k == 'D') ? 2 : 1;
  *cursor = s; return 1;
}
mulib_ezjni_status_t mulib_ezjni_signature (const char *s, mulib_ezjni_signature_t *out)
{
  mulib_ezjni_signature_t r; unsigned total = 0, slots;
  memset (&r, 0, sizeof (r));
  if (!s || !out || *s++ != '(') return MULIB_EZJNI_INVALID;
  while (*s && *s != ')')
    {
      if (r.count == 255 || !mulib_ezjni_type_ (&s, 0, &r.parameters[r.count], &slots)) return MULIB_EZJNI_INVALID;
      if (total + slots > 255) return MULIB_EZJNI_RANGE;
      total += slots; ++r.count;
    }
  if (*s++ != ')' || !mulib_ezjni_type_ (&s, 1, &r.result, &slots) || *s) return MULIB_EZJNI_INVALID;
  *out = r; return MULIB_EZJNI_OK;
}
int mulib_ezjni_available (void)
{
#ifdef MULIB_EZJNI_NATIVE
  return 1;
#else
  return 0;
#endif
}
mulib_ezjni_status_t mulib_ezjni_exception (void *raw)
{
  if (!raw) return MULIB_EZJNI_INVALID;
#ifdef MULIB_EZJNI_NATIVE
  JNIEnv *e = (JNIEnv *)raw;
  return MULIB_EZJNI_CALL0_ (e, ExceptionCheck) ? MULIB_EZJNI_EXCEPTION : MULIB_EZJNI_OK;
#else
  return MULIB_EZJNI_UNSUPPORTED;
#endif
}
mulib_ezjni_status_t mulib_ezjni_clear_exception (void *raw)
{
  if (!raw) return MULIB_EZJNI_INVALID;
#ifdef MULIB_EZJNI_NATIVE
  JNIEnv *e = (JNIEnv *)raw; MULIB_EZJNI_CALL0_ (e, ExceptionClear); return MULIB_EZJNI_OK;
#else
  return MULIB_EZJNI_UNSUPPORTED;
#endif
}
mulib_ezjni_status_t mulib_ezjni_enter (void *raw, size_t capacity, mulib_ezjni_scope_t *out)
{
  if (!raw || !out || out->vm || out->env || out->attached || out->frame || capacity > INT32_MAX) return MULIB_EZJNI_INVALID;
#ifdef MULIB_EZJNI_NATIVE
  {
    JavaVM *vm = (JavaVM *)raw; JNIEnv *env; void *ptr = NULL; int attached = 0;
    jint s = MULIB_EZJNI_CALL_ (vm, GetEnv, &ptr, JNI_VERSION_1_6);
    if (s == JNI_EDETACHED)
      {
#ifdef __cplusplus
        s = mulib_ezjni_attach_ (vm, vm->functions->AttachCurrentThread, &ptr);
#else
        {
          JNIEnv *typed = NULL;
          int typed_api = _Generic ((*vm)->AttachCurrentThread,
              jint (JNICALL *)(JavaVM *, JNIEnv **, void *): 1, default: 0);
          /* Use storage of the declared pointee type, not a pointer-to-pointer
           * type-pun. Old Android C headers used JNIEnv**, desktop uses void**. */
          s = MULIB_EZJNI_CALL_ (vm, AttachCurrentThread,
              typed_api ? (void *)&typed : (void *)&ptr, NULL);
          if (typed_api) ptr = (void *)typed;
        }
#endif
        if (s != JNI_OK) return MULIB_EZJNI_ERROR;
        attached = 1;
      }
    else if (s != JNI_OK) return MULIB_EZJNI_ERROR;
    env = (JNIEnv *)ptr;
    if (!env) { if (attached) (void)MULIB_EZJNI_CALL0_ (vm, DetachCurrentThread); return MULIB_EZJNI_ERROR; }
    if (MULIB_EZJNI_CALL0_ (env, ExceptionCheck) || MULIB_EZJNI_CALL_ (env, PushLocalFrame, (jint)capacity) != JNI_OK)
      {
        mulib_ezjni_status_t failure = MULIB_EZJNI_CALL0_ (env, ExceptionCheck) ? MULIB_EZJNI_EXCEPTION : MULIB_EZJNI_NOMEM;
        if (attached && MULIB_EZJNI_CALL0_ (vm, DetachCurrentThread) != JNI_OK)
          { out->vm = raw; out->env = ptr; out->attached = 1; return MULIB_EZJNI_ERROR; }
        return failure;
      }
    out->vm = raw; out->env = ptr; out->attached = attached; out->frame = 1; return MULIB_EZJNI_OK;
  }
#else
  return MULIB_EZJNI_UNSUPPORTED;
#endif
}
mulib_ezjni_status_t mulib_ezjni_leave (mulib_ezjni_scope_t *s, void *keep, void **out)
{
  if (!s || (keep && (!out || s->attached))) return MULIB_EZJNI_INVALID;
  if (!s->env && !s->vm && !s->frame && !s->attached) { if (out) *out = NULL; return MULIB_EZJNI_OK; }
  if (!s->env || !s->vm || (keep && !s->frame)) return MULIB_EZJNI_STATE;
#ifdef MULIB_EZJNI_NATIVE
  {
    JNIEnv *env = (JNIEnv *)s->env; JavaVM *vm = (JavaVM *)s->vm; jobject ref = NULL;
    if (s->frame) { ref = MULIB_EZJNI_CALL_ (env, PopLocalFrame, (jobject)keep); s->frame = 0; }
    if (s->attached && MULIB_EZJNI_CALL0_ (vm, DetachCurrentThread) != JNI_OK) return MULIB_EZJNI_ERROR;
    memset (s, 0, sizeof (*s)); if (out) *out = (void *)ref; return MULIB_EZJNI_OK;
  }
#else
  return MULIB_EZJNI_UNSUPPORTED;
#endif
}
mulib_ezjni_status_t mulib_ezjni_global (void *raw, void *obj, void **out)
{
  mulib_ezjni_status_t s;
  if (!out) return MULIB_EZJNI_INVALID;
  s = mulib_ezjni_exception (raw); if (s != MULIB_EZJNI_OK) return s;
#ifdef MULIB_EZJNI_NATIVE
  {
    JNIEnv *e = (JNIEnv *)raw; jobject ref;
    if (!obj) { *out = NULL; return MULIB_EZJNI_OK; }
    ref = MULIB_EZJNI_CALL_ (e, NewGlobalRef, (jobject)obj);
    s = mulib_ezjni_exception (raw); if (s != MULIB_EZJNI_OK) return s;
    if (!ref) return MULIB_EZJNI_NOMEM;
    *out = (void *)ref; return MULIB_EZJNI_OK;
  }
#else
  (void)obj; return MULIB_EZJNI_UNSUPPORTED;
#endif
}
static mulib_ezjni_status_t mulib_ezjni_delete_ (void *raw, void **ref, int global)
{
  if (!raw || !ref) return MULIB_EZJNI_INVALID;
#ifdef MULIB_EZJNI_NATIVE
  JNIEnv *e = (JNIEnv *)raw;
  if (*ref) { if (global) MULIB_EZJNI_CALL_ (e, DeleteGlobalRef, (jobject)*ref); else MULIB_EZJNI_CALL_ (e, DeleteLocalRef, (jobject)*ref); }
  *ref = NULL; return MULIB_EZJNI_OK;
#else
  (void)global; return MULIB_EZJNI_UNSUPPORTED;
#endif
}
mulib_ezjni_status_t mulib_ezjni_delete_global (void *e, void **r) { return mulib_ezjni_delete_ (e, r, 1); }
mulib_ezjni_status_t mulib_ezjni_delete_local (void *e, void **r) { return mulib_ezjni_delete_ (e, r, 0); }
mulib_ezjni_status_t mulib_ezjni_method_class (void *raw, void *clazz, const char *name,
    const char *descriptor, int is_static, mulib_ezjni_method_t *out)
{
  mulib_ezjni_method_t m; mulib_ezjni_status_t s;
  memset (&m, 0, sizeof (m));
  if (!out || out->clazz || out->id || !clazz || !name || !*name || (is_static != 0 && is_static != 1)) return MULIB_EZJNI_INVALID;
  s = mulib_ezjni_signature (descriptor, &m.signature); if (s != MULIB_EZJNI_OK) return s;
  if (!strcmp (name, "<init>") || !strcmp (name, "<clinit>")) return MULIB_EZJNI_UNSUPPORTED;
  if (!is_static)
    { size_t k, slots = 1; for (k = 0; k < m.signature.count; ++k) slots += m.signature.parameters[k] == 'J' || m.signature.parameters[k] == 'D' ? 2 : 1; if (slots > 255) return MULIB_EZJNI_RANGE; }
  s = mulib_ezjni_exception (raw); if (s != MULIB_EZJNI_OK) return s;
#ifdef MULIB_EZJNI_NATIVE
  {
    JNIEnv *e = (JNIEnv *)raw; jmethodID id;
    id = is_static ? MULIB_EZJNI_CALL_ (e, GetStaticMethodID, (jclass)clazz, name, descriptor)
                   : MULIB_EZJNI_CALL_ (e, GetMethodID, (jclass)clazz, name, descriptor);
    s = mulib_ezjni_exception (raw); if (s != MULIB_EZJNI_OK) return s;
    if (!id) return MULIB_EZJNI_ERROR;
    s = mulib_ezjni_global (raw, clazz, &m.clazz); if (s != MULIB_EZJNI_OK) return s;
    m.id = (void *)id; m.is_static = is_static; *out = m; return MULIB_EZJNI_OK;
  }
#else
  return MULIB_EZJNI_UNSUPPORTED;
#endif
}
mulib_ezjni_status_t mulib_ezjni_method (void *raw, const char *class_name,
    const char *name, const char *descriptor, int is_static, mulib_ezjni_method_t *out)
{
  mulib_ezjni_status_t s;
  if (!class_name || !*class_name || strchr (class_name, '.')) return MULIB_EZJNI_INVALID;
  s = mulib_ezjni_exception (raw); if (s != MULIB_EZJNI_OK) return s;
#ifdef MULIB_EZJNI_NATIVE
  {
    JNIEnv *e = (JNIEnv *)raw; jclass c = MULIB_EZJNI_CALL_ (e, FindClass, class_name);
    s = mulib_ezjni_exception (raw);
    if (s == MULIB_EZJNI_OK) s = c ? mulib_ezjni_method_class (raw, (void *)c, name, descriptor, is_static, out) : MULIB_EZJNI_ERROR;
    if (c) MULIB_EZJNI_CALL_ (e, DeleteLocalRef, c);
    return s;
  }
#else
  (void)name; (void)descriptor; (void)is_static; (void)out; return MULIB_EZJNI_UNSUPPORTED;
#endif
}
mulib_ezjni_status_t mulib_ezjni_method_free (void *e, mulib_ezjni_method_t *m)
{
  mulib_ezjni_status_t s;
  if (!m) return MULIB_EZJNI_INVALID;
  if (!m->clazz && !m->id) { memset (m, 0, sizeof (*m)); return MULIB_EZJNI_OK; }
  s = mulib_ezjni_delete_global (e, &m->clazz); if (s == MULIB_EZJNI_OK) memset (m, 0, sizeof (*m)); return s;
}
mulib_ezjni_status_t mulib_ezjni_call (void *raw, const mulib_ezjni_method_t *m,
    void *receiver, const mulib_ezjni_value_t *args, size_t count, mulib_ezjni_value_t *out)
{
  mulib_ezjni_status_t s;
  if (!m || !m->clazz || !m->id || count != m->signature.count || count > 255
      || (count && !args) || (m->is_static ? receiver != NULL : receiver == NULL)
      || (!out && m->signature.result != 'V')) return MULIB_EZJNI_INVALID;
  s = mulib_ezjni_exception (raw); if (s != MULIB_EZJNI_OK) return s;
#ifdef MULIB_EZJNI_NATIVE
  {
    JNIEnv *e = (JNIEnv *)raw; jvalue values[255]; mulib_ezjni_value_t result;
    size_t k; memset (&result, 0, sizeof (result)); memset (values, 0, sizeof (values));
    for (k = 0; k < count; ++k) switch (m->signature.parameters[k]) {
      case 'Z': if (args[k].z > 1) return MULIB_EZJNI_RANGE; values[k].z = args[k].z; break;
      case 'B': values[k].b = args[k].b; break; case 'C': values[k].c = args[k].c; break;
      case 'S': values[k].s = args[k].s; break; case 'I': values[k].i = args[k].i; break;
      case 'J': values[k].j = args[k].j; break; case 'F': values[k].f = args[k].f; break;
      case 'D': values[k].d = args[k].d; break; case 'L': values[k].l = (jobject)args[k].l; break;
      default: return MULIB_EZJNI_INVALID;
    }
#define MULIB_EZJNI_INVOKE_(field,Type) do { result.field = m->is_static ? MULIB_EZJNI_CALL_ (e, CallStatic##Type##MethodA, (jclass)m->clazz, (jmethodID)m->id, values) : MULIB_EZJNI_CALL_ (e, Call##Type##MethodA, (jobject)receiver, (jmethodID)m->id, values); } while (0)
    switch (m->signature.result) {
      case 'Z': MULIB_EZJNI_INVOKE_ (z, Boolean); break; case 'B': MULIB_EZJNI_INVOKE_ (b, Byte); break;
      case 'C': MULIB_EZJNI_INVOKE_ (c, Char); break; case 'S': MULIB_EZJNI_INVOKE_ (s, Short); break;
      case 'I': MULIB_EZJNI_INVOKE_ (i, Int); break; case 'J': MULIB_EZJNI_INVOKE_ (j, Long); break;
      case 'F': MULIB_EZJNI_INVOKE_ (f, Float); break; case 'D': MULIB_EZJNI_INVOKE_ (d, Double); break;
      case 'L': MULIB_EZJNI_INVOKE_ (l, Object); break;
      case 'V':
        if (m->is_static) MULIB_EZJNI_CALL_ (e, CallStaticVoidMethodA, (jclass)m->clazz, (jmethodID)m->id, values);
        else MULIB_EZJNI_CALL_ (e, CallVoidMethodA, (jobject)receiver, (jmethodID)m->id, values);
        break;
      default: return MULIB_EZJNI_INVALID;
    }
#undef MULIB_EZJNI_INVOKE_
    s = mulib_ezjni_exception (raw); if (s != MULIB_EZJNI_OK) return s;
    if (out) *out = result;
    return MULIB_EZJNI_OK;
  }
#else
  return MULIB_EZJNI_UNSUPPORTED;
#endif
}
mulib_ezjni_status_t mulib_ezjni_string (void *raw, const uint16_t *text, size_t n, void **out)
{
  mulib_ezjni_status_t s;
  if (!out || (n && !text) || n > INT32_MAX) return MULIB_EZJNI_INVALID;
  s = mulib_ezjni_exception (raw); if (s != MULIB_EZJNI_OK) return s;
#ifdef MULIB_EZJNI_NATIVE
  {
    JNIEnv *e = (JNIEnv *)raw; jchar empty = 0; jstring str = MULIB_EZJNI_CALL_ (e, NewString, text ? (const jchar *)text : &empty, (jsize)n);
    s = mulib_ezjni_exception (raw); if (s != MULIB_EZJNI_OK) return s;
    if (!str) return MULIB_EZJNI_NOMEM;
    *out = (void *)str; return MULIB_EZJNI_OK;
  }
#else
  return MULIB_EZJNI_UNSUPPORTED;
#endif
}
mulib_ezjni_status_t mulib_ezjni_string_copy (void *raw, void *str, uint16_t *text, size_t cap, size_t *length)
{
  mulib_ezjni_status_t s;
  if (!str || !length || (cap && !text)) return MULIB_EZJNI_INVALID;
  s = mulib_ezjni_exception (raw); if (s != MULIB_EZJNI_OK) return s;
#ifdef MULIB_EZJNI_NATIVE
  {
    JNIEnv *e = (JNIEnv *)raw; jsize n = MULIB_EZJNI_CALL_ (e, GetStringLength, (jstring)str);
    s = mulib_ezjni_exception (raw); if (s != MULIB_EZJNI_OK) return s;
    if (n < 0) return MULIB_EZJNI_ERROR;
    if (!text && !cap) { *length = (size_t)n; return MULIB_EZJNI_OK; }
    if ((size_t)n > cap) return MULIB_EZJNI_RANGE;
    if (n) MULIB_EZJNI_CALL_ (e, GetStringRegion, (jstring)str, 0, n, (jchar *)text);
    s = mulib_ezjni_exception (raw); if (s != MULIB_EZJNI_OK) return s;
    *length = (size_t)n; return MULIB_EZJNI_OK;
  }
#else
  return MULIB_EZJNI_UNSUPPORTED;
#endif
}
mulib_ezjni_status_t mulib_ezjni_bytes (void *raw, const void *bytes, size_t n, void **out)
{
  mulib_ezjni_status_t s;
  if (!out || (n && !bytes) || n > INT32_MAX) return MULIB_EZJNI_INVALID;
  s = mulib_ezjni_exception (raw); if (s != MULIB_EZJNI_OK) return s;
#ifdef MULIB_EZJNI_NATIVE
  {
    JNIEnv *e = (JNIEnv *)raw; jbyteArray a = MULIB_EZJNI_CALL_ (e, NewByteArray, (jsize)n);
    s = mulib_ezjni_exception (raw); if (s != MULIB_EZJNI_OK) return s;
    if (!a) return MULIB_EZJNI_NOMEM;
    if (n) MULIB_EZJNI_CALL_ (e, SetByteArrayRegion, a, 0, (jsize)n, (const jbyte *)bytes);
    s = mulib_ezjni_exception (raw); if (s != MULIB_EZJNI_OK) { MULIB_EZJNI_CALL_ (e, DeleteLocalRef, a); return s; }
    *out = (void *)a; return MULIB_EZJNI_OK;
  }
#else
  return MULIB_EZJNI_UNSUPPORTED;
#endif
}
mulib_ezjni_status_t mulib_ezjni_bytes_copy (void *raw, void *array, size_t at, void *bytes, size_t n)
{
  mulib_ezjni_status_t s;
  if (!array || (n && !bytes) || at > INT32_MAX || n > INT32_MAX - at) return MULIB_EZJNI_INVALID;
  s = mulib_ezjni_exception (raw); if (s != MULIB_EZJNI_OK) return s;
#ifdef MULIB_EZJNI_NATIVE
  {
    JNIEnv *e = (JNIEnv *)raw; jsize size = MULIB_EZJNI_CALL_ (e, GetArrayLength, (jarray)array);
    s = mulib_ezjni_exception (raw); if (s != MULIB_EZJNI_OK) return s;
    if (size < 0 || at + n > (size_t)size) return MULIB_EZJNI_RANGE;
    if (n) MULIB_EZJNI_CALL_ (e, GetByteArrayRegion, (jbyteArray)array, (jsize)at, (jsize)n, (jbyte *)bytes);
    return mulib_ezjni_exception (raw);
  }
#else
  return MULIB_EZJNI_UNSUPPORTED;
#endif
}
mulib_ezjni_status_t mulib_ezjni_static_int (void *raw, const char *cls, const char *field, int32_t *out)
{
  mulib_ezjni_status_t s;
  if (!cls || !*cls || !field || !*field || !out || strchr (cls, '.')) return MULIB_EZJNI_INVALID;
  s = mulib_ezjni_exception (raw); if (s != MULIB_EZJNI_OK) return s;
#ifdef MULIB_EZJNI_NATIVE
  {
    JNIEnv *e = (JNIEnv *)raw; jclass c = MULIB_EZJNI_CALL_ (e, FindClass, cls); jfieldID id = NULL; jint value = 0;
    s = mulib_ezjni_exception (raw);
    if (s == MULIB_EZJNI_OK && !c) s = MULIB_EZJNI_ERROR;
    if (s == MULIB_EZJNI_OK) { id = MULIB_EZJNI_CALL_ (e, GetStaticFieldID, c, field, "I"); s = mulib_ezjni_exception (raw); if (s == MULIB_EZJNI_OK && !id) s = MULIB_EZJNI_ERROR; }
    if (s == MULIB_EZJNI_OK) { value = MULIB_EZJNI_CALL_ (e, GetStaticIntField, c, id); s = mulib_ezjni_exception (raw); }
    if (c) MULIB_EZJNI_CALL_ (e, DeleteLocalRef, c);
    if (s == MULIB_EZJNI_OK) *out = value;
    return s;
  }
#else
  return MULIB_EZJNI_UNSUPPORTED;
#endif
}
mulib_ezjni_status_t mulib_ezjni_register (void *raw, void *clazz, const void *methods, size_t n)
{
  mulib_ezjni_status_t s;
  if (!clazz || (n && !methods) || n > INT32_MAX) return MULIB_EZJNI_INVALID;
  s = mulib_ezjni_exception (raw); if (s != MULIB_EZJNI_OK) return s;
#ifdef MULIB_EZJNI_NATIVE
  { JNIEnv *e = (JNIEnv *)raw; jint rc = MULIB_EZJNI_CALL_ (e, RegisterNatives, (jclass)clazz, (const JNINativeMethod *)methods, (jint)n);
    s = mulib_ezjni_exception (raw); return s != MULIB_EZJNI_OK ? s : rc == JNI_OK ? MULIB_EZJNI_OK : MULIB_EZJNI_ERROR; }
#else
  return MULIB_EZJNI_UNSUPPORTED;
#endif
}
#ifdef __cplusplus
}
#endif
#ifdef MULIB_EZJNI_NATIVE
#undef MULIB_EZJNI_CALL_
#undef MULIB_EZJNI_CALL0_
#endif
#endif
