# `std.vmm`: ArchVirt configuration and trusted host companion

`std.vmm` checks configuration values in On1x without loading, linking, or
starting ArchVirt. A separate optional C-host companion runs one VM from
creation through cleanup. **There is no safe-admitted On1x foreign binding for
VM management.** Neither interface makes ArchVirt, its guest inputs, or its
host-side firmware execution a sandbox.

## On1x configuration

```on1x
import std.vmm as Vmm;

fn main() {
  let machine = variant<Vmm.VmmMachine>(VmmRiscv64, record {});
  let config = Vmm.bundled_config(machine);
  let result = Vmm.validate_config(config, "", "", "", "", "");
  assert(match result with { VmmValid() => true, _ => false });
}
```

`VmmMachine` selects `x86_64`, `aarch64`, or `riscv64`. `VmmBackend` selects
`software`, `auto`, or `kvm`; the latter is rejected for this profile. Positive
instruction limits make ArchVirt's `auto` select its software backend. `VmmConfig`
also contains RAM (40–1024 MiB), instruction limit (1–100,000,000), and
separate `domix` and `domios` boot flags. `bundled_config` selects software,
40 MiB, 3,000,000 instructions, and both flags.

`validate_config` takes ArchML profile, ELF kernel, image, firmware and
command-line strings, in that order, since On1x nominal and structural records
do not accept `string` fields. Empty paths mean the pinned defaults where
applicable; omitting the kernel requires `domix`, and supplying firmware requires
`domios`. Paths are limited to 4,096 bytes each and cmdline to 2,048 bytes.
Validation does not touch the filesystem or validate the contents of any file.

## Optional C-host companion

Configure with `-DON1X_WITH_ARCHVIRT=ON -DON1X_ARCHVIRT_ROOT=/absolute/prefix`
where the prefix is an installed ArchVirt build from pinned LibStone
`21f886afe61ce27ef5684acde8f1ed7c064e4e6a` (ArchVirt reports `0.3.0`).
The `on1x_stdlib_vmm` CMake target links `On1xArchVirt::Library`; installation
adds `lib/libon1x_stdlib_vmm.a` and `include/on1x/vmm.h`. Consumers must explicitly
link the companion, the pinned `libdomvirt.so`, and the C++ runtime; keep the
matching ArchVirt shared dependencies and boot assets available. For example:

```c
#include <on1x/vmm.h>
#include <stdio.h>

int main(void) {
    const On1xVmmConfigV1 config = {
        .struct_size = sizeof config,
        .machine = "riscv64", .backend = "software",
        .memory_mb = 40, .instruction_limit = 3000000,
        .flags = ON1X_VMM_DOMIX | ON1X_VMM_DOMIOS
    };
    const uint8_t input[] = "uname\nexit\n";
    On1xVmmReportV1 report;
    On1xVmmStatus status = on1x_vmm_run_v1(&config, input, sizeof input - 1, &report);
    if (status != ON1X_VMM_OK) {
        fprintf(stderr, "VM phase %d: %s\n", (int)status, report.error);
        return 1;
    }
    fwrite(report.serial_output, 1, report.serial_output_size, stdout);
    return report.serial_output_truncated != 0;
}
```

The bridge validates the C configuration again, starts the pinned software VM
and joins its worker before publishing the caller-owned report. It releases the
VM on success and recoverable failures; if start or wait fails it attempts
stop and join before destruction. The native handle and retained callback
context are never returned. No callback enters On1x or user C code. At most
one VM operation is allowed per linked companion instance; overlap returns
`ON1X_VMM_BUSY`. Do not mix this adapter with concurrent raw ArchVirt use.

Input is at most 4,096 bytes; the callback consumes input in order, including
NUL, and returns zero on exhaustion as required by the upstream byte API.
The report contains consumed count, at most 4,096 bytes of output, and explicit
output truncation. A truncated capture does not stop the VM. Machine output
is bytes, not a C or On1x string. Diagnostics are copied before destroy,
NUL-terminated with at most 1,023 diagnostic bytes, and have a separate
truncation indicator. `WAIT_FAILED` covers guest errors including instruction
exhaustion as well as native join failures; the C API does not give a stable
subcode to distinguish them.

The adapter is for trusted inputs, host environment, and a trusted native
library. ArchVirt reads host paths and runs DomIOS boot code in the host.
The instruction budget applies to software guest stepping, not filesystem
access, host firmware, or wall time. Host-fatal faults can terminate the
process with no cleanup guarantee. There is no exposed cancellation,
live serial console, concurrent guest session, or untrusted execution profile.

