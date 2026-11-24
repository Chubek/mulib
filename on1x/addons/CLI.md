# On1x syntax, compiler flags, and project manifests

Run `on1xc --help` for the options enabled in the installed compiler.
On POSIX, `on1xc aux --help` lists the bundled project driver's commands.
Use `on1xc aux COMMAND --help` for that command's options. `aux` passes the
arguments, working directory, and result through unchanged; it does not
look up named commands in an On1xfile as `on1xc exec` does. You can also
invoke the project driver directly with
`python3 on1x/tools/on1x-project.py COMMAND`, or use
`python3 on1x/tools/on1x-build.py --help` for single-file builds.
When a command needs a compiler, `aux` defaults to the invoked `on1xc`;
an explicit `--on1xc PATH` takes precedence.

## Source syntax

```on1x
import kernel[dynlib] mulib.clamp as Clamp;
```

The bracketed modifier selects shared-library linkage. The explicitly unsafe
form starts with `unsafe import`. Plain `import kernel mulib.clamp as Clamp;`
selects static linkage. `on1xc --kernel-imports INPUT` reports the requests
without running PPX, while `on1x-build.py --inspect-kernels` inspects
declarations. Callable imports still need the applicable
[KernelBind profile](../docs/kernel-bind.md). With `--kernel-bind` or
`--kernel-review-candidate`, `--kernel-dynlib-compiler cephyr|host`
selects the implementation compiler. Cephyr is the default.

