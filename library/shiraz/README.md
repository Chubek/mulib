# Shiraz runtime libraries

Build with `make -j4`; run `make check` for Windows-to-Linux interoperability.
This directory ships source files and build rules. Outputs go to `build/`.

| Output | Contents |
| --- | --- |
| `libshiraz.a`, `libshiraz.so` | PE/COFF loader, AMD64 interpreter/AOT helpers, MuLIB storage dependencies |
| `libshiraz-win32.a`, `libshiraz-win32.so` | Windows imports, handles, files, heap adapters and Registry |
| `shiraz` | `run`, `emit-c` and `compile` commands |
| `shiraz-reg` | Persistent registry `get`, `set` and `delete` application |

`core.c` places MuLIB implementation switches. `runtime.c` owns runtime state and
import resolution; `kernel32.c` implements OS services; `registry.c` provides
ADVAPI32 using `mulib-kvdb.h`. Static/shared builds use the same sources. Archive
link order is `libshiraz-win32.a libshiraz.a`.

```sh
build/shiraz run hello.exe
build/shiraz compile hello.exe hello.elf             # static Shiraz libraries
build/shiraz compile hello.exe hello-static --static # also static libc
build/shiraz compile hello.exe hello-shared --shared-runtime
make install PREFIX=/usr/local DESTDIR=/staging
```

Configure `CC`, `AR`, `CFLAGS`, `CPPFLAGS`, `LDFLAGS`, `INCLUDE`, `BUILD`, `PREFIX`
and `DESTDIR` as needed. Building requires a Linux C11 toolchain. Tests additionally
use Clang's Windows target, LLD and llvm-dlltool. Fully static output needs static
libc archives. Installed CLI defaults use `PREFIX`, not the staging directory.
Override locations with `SHIRAZ_CC`, `SHIRAZ_INCLUDE_DIR`, `SHIRAZ_LIBRARY_DIR`.

## Windows API catalogue

DLL names are ASCII case-insensitive; function names are case-sensitive.
`kernelbase.dll` shares Kernel32 implementations. COFF symbols have no DLL
qualifier and resolve against this catalogue. Missing names/ordinals fail at
runtime attachment with a diagnostic.

| DLL | Functions | Profile |
| --- | --- | --- |
| Kernel32 | `ExitProcess` | Stops the guest; preserves its 32-bit code |
| Kernel32 | `GetLastError`, `SetLastError` | One error slot per runtime |
| Kernel32 | `GetStdHandle`, `ReadFile`, `WriteFile` | Synchronous byte I/O, max 16 MiB per call, count pointer required, no OVERLAPPED |
| Kernel32 | `CreateFileA/W`, `CloseHandle` | Dispositions 1–5, GENERIC_READ/WRITE, ordinary files |
| Kernel32 | `GetProcessHeap`, `HeapAlloc`, `HeapFree` | One heap, flags 0 or HEAP_ZERO_MEMORY; exact allocations freed/coalesced |
| Kernel32 | `VirtualAlloc` | New address, MEM_RESERVE\|MEM_COMMIT, PAGE_READWRITE; 64 KiB-aligned, VM-lifetime storage |
| Kernel32 | `GetCurrentProcessId` | Host PID |
| Advapi32 | `RegCreateKeyExA/W`, `RegOpenKeyExA/W`, `RegCloseKey` | HKCU/HKLM/HKCR/HKU, ordinary keys, intermediate keys retained |
| Advapi32 | `RegSetValueExA/W`, `RegQueryValueExA/W`, `RegDeleteValueA/W` | REG_SZ/BINARY/DWORD, size queries and ERROR_MORE_DATA |

There are 252 owned handle slots plus standard streams. Security descriptors,
asynchronous files, special file flags and named heaps are unsupported. Sharing
flags are accepted without Windows share-denial enforcement. File paths translate
backslashes to slashes; `C:\path` maps to `/path`, other drives fail. `W` path/key/
value-name APIs accept ASCII UTF-16; non-ASCII names report unsupported input.
`A` names are byte strings with ASCII registry case folding. Registry masks do
not implement Windows ACLs; class/options/security parameters must be zero.
Roots are separate namespaces, without HKCR merged-view behavior. VirtualAlloc
has no decommit/protection-changing API and is reclaimed with its VM.

This catalogue is the supported compatibility profile. CRT, User32/GDI, COM,
DirectX, networking, threads/processes, DLL initialization and SEH are not included.

## Registry application

```sh
build/shiraz-reg user.hive set 'HKCU\Software\Demo' Enabled dword 1
build/shiraz-reg user.hive set 'HKCU\Software\Demo' Greeting sz hello
build/shiraz-reg user.hive set 'HKCU\Software\Demo' Bytes binary 00ff1020
build/shiraz-reg user.hive get 'hkcu/software/demo' Greeting
build/shiraz-reg user.hive delete 'HKCU\Software\Demo' Enabled
SHIRAZ_REGISTRY="$PWD/user.hive" build/shiraz run demo.exe
```

CLI `sz` converts byte characters to UTF-16LE; the C API accepts UTF-16LE bytes
directly. DWORD is four little-endian bytes. Values are bounded to 1 MiB and
key/name text to 1023 bytes each. An empty name selects the default value. The
MuLIB KVDB hive has CRC, atomic replacement and explicit save, shared by the
application and Windows imports. It is independent of Microsoft's hive/`.reg`
formats. A NULL path creates memory-only state; destruction discards unsaved data.

See [`manual/mulib-shiraz.md`](../../manual/mulib-shiraz.md) for embedding,
translation semantics, instruction coverage, budgets and MuLL bindings.
