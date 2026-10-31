# librl: MuLIB Readline language profiles

Each language has three version-1 JSON files:

* `.mlsyn`: ordered lexical highlighting rules.
* `.mlac`: completion texts and descriptions.
* `.mldir`: `:<language>-example` starter expansion and `:<language>-doc` action.

Languages: **C, C++, Python, JavaScript, TypeScript, Lua, Rust, Go, Java, C#,
Ruby, PHP, Bash, SQL, JSON**. Filename stems are lowercase; C++ is `cpp`, C# is
`csharp`. There are 45 profiles total.

```c
mulib_readline_t *editor = mulib_readline_create(NULL);
int rc = mulib_readline_load(editor, "libraries/librl/lua.mlsyn", NULL);
if (!rc) rc = mulib_readline_load(editor, "libraries/librl/lua.mlac", NULL);
if (!rc) rc = mulib_readline_load(editor, "libraries/librl/lua.mldir", NULL);
/* Use editor; dispatch actions/expanded text in your application. */
mulib_readline_destroy(editor);
```

Readline copies definitions and never executes profiles. Starter expansions use
raw `$*` argument substitution. Documentation actions are symbolic host requests,
not shell commands. Profiles are lexical editing aids, without cross-line parser
state or complete grammar coverage. Choose/create an editor for the language you
want and add application-specific rules and completions through the API.

See the [Readline manual](../../manual/mulib-readline.md) for schemas and limits.
Modify `tools/generate_librl.py` and regenerate with
`python3 tools/generate_librl.py`; `--check` verifies reproducibility.
