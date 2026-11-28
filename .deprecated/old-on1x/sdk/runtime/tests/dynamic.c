#define _POSIX_C_SOURCE 200809L
#include "On1xRT.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <unistd.h>

#define TEXT(s) ((On1xRT_StringView){s, sizeof(s) - 1u})
#define CHECK(test)                                                            \
  do {                                                                         \
    if (!(test)) {                                                             \
      fprintf(stderr, "dynamic.c:%d: %s\n", __LINE__, #test);                   \
      return 1;                                                                \
    }                                                                          \
  } while (0)

static On1xRT_StringView text(const char *s) {
  return (On1xRT_StringView){s, strlen(s)};
}

static int equal(On1xRT_StringView view, const char *s) {
  return view.size == strlen(s) && !memcmp(view.data, s, view.size);
}

static int trace_is(const char *path, const char *wanted) {
  FILE *file = fopen(path, "rb");
  char trace[128] = {0};
  if (!file)
    return !*wanted;
  const size_t n = fread(trace, 1u, sizeof(trace) - 1u, file);
  const int ok = !ferror(file) && feof(file) && n == strlen(wanted) &&
                 !memcmp(trace, wanted, n);
  (void)fclose(file);
  if (!ok)
    fprintf(stderr, "wanted trace '%s', got '%s'\n", wanted, trace);
  return ok;
}

static size_t open_fds(void) {
  size_t count = 0;
  DIR *dir = opendir("/proc/self/fd");
  if (dir) {
    while (readdir(dir))
      ++count;
    (void)closedir(dir);
  }
  return count;
}

static int copy_file(const char *from, const char *to) {
  FILE *src = fopen(from, "rb");
  if (!src)
    return 0;
  FILE *dst = fopen(to, "wb");
  int ok = dst != NULL;
  char buffer[8192];
  for (size_t n; ok && (n = fread(buffer, 1u, sizeof(buffer), src));)
    ok = fwrite(buffer, 1u, n, dst) == n;
  ok = ok && !ferror(src);
  if (dst && fclose(dst))
    ok = 0;
  (void)fclose(src);
  return ok;
}

typedef struct Policy {
  On1xRT_Engine *engine;
  const char *path;
  const char *digest;
  const char *replacement;
  unsigned artifact_calls;
  unsigned descriptor_calls;
  unsigned mismatches;
  int deny_artifact;
  int deny_descriptor;
} Policy;

static On1xRT_Status authorize_artifact(void *context, On1xRT_StringView path,
                                       On1xRT_StringView digest) {
  Policy *p = context;
  ++p->artifact_calls;
  if (!equal(path, p->path) || !equal(digest, p->digest) ||
      on1x_rt_engine_enable_concurrency(p->engine, NULL) != ON1X_RT_REENTRANT ||
      (p->replacement && rename(p->replacement, p->path))) {
    ++p->mismatches;
    return ON1X_RT_DENIED;
  }
  return p->deny_artifact ? ON1X_RT_DENIED : ON1X_RT_OK;
}

static On1xRT_Status authorize_descriptor(void *context,
                                          const On1xRT_ModuleDescriptor *m) {
  Policy *p = context;
  ++p->descriptor_calls;
  if (m->flags != ON1X_RT_MODULE_AUTHORIZED_AOT ||
      !equal(m->revision, "test-v1") ||
      !equal(m->identity_sha256,
             "aeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeae") ||
      on1x_rt_engine_enable_concurrency(p->engine, NULL) != ON1X_RT_REENTRANT) {
    ++p->mismatches;
    return ON1X_RT_DENIED;
  }
  return p->deny_descriptor ? ON1X_RT_DENIED : ON1X_RT_OK;
}

static On1xRT_Status load(Policy *p, const char *path, const char *digest,
                         On1xRT_Module *module, On1xRT_Error **error) {
  p->path = path;
  p->digest = digest;
  return on1x_rt_engine_load_authorized_dynamic(
      p->engine, text(path), authorize_artifact, p, module, error);
}

static int call(Policy *p, On1xRT_Module module, int request_retry,
                int64_t expected_result) {
  const On1xRT_ValueKind kind = ON1X_RT_VALUE_BOOL;
  const On1xRT_Signature signature = {ON1X_RT_ABI_VERSION,
                                     sizeof(On1xRT_Signature), &kind, 1u,
                                     ON1X_RT_VALUE_I64, 0u};
  On1xRT_Instance instance = {0};
  On1xRT_Function function = {0};
  On1xRT_Value result;
  On1xRT_Value argument = on1x_rt_value_i64(1);
  CHECK(on1x_rt_instance_create(p->engine, module, &instance, NULL) == ON1X_RT_OK);
  CHECK(on1x_rt_module_unload(p->engine, &module, NULL) == ON1X_RT_BUSY);
  CHECK(on1x_rt_instance_function(p->engine, instance, TEXT("value"), &signature,
                                  &function, NULL) == ON1X_RT_OK);
  CHECK(on1x_rt_instance_release(p->engine, &instance, NULL) == ON1X_RT_BUSY);
  CHECK(on1x_rt_function_call(p->engine, function, &argument, 1u, &result, NULL) ==
        ON1X_RT_SIGNATURE_MISMATCH);
  argument = on1x_rt_value_bool(request_retry);
  CHECK(on1x_rt_function_call(p->engine, function, &argument, 1u, &result, NULL) ==
        ON1X_RT_OK);
  CHECK(result.kind == ON1X_RT_VALUE_I64 && result.as.i64 == expected_result);
  CHECK(on1x_rt_function_release(p->engine, &function, NULL) == ON1X_RT_OK);
  CHECK(on1x_rt_instance_release(p->engine, &instance, NULL) == ON1X_RT_OK);
  return 0;
}

static int other_thread(void *context) {
  Policy *p = context;
  On1xRT_Module module = {0};
  On1xRT_Error *error = NULL;
  CHECK(on1x_rt_engine_load_authorized_dynamic(
            p->engine, text(p->path), authorize_artifact, p, &module, &error) ==
        ON1X_RT_WRONG_THREAD);
  CHECK(!error && !module.id && !module.owner);
  return 0;
}

static int run(Policy *p, const char *trace, const char *swap, const char *next,
               const char *const paths[], const char *const digests[]) {
  On1xRT_Module module = {0}, second = {0};
  On1xRT_Error *error = NULL, *retained = NULL;
  const size_t fd_baseline = open_fds();
  thrd_t thread;
  int thread_result;
  CHECK(fd_baseline != 0u);
  CHECK(on1x_rt_feature_status(ON1X_RT_FEATURE_DYNAMIC_MODULE) == ON1X_RT_OK);
  CHECK(on1x_rt_engine_load_dynamic(p->engine, text(paths[1]), &module, NULL) ==
        ON1X_RT_UNSUPPORTED);
  CHECK(load(p, "relative.so", digests[1], &module, NULL) ==
        ON1X_RT_INVALID_ARGUMENT);
  CHECK(load(p, "/dev/null", digests[1], &module, NULL) ==
        ON1X_RT_INVALID_ARGUMENT);
  CHECK(load(p, next, digests[1], &module, NULL) == ON1X_RT_NOT_FOUND);
  CHECK(on1x_rt_engine_load_authorized_dynamic(
            p->engine, text(paths[1]), NULL, NULL, &module, NULL) == ON1X_RT_DENIED);
  p->path = paths[1];
  p->digest = digests[1];
  CHECK(thrd_create(&thread, other_thread, p) == thrd_success);
  CHECK(thrd_join(thread, &thread_result) == thrd_success && !thread_result);
  CHECK(!p->artifact_calls && !p->descriptor_calls);

  FILE *oversized = fopen(next, "wb");
  CHECK(oversized && ftruncate(fileno(oversized),
                               (off_t)ON1X_RT_MAX_DYNAMIC_IMAGE_BYTES + 1) == 0);
  CHECK(fclose(oversized) == 0);
  CHECK(load(p, next, digests[1], &module, NULL) == ON1X_RT_LIMIT);
  CHECK(unlink(next) == 0 && !p->artifact_calls);

  p->deny_artifact = 1;
  CHECK(load(p, paths[1], digests[1], &module, &error) == ON1X_RT_DENIED);
  CHECK(on1x_rt_error_code(error) == ON1X_RT_DENIED);
  CHECK(on1x_rt_error_release(&error) == ON1X_RT_OK);
  CHECK(!module.id && !module.owner && trace_is(trace, "") &&
        p->artifact_calls == 1u && !p->descriptor_calls);
  p->deny_artifact = 0;
  /* Different bytes with a valid On1xRT entry cannot skip the digest policy. */
  CHECK(load(p, paths[2], digests[1], &module, NULL) == ON1X_RT_DENIED);
  CHECK(p->mismatches == 1u && trace_is(trace, "") && !p->descriptor_calls);
  p->mismatches = 0u;
  On1xRT_Engine *unconfigured = NULL;
  CHECK(on1x_rt_engine_create(NULL, &unconfigured, NULL) == ON1X_RT_OK);
  CHECK(on1x_rt_engine_load_authorized_dynamic(unconfigured, text(paths[1]),
      authorize_artifact, p, &module, NULL) == ON1X_RT_DENIED);
  CHECK(p->artifact_calls == 2u && !module.id && !module.owner);
  CHECK(on1x_rt_engine_destroy(&unconfigured, NULL) == ON1X_RT_OK);
  CHECK(open_fds() == fd_baseline);

  FILE *not_elf = fopen(next, "wb");
  CHECK(not_elf && fputs("abc", not_elf) >= 0 && fclose(not_elf) == 0);
  CHECK(load(p, next,
             "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
             &module, NULL) == ON1X_RT_ERROR);
  CHECK(unlink(next) == 0 && trace_is(trace, "") && !p->descriptor_calls);
  CHECK(load(p, paths[2], digests[2], &module, NULL) == ON1X_RT_ABI_MISMATCH);
  CHECK(trace_is(trace, "LEU") && !p->descriptor_calls);
  CHECK(load(p, paths[3], digests[3], &module, NULL) == ON1X_RT_NOT_FOUND);
  CHECK(trace_is(trace, "LEULU") && !p->descriptor_calls);
  CHECK(load(p, paths[4], digests[4], &module, NULL) == ON1X_RT_ABI_MISMATCH);
  CHECK(trace_is(trace, "LEULULU") && !p->descriptor_calls);
  CHECK(load(p, paths[8], digests[8], &module, NULL) == ON1X_RT_ABI_MISMATCH);
  CHECK(trace_is(trace, "LEULULULLUU") && !p->descriptor_calls);
  CHECK(unlink(trace) == 0);

  p->deny_descriptor = 1;
  CHECK(load(p, paths[1], digests[1], &module, NULL) == ON1X_RT_DENIED);
  CHECK(trace_is(trace, "LEU") && p->descriptor_calls == 1u);
  p->deny_descriptor = 0;
  CHECK(load(p, paths[5], digests[5], &module, &retained) ==
        ON1X_RT_CALLBACK_FAILED);
  CHECK(trace_is(trace, "LEULEIU") && p->descriptor_calls == 2u);
  CHECK(!module.id && !module.owner && open_fds() == fd_baseline);
  CHECK(unlink(trace) == 0);

  /* Swap the requested filename after measurement, without touching fixtures. */
  CHECK(copy_file(paths[1], swap) && copy_file(paths[2], next));
  p->replacement = next;
  CHECK(load(p, swap, digests[1], &module, NULL) == ON1X_RT_OK);
  p->replacement = NULL;
  CHECK(trace_is(trace, "LEI"));
  CHECK(load(p, paths[7], digests[7], &second, NULL) == ON1X_RT_OK);
  const unsigned authorizations = p->artifact_calls;
  On1xRT_Module excess = {0};
  CHECK(load(p, paths[4], digests[4], &excess, NULL) == ON1X_RT_LIMIT);
  CHECK(!excess.id && !excess.owner && p->artifact_calls == authorizations);
  CHECK(on1x_rt_module_unload(p->engine, &module, NULL) == ON1X_RT_BUSY);
  CHECK(on1x_rt_module_unload(p->engine, &second, NULL) == ON1X_RT_OK);
  CHECK(on1x_rt_engine_destroy(&p->engine, NULL) == ON1X_RT_BUSY);
  CHECK(call(p, module, 1, 101) == 0);
  On1xRT_Module stale = module;
  CHECK(on1x_rt_module_unload(p->engine, &module, &error) ==
        ON1X_RT_CALLBACK_FAILED);
  CHECK(on1x_rt_error_code(error) == 4243u);
  CHECK(on1x_rt_error_release(&error) == ON1X_RT_OK);
  CHECK(module.id == stale.id && module.owner == stale.owner);
  CHECK(trace_is(trace, "LEILEISUCVDF"));
  CHECK(on1x_rt_module_unload(p->engine, &module, NULL) == ON1X_RT_OK);
  CHECK(trace_is(trace, "LEILEISUCVDFSU") && !module.id && !module.owner);
  CHECK(on1x_rt_module_unload(p->engine, &stale, NULL) == ON1X_RT_STALE_HANDLE);
  CHECK(open_fds() == fd_baseline);
  CHECK(unlink(trace) == 0);

  /* A GNU NODELETE library remains mapped after its loader ref is released.
   * The next different digest must not accidentally resolve its cached path. */
  CHECK(load(p, paths[6], digests[6], &module, NULL) == ON1X_RT_OK);
  CHECK(call(p, module, 0, 106) == 0);
  CHECK(on1x_rt_module_unload(p->engine, &module, NULL) == ON1X_RT_OK);
  CHECK(trace_is(trace, "LEICVDS") && open_fds() == fd_baseline);
  CHECK(load(p, paths[1], digests[1], &module, NULL) == ON1X_RT_OK);
  CHECK(call(p, module, 0, 101) == 0);
  CHECK(on1x_rt_module_unload(p->engine, &module, NULL) == ON1X_RT_OK);
  CHECK(trace_is(trace, "LEICVDSLEICVDSU") && open_fds() == fd_baseline);
  CHECK(!p->mismatches);
  CHECK(on1x_rt_engine_destroy(&p->engine, NULL) == ON1X_RT_OK);
  CHECK(on1x_rt_error_code(retained) == 4242u &&
        equal(on1x_rt_error_message(retained), "init failure"));
  CHECK(on1x_rt_error_release(&retained) == ON1X_RT_OK);
  return 0;
}

int main(int argc, char **argv) {
  /* CMake independently measures fixture bytes; this test does not call the
   * loader's MuLIB hash implementation to derive its expectations. */
  CHECK(argc == 18);
  const char *paths[9] = {0}, *digests[9] = {0};
  for (unsigned i = 1u; i <= 8u; ++i) {
    paths[i] = argv[2u * i];
    digests[i] = argv[2u * i + 1u];
  }
  char directory[4096], trace[4120], swap[4120], next[4120];
  const int n = snprintf(directory, sizeof(directory), "%s/rt-test-XXXXXX", argv[1]);
  CHECK(n > 0 && (size_t)n < sizeof(directory) && mkdtemp(directory));
  (void)snprintf(trace, sizeof(trace), "%s/trace", directory);
  (void)snprintf(swap, sizeof(swap), "%s/swap.so", directory);
  (void)snprintf(next, sizeof(next), "%s/next.so", directory);
  CHECK(setenv("ON1X_RT_TEST_DYNAMIC_TRACE", trace, 1) == 0);
  Policy policy = {0};
  On1xRT_EngineConfig config = {.abi_version = ON1X_RT_ABI_VERSION,
                                 .struct_size = sizeof(config),
                                 .max_modules = 2u,
                                 .authorization_context = &policy,
                                 .authorize_aot = authorize_descriptor};
  CHECK(on1x_rt_engine_create(&config, &policy.engine, NULL) == ON1X_RT_OK);
  const int result = run(&policy, trace, swap, next, paths, digests);
  (void)unsetenv("ON1X_RT_TEST_DYNAMIC_TRACE");
  (void)unlink(trace);
  (void)unlink(swap);
  (void)unlink(next);
  (void)rmdir(directory);
  return result;
}
