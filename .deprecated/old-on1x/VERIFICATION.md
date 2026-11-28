# On1x verification index

For the 2026-11-26 qualification, use the [release checklist and evidence](docs/release-qualification.md)
and [completion ledger](COMPLETION_MATRIX.md). Dated runs from other revisions
stay separate from this task's results.

For the intrinsic operator work, see the separate
[Clang qualification and reproduction commands](docs/operator-qualification.md).

## Earlier evidence

- [Historical verification through 2026-11-24](https://github.com/Chubek/mulib/blob/bfb3bbca3ab967681fdb821ea45298d1b523164b/on1x/VERIFICATION.md):
  feature-specific build flags, revisions, commands, results and test limits.
  This is the parent of the cleanup that removed the file. For offline use
  in a clone with that history:

  ```sh
  git show bfb3bbca3ab967681fdb821ea45298d1b523164b:on1x/VERIFICATION.md
  git show faa82816461b961614b16cf81df8a5d40e2ff155:on1x/COMPLETION_REPORT.md
  ```

- [Earlier consolidation build and benchmark smoke](docs/consolidation-status.md).
- [Scalar and tuple-loan backend route audit](docs/backend-phase-audit.md).
- [Load-time clamp qualification](docs/kernel-load-qualification.md) and
  [Cephyr adapter ABI evidence](docs/cephyr-ffi-module-verification.md).
- The removed [backend plan](https://github.com/Chubek/mulib/blob/bfb3bbca3ab967681fdb821ea45298d1b523164b/on1x/BACKEND.md)
  and [KernelBind acceptance specification](https://github.com/Chubek/mulib/blob/bfb3bbca3ab967681fdb821ea45298d1b523164b/on1x/KERNEL_DIRECT_USAGE.md)
  are historical design records, not current source or execution evidence.
  Use the [backend phase audit](docs/backend-phase-audit.md) and
  [KernelBind guide](docs/kernel-bind.md) for actual route and scope.
- [SDK and relocation verification](sdk/VERIFICATION.md),
  [standard-library](stdlib/VERIFICATION.md),
  [standard PPX](stdppx/VERIFICATION.md), and [toolchain](toolchains/VERIFICATION.md)
  verification are scoped subsystem records.

## Sandbox kernel discovery and archive

[Historical sandbox discovery results](https://github.com/Chubek/mulib/blob/bfb3bbca3ab967681fdb821ea45298d1b523164b/on1x/VERIFICATION.md#sandbox-kernel-discovery-and-archive).
This is archive and declaration evidence, not callable sandbox admission.

## GrammarEmbed — native liblr lexer/parser emission

[Historical GrammarEmbed commands and results](https://github.com/Chubek/mulib/blob/bfb3bbca3ab967681fdb821ea45298d1b523164b/on1x/VERIFICATION.md#grammarembed--native-liblr-lexerparser-emission).
See also the [supported grammar profile](stdppx/grammarembed/README.md).

## LuaEmbed native PPX and embedded `llembed`

[Historical LuaEmbed commands and results](https://github.com/Chubek/mulib/blob/bfb3bbca3ab967681fdb821ea45298d1b523164b/on1x/VERIFICATION.md#luaembed-native-ppx-and-embedded-llembed).
See also the [LuaEmbed profile](stdppx/luaembed/README.md).
