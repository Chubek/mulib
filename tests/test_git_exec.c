#include "../examples/exec-common.h"
#define MULIB_IMPL_GIT
#include "mulib-git.h"
#include <assert.h>

static void script (mulib_mune_state_t *s, const char *source)
{
  mulib_mune_result_t *r = NULL;
  mulib_mune_error_t e;
  int rc = mulib_mune_run_source (s,source,strlen (source),&r,&e);
  if (rc) fprintf (stderr,"Git DSL: %s at %zu\n",e.message,e.offset);
  assert (!rc); mulib_mune_result_destroy (r);
}
int main (int argc, char **argv)
{
  mulib_mune_state_t *s = mulib_mune_state_create (NULL);
  mulib_mune_error_t e;
  mulib_exec_options_t o = {0};
  mulib_exec_result_t r = {0};
  const char *version[] = {"--version"};
  const char *directory = argc > 1 ? argv[1] : NULL;
  int rc;
  assert (s);
  /* tools/check.py runs from a temporary directory without arguments; locate
   * source assets relative to its absolute __FILE__, or CMake's supplied path. */
  char path[4096];
  if (!directory) {
    const char *slash = strrchr (__FILE__, '/');
    size_t n = slash ? (size_t)(slash - __FILE__) : 0;
    assert (n + 16 < sizeof (path));
    if (n) memcpy (path,__FILE__,n);
    memcpy (path+n,"/../library/git",16);
    directory = n ? path : "library/git";
  }
  rc = mulib_git_bind (s,directory,&e);
  if (rc) fprintf (stderr,"Git bind %s: %s at %zu\n",directory,e.message,e.offset);
  assert (!rc);
  {
    mulib_mune_value_t v;
    assert (!mulib_mune_make_string (s,directory,strlen (directory),&v));
    assert (!mulib_mune_set_global (s,"git_library",v));
  }
  script (s,
    "local count=0; for name,c in pairs(git.catalog) do "
    "assert(type(c.doc)=='string' and #c.doc>3000); "
    "local p=git.commands[name]({'--example'},{global={'--no-pager'}}); "
    "assert(p.argv[1]=='git' and p.argv[2]=='--no-pager' and p.argv[3]==name and p.argv[4]=='--example'); "
    "assert(p.doc==c.doc); count=count+1 end; assert(count>150); "
    "assert(git.cherry_pick({'HEAD'}).argv[2]=='cherry-pick'); "
    "assert(git.command('future-extension',{'a;b'}).argv[3]=='a;b'); "
    "assert(git.raw({'--version'}).argv[2]=='--version'); "
    "assert(not pcall(function() git.command('-c',{}) end)); "
    "assert(not pcall(function() git.status({'ok',false}) end)); "
    "assert(type(git.help('commit'))=='string')");
  o.capture_stdout = o.capture_stderr = 1;
  rc = mulib_git_run (NULL,NULL,version,1,&o,&r);
  if (rc == MULIB_EXEC_UNSUPPORTED || (rc == MULIB_EXEC_SYSTEM && r.system_error == ENOENT)) {
    puts ("SKIP: Git execution unavailable");
    mulib_exec_result_destroy (&r); mulib_mune_state_destroy (s); return 0;
  }
  assert (!rc && !r.code && strstr (r.out,"git version"));
  mulib_exec_result_destroy (&r);
  /* All mutations are confined to an exclusively created fixture directory. */
#if defined(__unix__) || defined(__APPLE__)
  {
    char fixture[] = "mulib-git-exec-XXXXXX";
    mulib_mune_value_t v;
    extern char *mkdtemp (char *);
    assert (mkdtemp (fixture));
    assert (!mulib_mune_make_string (s,fixture,strlen (fixture),&v));
    assert (!mulib_mune_set_global (s,"fixture",v));
    script (s,
      "local opts={cwd=fixture,global={'--no-pager','-c','user.name=Exec Test','-c','user.email=exec@example.invalid','-c','commit.gpgsign=false'}}; "
      "assert(git.capture('init',{'--initial-branch=main'},opts).ok); "
      "local f=assert(io.open(fixture..'/a ; $name.txt','wb')); f:write('first\\n'); f:close(); "
      "assert(exec.check(git.add({'--','a ; $name.txt'},opts)).ok); "
      "assert(git.capture('commit',{'-m','first literal ; $HOME'},opts).ok); "
      "local st=exec.capture(git.status({'--porcelain=v1','-z'},opts)); assert(st.ok and st.stdout==''); "
      "assert(git.capture('branch',{'topic'},opts).ok); "
      "assert(git.capture('switch',{'topic'},opts).ok); "
      "f=assert(io.open(fixture..'/a ; $name.txt','ab')); f:write('second\\n'); f:close(); "
      "local diff=git.capture('diff',{'--exit-code'},opts); assert(diff.status==0 and diff.code==1); "
      "assert(git.capture('commit',{'-am','second'},opts).ok); "
      "assert(git.capture('switch',{'main'},opts).ok); "
      "assert(git.capture('merge',{'--ff-only','topic'},opts).ok); "
      "assert(git.capture('tag',{'v1'},opts).ok); "
      "local tree=git.capture('ls-tree',{'-rz','HEAD'},opts); assert(tree.ok and string.find(tree.stdout,'a ; $name.txt',1,true)); "
      "local hash=git.capture('hash-object',{'-w','--stdin'},opts,{input='a\\0b'}); assert(hash.ok); "
      "local oid=string.gsub(hash.stdout,'\\n',''); "
      "local blob=git.capture('cat-file',{'blob',oid},opts); assert(blob.ok and blob.stdout=='a\\0b'); "
      "local p=exec.pipeline(git.log({'--format=%s'},opts),exec.command{argv={'sort'}}); "
      "local lines=exec.capture(p,{pipefail=true}); assert(lines.ok and lines.stdout=='first literal ; $HOME\\nsecond\\n'); "
      "local workflows=dofile(git_library..'/workflows.lua'); "
      "assert(workflows.status(fixture).doc==workflows.docs.status); "
      "local sorted=exec.capture(workflows.subjects(fixture,2),{pipefail=true}); "
      "assert(sorted.ok and sorted.stdout==lines.stdout); "
      "local alias=git.capture('my-status',{'--porcelain=v1'}, {cwd=fixture,global={'-c','alias.my-status=status'}}); "
      "assert(alias.ok and alias.stdout==''); "
      "assert(git.capture('clone',{'--bare',fixture,fixture..'/remote.git'}).ok); "
      "assert(git.capture('remote',{'add','origin','./remote.git'},opts).ok); "
      "assert(git.capture('fetch',{'origin'},opts).ok); "
      "assert(git.capture('push',{'origin','main'},opts).ok); "
      "assert(exec.check(exec.command{argv={'rm','-rf','--',fixture}}).ok)");
  }
#endif
  mulib_mune_state_destroy (s);
  return 0;
}