`@InlineC11("unsafe_i64", "(a0 & a1) * 2", 37, 15)` uses the bundled
[InlineC11 processor](../stdppx/README.md#trusted-inline-c11). Select it with
`on1x-build.py --trust-inline-c11` in a direct C11 executable build. This flag
authorizes C from the entire source graph, including macro and PPX expansions.

## Compiler and build flags

| Task | Command or flags |
| --- | --- |
| VM bytecode and execution | `on1xc --emit-vm INPUT -o app.o1vm`, `on1xc --run-vm INPUT`, or `on1x-vm --verify app.o1vm` to verify a tape. Both compiler modes accept `--ir-input` for serialized MIR. `--vm-source-prefix FROM TO` remaps absolute source paths in emitted bytecode. See the [VM manual](../backends/mir-vm/README.md). |
| Optimization policy | `--list-optimizations` prints the linked compiler's JSON inventory. Set `--opt-profile default\|debug\|release` and repeat `--enable-opt NAME` or `--disable-opt NAME`. `--all-optimizations` and `--no-optimizations` select the `all` baseline in `on1xc`. See [optimization profiles](../docs/optimization-profiles.md) for stage availability. |
| Optimization reports | `--dump-processor INPUT` shows authored rule hits. `--dump-memgraph INPUT` shows direct C11 bounds decisions with LLSynth-built MEMGraph. `--dump-passes INPUT` shows the MIR pipeline and accepts `--ir-input`. `--optimize-ir` enables the legacy MIR baseline under the default preset. `--opt-work UNITS` and `--opt-rewrites COUNT` bound enabled MIR passes. |
| Independent checks | `--check-types INPUT` and `--verify-types` require Nerotypical. `--check-memory INPUT` and `--memory-check` require Chisel. Memory checks can take `--admitted-extern FILE` for reviewed scalar extern evidence. See [type verification](../docs/type-safety.md) and [resource-usage checking](../docs/memory-safety.md). |
| Archon native emission | `on1xc --sdk-emit mir-archon --sdk-target TRIPLE INPUT -o OUTPUT.s` emits assembly with the matching SDK. `--native-pic` and `--native-no-entry` apply to target-aware emission. `on1x-native-build.py` builds the executable. See the [Archon driver](../backends/mir-archon/DRIVER.md). |
| Cephyr object emission | `on1xc --emit-cephyr-object INPUT -o OUTPUT.o` requires a Cephyr-enabled native AMD64 Linux build. Optional `--cephyr-target x86_64-linux-gnu` selects the supported target. |
| Cephyr policy | `--cephyr-profile scalar`, `--cephyr-qml-pass NAME`, `--cephyr-trusted-qml-file FILE`, `--cephyr-qml-fuel N`, `--cephyr-qml-param PHASE.fuel=N`, and on/off switches `--cephyr-blackstone`, `--cephyr-factir`, `--cephyr-sandslab`. See [Cephyr optimization](../docs/cephyr-optimization.md). |
| Cephyr reports | `--cephyr-sarif FILE` writes compile diagnostics. With `--cephyr-factir on`, `--cephyr-factir-facts NEWFILE` and `--cephyr-factir-smtlib NEWFILE` request create-only evidence files. |
| Single-file build backend | `on1x-build.py --backend c11\|mir-c11\|mir-vm\|mir-archon INPUT -o OUTPUT`. `--via-mir` selects `mir-c11`. For `mir-vm`, the output is bytecode. For `mir-archon`, `--emit-only` outputs assembly. |

## On1xfile build policy

### Initialize a project

`on1xc aux init [DIRECTORY]` creates a root `On1xfile` for a library named
after the directory. The directory defaults to the current directory. The
command does not generate source code, guess a `main`, or scan for files
unless you add `--recursive`.

With `--recursive`, it writes an `On1xfile.m` in each directory containing
regular `.on1x` files and an `On1xfile.i` for `.on1xi` files with a matching
same-directory implementation. Hidden entries, symlinks, `build` and `target`
subdirectories, declared dependency stores, and nested projects are skipped.

Add `--with-gitignore` to write the bundled On1x and MuLIB `.gitignore` at
the project root, alongside `On1xfile`. It covers build outputs, caches,
local configuration, and common tooling artifacts. Without the flag, init
leaves `.gitignore` alone. With the flag, any existing
`.gitignore` (including a symlink) is left untouched and init fails before
creating any files. The same no-overwrite rule applies to all manifests.

Use `--template NAME` to copy a plain root manifest named `NAME.On1xfile`.
`--templates-path PATH_LIST` replaces `$ON1XFILE_TEMPLATES_PATH`; on POSIX,
separate directories with `:`. The first matching file wins. Empty path
entries, symlinks, and QMR templates are rejected. Setting the environment
variable alone does not select a template. Templates are validated and
copied as text, not executed or interpolated. When recursing, the template's
`directory` controls where scanning starts and its `target` declarations
select the implementations to mark `source` rather than `library`.

### Manifest directives

The root `On1xfile` declares targets and compiler policy. `On1xfile.m`
declares directory roles (`source`, `library`, and restricted `resource`),
and `On1xfile.i` declares `interface` files. Each plain manifest starts with
`on1xfile 1` and uses JSON-escaped double quotes and `#` comments.

```text
on1xfile 1
kind executable
name "hello"
directory "src"
target "hello" "src/main.on1x"
backend c11
backend "debug" mir-vm
optimization-profile default
optimization "debug" "integer_add_zero_right" on
scrutiny standard
scrutiny-work 8192
```

- `backend NAME` sets the project default. `backend "BUILD_PROFILE" NAME`
  overrides it for that build profile, and CLI `--backend NAME` takes
  precedence over both. Supported names are `c11`, `mir-c11`, `mir-vm`,
  and `mir-archon`. The VM project output includes bytecode, a launcher,
  and a runner.
- `optimization-profile default|debug|release` sets the compiler preset.
  `optimization "NAME" on|off` applies across the project, while
  `optimization "BUILD_PROFILE" "NAME" on|off` applies in one build
  profile. CLI `--opt-profile`, `--enable-opt`, and `--disable-opt`
  override manifest settings. The checked HIR passes `firestone-hir` and
  `novostone-hir` remain opt-in on builds that include them.
  `--profile` selects the **build profile**
  in the project driver and aliases `--opt-profile` in the single-file
  compiler and build helper.
- `scrutiny-work N` sets an optional static-analysis budget
  (`1` through `10000000`). CLI `--scrutiny-work N` overrides it for a
  project build. `--scrutiny LEVEL` overrides the analysis level. These policy
  declarations belong to the root project.
- `on1xc aux plan PROJECT` shows resolved settings. Use
  `on1xc aux backends` to probe installed backends and
  `on1xc aux format PATH --check` to check a plain manifest. Use
  `on1xc aux lock PROJECT` and `on1xc aux fetch DEPENDENCY PROJECT` to
  manage locked dependencies, then `on1xc aux build PROJECT --frozen`
  to build without acquiring dependencies.
  Backend or optimization declarations use the `on1x.lock/v2` lock format.

See the [On1xfile reference](../stdlib/on1xfile/README.md) for file roles,
QMR templates, scoped policy, backend requirements, and lockfile behavior.
