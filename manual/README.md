# MuLIB manuals

Start with the [library index](index.md) to find a header, its manual, and a
runnable example. The [module guide](modules.md) groups libraries by task;
[dependency strata](strata.md) shows how they compose.

## Using a library

1. Read its supported profile and ownership/error contract.
2. Open the linked example for a complete implementation-switch setup. Some
   examples share a setup header; follow that include too.
3. Compile implementations as C11, defining switches in exactly one translation
   unit. Include declarations without switches elsewhere. See
   [integration](integration.md) for single-file, multi-file, C++, and CMake builds.
4. Use the linked header for exact types, constants, signatures, and configuration
   macros. A module name alone does not imply full format or protocol support.

## Stack guides

| Area | Guide |
| --- | --- |
| Composition and API families | [Modules](modules.md) |
| Storage and server policies | [Storage/server](storage-server.md) |
| Storage, documents and execution layers | [Extension layers](extension-layers.md) |
| ELF, COFF, PE and Mach-O | [Binary formats](binary-formats.md) |
| Container images and runtimes | [Container stack](container-stack.md) |
| SQLite development status | [Implementation plan](sqlite-implementation-plan.md) |
| Allocation and malformed-input checks | [Header hardening](hardening.md) |
| Unicode and native text backends | [Text verification](text-hardening.md) |

## Reading contracts

There is **no universal MuLIB return-code convention**. Some APIs use `1` for
success; others use `0`, negative error codes, counts, or pointers. Follow the
individual function's contract, including whether failure changes output.

“Owned” storage must be released with the documented destructor. “Borrowed”
storage belongs to another object and must stay within that object's documented
lifetime. Copying a public structure containing owned pointers does not duplicate
its allocations. Byte containers copy bytes, not the objects referenced by them.

Lengths, capacities, terminators, alignment, callback lifetimes, and thread-safety
rules are API-specific. A `const` parameter alone does not establish concurrent
access safety. Platform/backend requirements appear in the individual manuals.

## Maintaining these manuals

Keep behavioral prose and examples synchronized with implementation changes.
The [authoring guide](authoring.md) describes the expected contract coverage.
Source links, include dependencies, example commands, and marked header excerpts
are maintained by a dependency-free tool:

```sh
python3 tools/manuals.py --write
python3 tools/manuals.py --check
```

The checker verifies inventory, generated sections, local Markdown links, and
header excerpts. It does not infer behavioral guarantees from C signatures.
Use `--compile-examples` to compile all linked examples with strict warnings;
use the focused suites and `tools/check.py` for runtime validation.
