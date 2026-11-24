/** @file mulib-task.h
 * @brief Borrowed native task specifications and explicit execution boundaries.
 * Define MULIB_IMPL_TASK and the EXEC dependencies separately.
 */
#ifndef MULIB_TASK_H
#define MULIB_TASK_H
#include "mulib-exec.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_task_kind {
  MULIB_TASK_COMMAND, MULIB_TASK_FILE, MULIB_TASK_SHELL
} mulib_task_kind_t;
/** Inert borrowed plan. FILE executes source with kernel shebang handling;
 * SHELL explicitly executes source through /bin/sh -c; COMMAND uses command.
 * inputs/outputs are unique logical relative paths, not a native sandbox.
 * Strings, arrays and input bytes must remain immutable through execution. */
typedef struct mulib_task {
  const char *name, *source;
  mulib_task_kind_t kind;
  mulib_exec_command_t command;
  mulib_exec_options_t options;
  const char *const *inputs, *const *outputs;
  size_t input_count, output_count;
} mulib_task_t;
/** Defaults: empty replacement environment, EOF stdin, captured stdout/stderr,
 * 16 MiB per stream and a 60-second deadline. No inherited stdin/environment.
 * cwd and other file descriptors are NOT isolated. */
void mulib_task_init (mulib_task_t *);
/** Validate kind, execution shape and declared path boundaries, without effects.
 * Returns EXEC_OK/INVALID. Detailed OS command validation occurs in EXEC. */
int mulib_task_validate (const mulib_task_t *);
/** Execute synchronously. API status and child exit code are separate; result
 * must be zero initialized and must be destroyed even on execution failure. */
int mulib_task_run (const mulib_task_t *, mulib_exec_result_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TASK) && !defined(MULIB_TASK_IMPLEMENTED)
#define MULIB_TASK_IMPLEMENTED
#include <string.h>
static const char *const mulib_task_empty_env_[] = {NULL};
void mulib_task_init (mulib_task_t *t)
{
  if (!t) return;
  memset (t, 0, sizeof (*t)); mulib_exec_command_init (&t->command);
  t->command.env = mulib_task_empty_env_;
  t->options.input = ""; t->options.capture_stdout = t->options.capture_stderr = 1;
  t->options.timeout_ms = 60000;
}
static int mulib_task_path_ (const char *s)
{
  size_t i = 0, start = 0;
  if (!s || !*s) return 0;
  for (;;) {
    unsigned char c = (unsigned char)s[i];
    if (i > 4095 || (c && (c < 32 || c == 127 || c == '\\' || c == ':'))) return 0;
    if (!c || c == '/') {
      size_t n = i - start;
      if (!n || (n == 1 && s[start] == '.') || (n == 2 && s[start] == '.' && s[start+1] == '.')) return 0;
      if (!c) return 1;
      start = i + 1;
    }
    ++i;
  }
}
int mulib_task_validate (const mulib_task_t *t)
{
  size_t i, j;
  if (!t || !t->name || !*t->name || t->kind < MULIB_TASK_COMMAND || t->kind > MULIB_TASK_SHELL
      || (!t->inputs && t->input_count) || (!t->outputs && t->output_count)
      || t->input_count > 65536 || t->output_count > 65536
      || (!t->options.input && t->options.input_size) || t->options.max_output == SIZE_MAX
      || t->command.argc > 65536 || t->command.envc > 65536
      || (!t->command.env && t->command.envc)
      || t->command.backend < MULIB_EXEC_AUTO || t->command.backend > MULIB_EXEC_SPAWN)
    return MULIB_EXEC_INVALID;
  for (i = 0; i < t->command.envc; ++i)
    if (!t->command.env[i] || !strchr (t->command.env[i], '=') || t->command.env[i][0] == '=')
      return MULIB_EXEC_INVALID;
  if (t->kind == MULIB_TASK_COMMAND) {
    if (!t->command.argv || !t->command.argc || t->command.argc > 65536) return MULIB_EXEC_INVALID;
    for (i = 0; i < t->command.argc; ++i) if (!t->command.argv[i]) return MULIB_EXEC_INVALID;
  } else if (!t->source || (t->kind == MULIB_TASK_FILE && !*t->source)) return MULIB_EXEC_INVALID;
  for (i = 0; i < t->input_count; ++i) {
    if (!mulib_task_path_ (t->inputs[i])) return MULIB_EXEC_INVALID;
    for (j = 0; j < i; ++j) if (!strcmp (t->inputs[i], t->inputs[j])) return MULIB_EXEC_INVALID;
  }
  for (i = 0; i < t->output_count; ++i) {
    if (!mulib_task_path_ (t->outputs[i])) return MULIB_EXEC_INVALID;
    for (j = 0; j < i; ++j) if (!strcmp (t->outputs[i], t->outputs[j])) return MULIB_EXEC_INVALID;
    for (j = 0; j < t->input_count; ++j) if (!strcmp (t->outputs[i], t->inputs[j])) return MULIB_EXEC_INVALID;
  }
  return MULIB_EXEC_OK;
}
int mulib_task_run (const mulib_task_t *t, mulib_exec_result_t *r)
{
  int rc = mulib_task_validate (t);
  if (rc) return rc;
  if (t->kind == MULIB_TASK_FILE) return mulib_exec_run_file (t->source, &t->command, &t->options, r);
  if (t->kind == MULIB_TASK_SHELL) return mulib_exec_run_text (t->source, &t->command, &t->options, r);
  return mulib_exec_run (&t->command, &t->options, r);
}
#endif