## Public C API inventory at the pin

| Facility (`archvirt.h`) | Disposition |
|---|---|
| `ArchVirtConfig` and `ARCHVIRT_DOMIX` / `ARCHVIRT_DOMIOS` | Internal, native header-derived layout and `struct_size`; checked `On1xVmmConfigV1` maps every field explicitly. |
| `archvirt_create`, `archvirt_initialize` | Private in the single-call companion; do not return a raw pointer or numeric handle. |
| `ArchVirtSerial`, `archvirt_set_serial` | Private C callback installed before init; byte input and bounded capture only. `NULL`/host terminal is never selected. |
| `archvirt_start`, `archvirt_wait` | Private, ordered start and join; no detach or restart. |
| `archvirt_stop`, `archvirt_destroy` | Internal cleanup; no external stop method or cross-thread shutdown claim. |
| `archvirt_last_error` | Copies borrowed `std::string::c_str()` while its owner is live; not immutable static storage. |
| `archvirt_version` | Native callers can read the pinned library's immutable version literal. Version string is not an ABI query or proof of revision. The On1x contract seed is not admitted. |
| `domvirt_create`, `domvirt_initialize`, `domvirt_run`, `domvirt_stop`, `domvirt_destroy`, `domvirt_version` | Legacy aliases not wrapped. Legacy init consults host environment, and legacy run starts asynchronously, not start-and-wait. |
| `ArchVirtBootInfo` and `ARCHVIRT_BOOT_MAGIC` (`archvirt_boot.h`) | Guest entry ABI, not a host management call. |

The internal C++ `machine::Runtime` also has `pause`, `resume`, `step`,
register/memory inspection and BIOS-mode options. None are declared in the
public C API at this pin. This package does not bind C++ private/internal
layouts, test/control sockets, or the CLI as if they were the C ABI.

## Admission and remaining work

The [reviewed FFI profile](../../docs/ffi-strings.md) rejects aggregate
arguments/results, byte buffer borrows, retained callbacks, ownership and
cleanup obligations, error classification, and allocation/thread effects.
The C shim narrows the authority and lifetime surface, but `on1x_vmm_run_v1`
still has aggregate and byte-span arguments, writes a borrowed report, uses
blocking host I/O, starts a native worker, and destroys a foreign resource.
Therefore the package declares **no extern** for it, ships no handwritten
admission evidence, and cannot be linked implicitly by an On1x import.

`version.contract.json` is an **unreviewed** ExoML seed for
`archvirt_version` alone. Pinning against the matching `archvirt.h` yields
Gate B and `reviewed-immutable-string/v4` eligibility. Independently review
the pinned literal implementation (`src/domvirt.cpp` and its build definition)
and installed library identity before using `on1x-bind emit-safe` with the
pinned shared object passed via `--impl`, then explicitly pass the emitted
adapter and `--admitted-extern` evidence to consumers. `check`/`pin` do not
perform admission. The package test checks that the checked-in seed cannot
publish bindings.

To admit a run from On1x, first add a reviewed scoped boundary for bounded
byte input and initialized aggregate output, stage-specific error lowering,
and blocking/native-thread/foreign-resource effects. For incremental use,
require affine thread-affine VM ownership, consuming close, and verified
callback quiescence before introducing a public VM value. Upstream would
need a typed outcome and quiescence API (wait currently returns only `int`,
and destroy relies on the internal runtime destructor), synchronous
cancel/deadline semantics, and an ABI revision query if those guarantees are
needed. To enforce a host file policy, file/config acquisition must use
reviewed already-open resources or a separate enforced isolation boundary;
this C API takes paths and does not provide either.

## Verification

Build the enabled configuration and run `ctest --test-dir BUILD -R
'^on1x\.stdlib\.vmm\.' --output-on-failure`. The native fixture runs the actual
bundled software guests on all three architectures, also exercises explicit
profile/kernel/image/firmware/cmdline, tests raw byte input, capture and error
truncation, limits and overlap, and reruns after failure. It needs the
installed `share/archvirt` data. The On1x fixture compiles and executes
configuration validation without linking ArchVirt and rejects an attempt to
invoke a nonexistent `start` export. The admission fixture requires the
optional ExoML extractor and Chisel builds.
