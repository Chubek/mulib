# git — command execution, Mune workflows, object IDs and packet lines

Define `MULIB_IMPL_GIT` plus every switch listed in
[`mulib-exec.md`](mulib-exec.md#build-and-implementation-placement) in the
implementation translation unit. Direct dependency: `mulib-exec.h`.
See [`examples/modules/git.c`](../examples/modules/git.c).

## Git command execution

`mulib_git_run(git_file, cwd, argv, argc, options, result)` executes any Git CLI
invocation through EXEC. The counted argv **excludes** argv[0]; the function
prepends `git_file`, defaulting to `"git"`. Pass global options before the
subcommand exactly as on the CLI. cwd is child-only and may be NULL. All command
names/options are forwarded, including aliases, plumbing, optional components,
installed `git-*` extensions and future commands. Git's installed version decides
which operations exist; the library does not emulate unavailable commands.

Initialize options/result to zero and destroy the owned result after every run.
API return values describe execution failures, while result.code describes Git's
exit status. For example, `diff --exit-code` or `grep` can return 1 as a normal
predicate outcome. Capture preserves binary bytes and NUL-delimited records.
Use the full EXEC command specification when explicit environment, descriptor
mapping, redirection or backend selection is needed.

```c
const char *args[] = {"--no-pager", "status", "--porcelain=v2", "-z"};
mulib_exec_options_t options = {0};
mulib_exec_result_t result = {0};
options.capture_stdout = options.capture_stderr = 1;
options.timeout_ms = 5000;
int status = mulib_git_run(NULL, "/path/to/repo", args, 4, &options, &result);
/* Check status and result.code; parse result.out using result.out_size. */
mulib_exec_result_destroy(&result);
```

## Mune Git DSL and command docstrings

Call `mulib_git_bind(state, "/path/to/library/git", &error)` to install EXEC and
evaluate `library/git/init.lua`, creating global `git`. The directory is explicit;
the loader does not change process cwd or require package.path configuration.
The source file must be deployed alongside the application. It uses public Mune
APIs and normal Lua code; it can also be loaded after `mulib_exec_bind` using
`dofile` or an appropriately configured module search path.

```lua
local repository = {cwd="/path/to/repo", global={"--no-pager"}}
local plan = git.status({"--porcelain=v2", "-z"}, repository)
assert(type(plan.doc) == "string")
local result = exec.capture(plan, {timeout_ms=5000})
if not result.ok then error(result.stderr) end

local pipeline = exec.pipeline(
  git.log({"-n", "20", "--format=%s"}, repository),
  exec.command{argv={"sort"}, doc=[[Sort incoming subjects using the host locale.]]}
)
local subjects = exec.capture(pipeline, {pipefail=true})
```

* `git.command(name, arguments, options)` constructs an inert EXEC command.
* `git.raw(arguments, options)` accepts the entire CLI after the executable,
  including global-only operations such as `--version`.
* `git.commands[name](arguments, options)` selects a catalogued constructor.
  Top-level aliases replace `-` with `_`: `git.cherry_pick`, `git.rev_parse`, etc.
* `git.run(name, arguments, command_options, execution_options)` executes a command.
* `git.capture(...)` executes with stdout and stderr capture.
* `git.help(name)` returns a docstring without starting Git. The actual Git help
  constructor is `git.commands.help(...)` to avoid colliding with this helper.

Command options include `executable`, `cwd`, `global` (global Git arguments),
`env`, `stdin`, `stdout`, `stderr`, `append_stdout`, `append_stderr`,
`merge_stderr`, `backend` and `doc`. Execution options belong to run/capture and
include binary `input`, `timeout_ms`, `max_output` and `pipefail`. See EXEC for
complete lifetime, result, deadline and platform semantics. No shell quoting is
needed: `{"--", "file ; $name.txt"}` contains one literal filename argument.
Some Git operations (shell aliases, submodule foreach, filter-branch filters)
deliberately interpret their own embedded languages; the catalogue calls this out.

`library/git/init.lua` contains more than 150 named command entries, covering
porcelain, object/index/ref/pack plumbing, transport servers/helpers, maintenance,
optional interoperability components and internal compatibility entry points.
Every registered command has a runtime **string** docstring containing its
purpose, synopsis, command-specific effects/output/failure interpretation, and
the full shared argument/environment/result/composition contract. Inspect these
through `git.docs[name]`, `git.catalog[name].doc`, `git.help(name)` or `plan.doc`.
`git.catalog` also exposes purpose, synopsis and command-specific details.
Caller-supplied quoted or multiline `doc` strings override a plan's documentation.

The catalogue is descriptive, never an allowlist. Use `git.command` or `git.raw`
for any operation not named in it, including extensions and new Git releases;
unknown commands receive the generic command docstring. Version-dependent and
internal helpers are labelled as such. Command-specific docs summarize semantics;
the installed Git manual remains authoritative for exhaustive version-specific
flags. `git.docs.dsl_help` documents the docstring lookup separately from Git help.

`library/git/workflows.lua` supplies documented reusable status/pipeline examples.
All workflow construction is inert; exec.run/capture/check/sequence explicitly
start processes. Git remains responsible for configuration, identity, signing,
hooks, editors, credentials, transports and repository locking. Use explicit
configuration/environment for noninteractive workflows. Lua or execution errors
do not roll back earlier Git side effects.

## Object IDs and packet framing

`mulib_git_sha1` hashes arbitrary bytes, including empty input; `object_id`
hashes Git's `type SP decimal-length NUL content` representation. Supported
types are `blob`, `tree`, `commit`, and `tag`. Object content is opaque: hashing
a tree does not validate its entries. IDs are 20-byte values. `oid_format`
writes 41 bytes including NUL; `oid_parse` requires exactly 40 hexadecimal
characters and accepts either case. SHA-256 repositories are outside these
in-process SHA-1 helpers; command execution supports Git's available object formats.
SHA-1 is provided for Git compatibility, not authentication.

`pkt_encode` and `pkt_decode` implement four-hex-digit pkt-line lengths,
including flush, delimiter, and response-end controls. Public kind values are
0=data, 1=flush, 2=delimiter, 3=response-end. Maximum data length is 65516;
`0003` and lengths above 65520 are invalid. Empty data encodes as `0004`.
Decode returns 1 complete, 0 incomplete, -1 invalid; retain bytes on incomplete.
Payload pointers borrow the input. `used` is reset on failure. Encoders require
disjoint input/output storage. These hash/framing helpers perform no allocations
or subprocess execution. Repository/index/worktree operations use the separate
execution API and the installed Git executable.

Tests: `tests/test_vcs_formats.c`, including independent SHA-1 and Git vectors.
`tests/test_git_exec.c` checks every catalogue constructor/docstring and runs a
local repository workflow with literal filenames, binary plumbing, commits,
branches, merge, tags, aliases, pipelines, clone, fetch and push.
