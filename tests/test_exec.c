#include "../examples/exec-common.h"
#include "mulib-exec.h"
#include <assert.h>

#if !defined(MULIB_EXEC_NO_POSIX) && (defined(__unix__) || defined(__APPLE__))
static void command (mulib_exec_command_t *c, const char *const *a, size_t n,
                     mulib_exec_backend_t backend)
{
  mulib_exec_command_init (c); c->argv = a; c->argc = n; c->backend = backend;
}
static void run_script (mulib_mune_state_t *s, const char *source)
{
  mulib_mune_result_t *r = NULL;
  mulib_mune_error_t e;
  int rc = mulib_mune_run_source (s, source, strlen (source), &r, &e);
  if (rc) fprintf (stderr, "DSL: %s at %zu\n", e.message, e.offset);
  assert (!rc); mulib_mune_result_destroy (r);
}
static void backend (mulib_exec_backend_t mode)
{
  mulib_exec_command_t c[2];
  mulib_exec_options_t o = {0};
  mulib_exec_result_t r = {0};
  const char *literal[] = {"printf", "%s", "x ; $HOME ' \" *\n"};
  const char *cat[] = {"cat"};
  const char *missing[] = {"/mulib-exec-definitely-missing"};
  const char *fail[] = {"sh", "-c", "exit 7"};
  const char *slow[] = {"sleep", "10"};
  const char *large[] = {"sh", "-c", "i=0; while [ $i -lt 10000 ]; do printf 1234567890; printf abcdefghij >&2; i=$((i+1)); done"};
  char *input = (char *)malloc (200000);
  assert (input); memset (input, 'x', 200000); input[99999] = 0;
  o.capture_stdout = o.capture_stderr = 1; o.timeout_ms = 5000;
  command (c,literal,3,mode);
  assert (!mulib_exec_run (c,&o,&r) && !r.code);
  assert (!strcmp (r.out,literal[2])); mulib_exec_result_destroy (&r);
  command (c,cat,1,mode); command (c+1,cat,1,mode);
  o.input = input; o.input_size = 200000;
  assert (!mulib_exec_pipeline (c,2,&o,&r) && !r.code && r.count == 2);
  assert (r.out_size == 200000 && !memcmp (r.out,input,200000));
  mulib_exec_result_destroy (&r); o.input = NULL; o.input_size = 0; free (input);
  command (c,large,3,mode);
  assert (!mulib_exec_run (c,&o,&r) && !r.code && r.out_size == 100000 && r.err_size == 100000);
  mulib_exec_result_destroy (&r);
  o.max_output = 10;
  assert (mulib_exec_run (c,&o,&r) == MULIB_EXEC_LIMIT);
  assert (r.stages[0].exited || r.stages[0].term_signal); mulib_exec_result_destroy (&r); o.max_output = 0;
  command (c,fail,3,mode); command (c+1,cat,1,mode);
  assert (!mulib_exec_pipeline (c,2,&o,&r) && !r.code && r.stages[0].exit_code == 7);
  mulib_exec_result_destroy (&r); o.pipefail = 1;
  assert (!mulib_exec_pipeline (c,2,&o,&r) && r.code == 7); mulib_exec_result_destroy (&r);
  command (c,slow,2,mode); command (c+1,missing,1,mode);
  assert (mulib_exec_pipeline (c,2,&o,&r) == MULIB_EXEC_SYSTEM);
  assert (r.failed_stage == 1 && r.system_error == ENOENT && r.stages[0].term_signal == SIGKILL);
  mulib_exec_result_destroy (&r);
  o.timeout_ms = 30;
  assert (mulib_exec_run (c,&o,&r) == MULIB_EXEC_TIMEOUT);
  assert (r.stages[0].term_signal == SIGKILL); mulib_exec_result_destroy (&r);
  command (c,missing,1,mode);
  assert (mulib_exec_run (c,&o,&r) == MULIB_EXEC_SYSTEM && r.system_error == ENOENT);
  mulib_exec_result_destroy (&r);
  {
    const char *env[] = {"PATH=/bin:/usr/bin", "MULIB_EXEC_VALUE=literal space"};
    const char *show[] = {"sh", "-c", "printf '%s' \"$MULIB_EXEC_VALUE\""};
    command (c,show,3,mode); c->env = env; c->envc = 2; o.timeout_ms = 5000;
    assert (!mulib_exec_run (c,&o,&r) && !strcmp (r.out,"literal space")); mulib_exec_result_destroy (&r);
  }
  {
    const char *write[] = {"sh", "-c", "printf out; printf err >&2"};
    FILE *f;
    char bytes[16] = {0};
    command (c,write,3,mode); c->stdout_path = "exec-redirection.txt"; c->merge_stderr = 1;
    o.capture_stdout = o.capture_stderr = 0;
    assert (!mulib_exec_run (c,&o,&r) && !r.code); mulib_exec_result_destroy (&r);
    c->append_stdout = 1;
    assert (!mulib_exec_run (c,&o,&r) && !r.code); mulib_exec_result_destroy (&r);
    f = fopen ("exec-redirection.txt","rb"); assert (f);
    assert (fread (bytes,1,sizeof (bytes),f) == 12 && !strcmp (bytes,"outerrouterr"));
    fclose (f); assert (!remove ("exec-redirection.txt"));
  }
  {
    mulib_process_t p = {0}; mulib_process_status_t st;
    command (c,slow,2,mode);
    assert (!mulib_exec_start (c,&p));
    assert (mulib_process_wait (&p,&st,1) == MULIB_PROCESS_RUNNING);
    assert (mulib_exec_signal (&p,SIGTERM,&st) == MULIB_PROCESS_RUNNING);
    assert (!mulib_process_wait (&p,&st,0) && st.term_signal == SIGTERM && !p.pid);
  }
}
#endif
int main (void)
{
#if !defined(MULIB_EXEC_NO_POSIX) && (defined(__unix__) || defined(__APPLE__))
  mulib_mune_state_t *s;
  mulib_exec_command_t c;
  mulib_exec_options_t o = {0};
  mulib_exec_result_t r = {0};
  const char *pwd[] = {"pwd"};
  int mode;
  backend (MULIB_EXEC_FORK); backend (MULIB_EXEC_SPAWN);
  {
    int fds[2], invalid;
    mulib_process_t child = {0};
    assert (!mulib_process_pipe (fds)); invalid = fds[1];
    assert (!mulib_process_close (fds[0]) && !mulib_process_close (fds[1]));
    command (&c,pwd,1,MULIB_EXEC_FORK); c.io.stdout_fd = invalid;
    assert (mulib_exec_start (&c,&child) == MULIB_EXEC_INVALID && !child.pid);
    assert (mulib_exec_run (&c,&o,&r) == MULIB_EXEC_INVALID);
  }
  {
    /* Staging input must not accidentally reopen originally closed stdout. */
    pid_t pid = fork ();
    int raw;
    assert (pid >= 0);
    if (!pid) {
      int rc;
      close (1);
      command (&c,pwd,1,MULIB_EXEC_FORK); c.merge_stderr = 1;
      o.input = "";
      rc = mulib_exec_run (&c,&o,&r);
      mulib_exec_result_destroy (&r);
      _exit (rc == MULIB_EXEC_SYSTEM ? 0 : 1);
    }
    assert (waitpid (pid,&raw,0) == pid && WIFEXITED (raw) && !WEXITSTATUS (raw));
  }
  command (&c,pwd,1,MULIB_EXEC_AUTO); c.cwd = "/";
  o.capture_stdout = 1;
  assert (!mulib_exec_run (&c,&o,&r) && !strcmp (r.out,"/\n")); mulib_exec_result_destroy (&r);
  c.backend = MULIB_EXEC_SPAWN;
  assert (mulib_exec_run (&c,&o,&r) == MULIB_EXEC_UNSUPPORTED); mulib_exec_result_destroy (&r);
  c.backend = MULIB_EXEC_FORK; c.cwd = "/mulib-exec-missing-cwd";
  assert (mulib_exec_run (&c,&o,&r) == MULIB_EXEC_SYSTEM && r.system_error == ENOENT);
  mulib_exec_result_destroy (&r);
  s = mulib_mune_state_create (NULL); assert (s);
  assert (!mulib_exec_bind (s,NULL));
  for (mode = MULIB_MUNE_TREE; mode <= MULIB_MUNE_VM; ++mode) {
    assert (!mulib_mune_set_mode (s,(mulib_mune_mode_t)mode));
    run_script (s,
      "local p=exec.pipeline(exec.command{argv={'printf','%s','a;b $HOME'},doc=[[literal\ntext]]},"
      "exec.command{argv={'tr','a-z','A-Z'}}); assert(p[1].doc=='literal\\ntext'); "
      "local r=exec.capture(p,{pipefail=true}); assert(r.ok and r.stdout=='A;B $HOME' and #r.stages==2); "
      "r=exec.capture(exec.command{argv={'cat'}},{input='a\\0b'}); assert(r.ok and r.stdout=='a\\0b'); "
      "r=exec.run(exec.command{argv={'sh','-c','exit 9'}}); assert(r.status==0 and r.code==9 and not r.ok); "
      "local rs=exec.sequence({{argv={'sh','-c','exit 2'}},{argv={'true'}}}); assert(#rs==1); "
      "assert(not pcall(function() exec.check({argv={'false'}}) end)); "
      "assert(not pcall(function() exec.run({argv={'a\\0b'}}) end)); "
      "assert(not pcall(function() exec.run({argv={'true'},backend='typo'}) end)); "
      "assert(not pcall(function() exec.run({argv={[1]='true',[3]='ignored'}}) end)); "
      "assert(not pcall(function() exec.run({[1]={argv={'true'}},[3]={argv={'false'}}}) end)); "
      "assert(#exec.docs.run>500)");
  }
  mulib_mune_state_destroy (s);
  {
    int st;
    assert (waitpid (-1,&st,WNOHANG) == -1 && errno == ECHILD);
  }
#endif
  return 0;
}
