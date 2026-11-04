#include "../exec-common.h"
#include "mulib-exec.h"
#include <assert.h>
#include <string.h>
int main (void)
{
  const char *a[] = {"printf", "%s", "literal ; $HOME"};
  const char *b[] = {"tr", "a-z", "A-Z"};
  mulib_exec_command_t commands[2];
  mulib_exec_options_t options = {0};
  mulib_exec_result_t result = {0};
  int rc;
  mulib_exec_command_init (&commands[0]);
  mulib_exec_command_init (&commands[1]);
  commands[0].argv = a; commands[0].argc = 3;
  commands[1].argv = b; commands[1].argc = 3;
  options.capture_stdout = options.capture_stderr = options.pipefail = 1;
  options.timeout_ms = 5000;
  rc = mulib_exec_pipeline (commands, 2, &options, &result);
  if (rc == MULIB_EXEC_UNSUPPORTED) return 0;
  assert (!rc && !result.code);
  assert (!strcmp (result.out, "LITERAL ; $HOME"));
  mulib_exec_result_destroy (&result);
  return 0;
}
