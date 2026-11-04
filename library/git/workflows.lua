-- Example reusable plans. Load after mulib_git_bind (or after init.lua).
local workflows = {}
workflows.docs = {}
workflows.docs.status = [[
status(directory) returns a Git status plan with porcelain-v2 NUL-delimited
records. Execute with exec.capture and inspect result.status/result.code before
parsing result.stdout. The plan supplies --no-pager globally and uses directory
as child cwd. It does not change the host cwd or execute during construction.
]]
function workflows.status(directory)
  return git.status({"--porcelain=v2","-z"},
                    {cwd=directory,global={"--no-pager"},doc=workflows.docs.status})
end
workflows.docs.subjects = [[
subjects(directory, count) returns a two-stage pipeline: Git emits up to count
commit subjects and sort sorts their lines. All stages run concurrently. Subject
sorting uses the inherited sort locale; set an explicit environment on both
stages for locale-independent ordering. Use exec.capture(plan,{pipefail=true})
so a Git failure is not hidden by sort succeeding on empty input. count must be
a positive integer. The pipeline reads history and does not update any refs.
]]
function workflows.subjects(directory,count)
  assert(type(count)=="number" and count>0 and count%1==0,"positive count required")
  return exec.pipeline(
    git.log({"-n",tostring(count),"--format=%s"},{cwd=directory,global={"--no-pager"}}),
    exec.command{argv={"sort"},doc=[[Sort newline-delimited commit subjects using
the inherited locale. Receives stdin from Git; writes sorted subjects to stdout.
Exit failure is preserved by pipeline pipefail. This stage modifies no files.]]})
end
return workflows
