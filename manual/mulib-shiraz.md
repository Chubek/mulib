# Shiraz: Windows PE/COFF execution and ELF translation

[Manuals](README.md) · [Library index](index.md)

`include/mulib-shiraz.h` provides an owned AMD64 image loader, a resumable
interpreter and helper-based C ahead-of-time translation. The separately compiled
[`library/shiraz`](../library/shiraz/) sources provide Windows imports, handles,
files, heap services and a persistent registry. Both execution modes use the same
runtime libraries.

The initial profile runs **freestanding x86-64 Windows programs using the
instructions and APIs documented below**. It is not a complete Wine replacement:
ordinary CRT startup, GUI applications and arbitrary Windows software generally
require additional instructions and Windows services.

## Build and use

```sh
make -C library/shiraz -j4
library/shiraz/build/shiraz run program.exe
library/shiraz/build/shiraz run function.obj --entry main
library/shiraz/build/shiraz compile program.exe program.elf
./program.elf
library/shiraz/build/shiraz compile program.exe program-static --static
library/shiraz/build/shiraz compile program.exe program-shared --shared-runtime
library/shiraz/build/shiraz emit-c program.exe program.c
cc -std=c11 -O2 -Iinclude program.c \
  library/shiraz/build/libshiraz-win32.a library/shiraz/build/libshiraz.a -o program
```

Default ELF compilation statically links Shiraz's archives with the system C
runtime linked normally. `--static` also statically links libc and requires its
development archives. Shared-runtime output records the configured library
directory as its runtime search path. `SHIRAZ_CC` is one compiler executable,
not a shell command. `SHIRAZ_INCLUDE_DIR` and `SHIRAZ_LIBRARY_DIR` override the
build-time locations. Compiler invocation uses `mulib-process.h` argv spawning,
checks exit status and ELF magic, and never runs the guest during compilation.

`--base ADDRESS` selects a relocated guest base. `run --steps COUNT` selects the
CLI execution budget (default 100 million). Errors exit 125. Windows exit codes
are 32-bit in the API; Linux process status exposes the low byte. Root CMake also
builds libraries/tools on Linux; disable with `-DMULIB_BUILD_SHIRAZ=OFF`.

## Header integration and ownership

For header-only CPU/loader use, define all switches before including dependencies:

```c
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_COFF
#define MULIB_IMPL_PE
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_SHIRAZ
#include "mulib-shiraz.h"
```

The [runnable example](../examples/modules/shiraz.c) also enables PE-builder
dependencies. Build with `cc -std=c11 -Iinclude examples/modules/shiraz.c -o demo`.
Applications linking `libshiraz.a` include declarations without switches.
The runtime/registry declarations are implemented separately by
`libshiraz-win32.a`; static archive order is Win32 followed by core.

```c
mulib_shiraz_t *vm = NULL;
mulib_shiraz_error_t error = {0};
int rc = mulib_shiraz_load_file(&vm, "program.exe", NULL, &error);
if (rc == 0) {
    mulib_shiraz_runtime_t *runtime = mulib_shiraz_runtime_create("user.hive");
    if (runtime && mulib_shiraz_runtime_attach(runtime, vm) == 0) {
        do { rc = mulib_shiraz_run(vm, 10000); } while (rc == MULIB_SHIRAZ_AGAIN);
        if (mulib_shiraz_runtime_save(runtime) != 0) rc = MULIB_SHIRAZ_EIO;
    }
    mulib_shiraz_destroy(vm);
    mulib_shiraz_runtime_destroy(runtime);
}
```

`load` copies input into owned file/image storage; input may be freed immediately.
An output VM pointer is unchanged on failure; destroy before reusing it. Borrowed
CPU/error/import views expire at destruction. VM and runtime are single-threaded.
An attached runtime must outlive its VMs; neither owns the other. Destruction is
NULL-safe. Runtime destruction discards unsaved registry changes.

Zero options select a 64 MiB image/input limit, 1 MiB stack and 16 MiB heap.
Limits: 96 mapped image regions, 4096 distinct imports, one million COFF symbols,
127-byte DLL and 255-byte import names. Guest addresses never represent host
pointers. Memory access checks section permissions, stack bounds and live heap
allocations. Executable bytes cannot be written. Heap allocations are 16-byte
aligned, zeroed, releasable and coalesced for reuse. The entry stack has Microsoft
x64 alignment, shadow space and a return sentinel; entry RET exits with EAX.

`bind` installs a borrowed synchronous host callback. RCX/RDX/R8/R9 and stack+40
carry arguments; `argument`, `read` and `write` provide checked access. Put the
return value in RAX; the dispatcher performs the guest return. `exit` halts the
guest without terminating its embedding process. `run` returns `DONE`, resumable
`AGAIN`, or a sticky negative fault. Runtime attachment resolves all imports up
front and identifies unresolved DLL/function names. The core is portable C11;
the supplied OS services/compiler driver target Linux/POSIX.

## Image and instruction coverage

* **PE:** AMD64 PE32+ console/GUI-subsystem executable images, headers, sections,
  BSS, DIR64 relocations and ABSOLUTE padding, named/ordinal import-table entries.
  The default runtime resolves named imports only. DLL images, PE32, TLS, delay
  imports, CLR and load-config initialization are rejected. Unwind, resource,
  export and debug metadata may be present as data; no SEH/resource/DLL API is
  implied. A GUI subsystem header does not provide GUI services.
