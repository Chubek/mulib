# Git execution DSL

Load `init.lua` with `mulib_git_bind(state, directory, &error)`. This installs
global `git` after binding the native `exec` DSL. Source files are ordinary Mune
Lua and must be shipped with the embedding application.

```lua
local options = {cwd="my-repository", global={"--no-pager"}}
local plan = git.diff({"--stat", "HEAD"}, options)
print(plan.doc) -- Full runtime documentation, without executing Git.
local result = exec.capture(plan, {timeout_ms=5000})
assert(result.status == 0, result.stderr)
```

`init.lua` is the command catalogue and DSL implementation. Each entry records
its own synopsis and behavior; registration adds the complete shared execution
contract to the command's string docstring. `git.docs`, `git.catalog`,
`git.help(name)` and constructed plans expose these strings at runtime. Multiline
caller docstrings use normal Lua long brackets: `doc=[[...]]`.

`workflows.lua` returns documented reusable plan builders. Load it after init;
its status plan emits porcelain-v2 NUL records, and its history pipeline composes
Git with a separate sort process. Loading/building plans does not execute them.

All Git operations are reachable through `git.command(name, args, options)` or
`git.raw(args, options)`, even when no catalogue entry exists. Named constructors
are in `git.commands[name]` and, where no DSL name collides, `git.name_with_underscores`.
Use `git.commands.help` to execute Git help; `git.help` retrieves a docstring.

See [the Git manual](../../manual/mulib-git.md) and
[the execution manual](../../manual/mulib-exec.md) for build switches, complete
options, result/error contracts, platform support, ownership and examples.