* **COFF:** one AMD64 object, entry symbol `main` by default, primary/auxiliary
  symbol records, sections/BSS, ADDR64, ADDR32, ADDR32NB and REL32 through REL32_5.
  `__imp_Name` references and direct external calls receive runtime import thunks.
  Single-object COMDAT sections are retained. Archives, multi-object selection,
  weak/common symbols and `.drectve` linker directives are unsupported.
* **AMD64:** REX/operand-size prefixes, ModRM/SIB, RIP-relative and indexed memory;
  byte/word/dword/qword MOV, immediate MOV, LEA, MOVZX/MOVSX/MOVSXD; register,
  memory and immediate ADD/SUB/OR/AND/XOR/CMP; TEST; INC/DEC; two-operand IMUL;
  64-bit PUSH/POP/LEAVE/CALL/JMP/RET; short/near Jcc and NOP (`90`, `0F 1F /0`).
  Exact accepted forms are those decoded in the header, not every encoding of
  these mnemonics. 32-bit writes zero-extend; CF/PF/ZF/SF/OF support all Jcc
  conditions, and INC/DEC preserve carry.

High-byte registers, address-size/segment overrides, locked/string operations,
SIMD/x87, shifts/division, system instructions and other unimplemented forms
return `EUNSUPPORTED` at their guest PC. There is no TEB/PEB, threading, exception
unwinding or floating-point/variadic Windows ABI. Windows instructions are never
executed directly on the host.

## AOT contract

`emit_c` traverses reachable direct control flow and emits predecoded records
with a C switch over guest PCs. Cases invoke the interpreter's checked execution
helpers. A native C compiler generates the ELF; **generated programs do not
decode instructions or fall back to interpretation**. Embedded PE/COFF bytes are
used for data layout, relocation and imports, so the original file is unnecessary.

This is helper-based AOT, not optimized native instruction selection. Returns and
indirect branches may reach translated PCs or import thunks; unknown computed
targets fail explicitly. Functions reachable only through data-held pointers are
not discovered automatically. Default translation limit: 100000 instructions.
Output replacement is atomic on success; the VM must still have its initial CPU
and memory state (guest writes and heap allocations invalidate emission). Generated
programs run until exit/fault; the helpers allow embedders to add
quotas. Self-modifying executable code is unsupported in both modes.

## Runtime and Registry

The [runtime catalogue](../library/shiraz/README.md) lists all imports and their
restrictions. Registry storage uses MuLIB's CRC-protected KVDB with atomic POSIX
snapshot replacement, not Microsoft's hive format. Names use ASCII case folding;
the C registry API stores REG_SZ as UTF-16LE, REG_DWORD as four LE bytes.

```sh
export SHIRAZ_REGISTRY="$PWD/user.hive"
library/shiraz/build/shiraz-reg "$SHIRAZ_REGISTRY" set \
  'HKCU\Software\Shiraz' Answer dword 42
library/shiraz/build/shiraz-reg "$SHIRAZ_REGISTRY" get \
  'HKCU\Software\Shiraz' Answer
```

The interpreted CLI and generated ELF share `SHIRAZ_REGISTRY`; absent it they
use memory-only state. Saves are explicit in C and automatic on CLI/generated
program cleanup. Concurrent writers follow KVDB's last-writer semantics. File
APIs operate on the host filesystem with host process permissions; Shiraz is an
execution environment, not a process sandbox.

## MuLL

The generated bridge exposes the complete API and compiles the runtime sources
for **both C++ and D MuLL hosts**. `MULL_SHIRAZ_SOURCE_DIR` selects their location
when headers and runtime sources are installed in a different layout.

```mull
mulib S <- G:MulibWrap("Shiraz")
val slot <- S:New("mulib_shiraz_t *");
val error <- S:New("mulib_shiraz_error_t");
G:Assert(S:LoadFile(slot, "program.exe", nil, error) == 0);
val vm <- slot:Get();
val runtime <- S:RuntimeCreate("user.hive");
G:Assert(S:RuntimeAttach(runtime, vm) == 0);
var result <- S:Run(vm, 10000);
while result == S:MULIB_SHIRAZ_AGAIN { result <- S:Run(vm, 10000); }
G:Assert(result == S:MULIB_SHIRAZ_DONE);
G:Assert(S:RuntimeSave(runtime) == 0);
S:Destroy(vm);
S:RuntimeDestroy(runtime);
```

Native ownership still applies: explicitly release VMs, runtimes and generated
string buffers. The shared MuLL fixture tests loading, execution and registry
operations in both hosts.

## Verification

`tests/test_shiraz.c` exercises execution/flags, budgets, memory ownership, heap
reuse, malformed/truncated images, atomic AOT output and registry snapshots.
`make -C library/shiraz check` builds real Windows PE/COFF inputs with Clang, LLD
and llvm-dlltool and verifies interpretation, rebasing, output, registry changes,
static/shared-runtime and fully static ELF execution, malformed input and compiler
argv handling. No Windows SDK or Wine installation is needed.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-shiraz.h`](../include/mulib-shiraz.h) |
| Implementation switch | `MULIB_IMPL_SHIRAZ` |
| Repository-local includes | [mulib-pe](mulib-pe.md), [mulib-strbuf](mulib-strbuf.md) |
| Complete example | [`examples/modules/shiraz.c`](../examples/modules/shiraz.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/shiraz.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
