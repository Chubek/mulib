# Run On1x in an ArchVirt machine

ArchVirt is a machine VM, not the `--run-vm` VMWeave bytecode backend.
`on1xc` checks the source and links a freestanding DomIOS guest ELF for
the supported native host architecture. ArchVirt loads that ELF into
guest RAM and starts a virtual CPU. An ordinary Linux executable is not
a DomIOS image and is not run by this mode.

## Compiler and linked launchers

```sh
on1xc --vm-build main.on1x -o main.guest
on1xc --vm-run main.on1x                 # compiles, caches, then runs
on1xc --vm-run-elf main.guest             # validates, caches, then runs
on1xc --vm-rerun KEY                      # use the cache key reported to stderr
on1xc --vm-cache-clear

on1xc --vm-launcher=always main.on1x -o main
./main
on1xc --vm-launcher=opt-in main.on1x -o main-opt
./main-opt --sandbox
```

`--vm-build` writes the guest without running it. Guest and launcher emission
report a stable VM ID and register the guest for replay; `--vm-uncached`
reports the same ID without registering. Reproducible project builds use the
uncached route. `--vm-run` can also export the guest with `-o FILE`. Neither command
makes a guest ELF executable on the host. `--vm-run-elf` accepts the
same DomIOS profile with no access to source. It checks the guest's recorded
runtime-archive fingerprint against the current compiler installation.
That metadata is not a signature; only use it for trusted On1x build
artifacts. Replay uses the exact previously compiled image without consulting
the current source. A
missing or incompatible entry is an error. Clearing the
[private cache](archvirt-cache.md) never deletes an explicitly written
guest or launcher.

The linked output is a native executable linked to `libdomvirt` with an
embedded guest ELF. It stages the guest in a sealed Linux memory file and
uses the ArchVirt C API. The launcher includes **no host-compiled copy of
the On1x program**. `always` enters the VM on every valid invocation;
`opt-in` requires `--sandbox` and otherwise refuses to run. Neither mode
supports guest arguments or attempts a host fallback. Stripping or
rewriting the linked executable after packaging may discard its embedded
image; the launcher then rejects it.

Put `vm always` or `vm opt-in` in a project's **root** `On1xfile` to use
linked builds and the `on1xc aux run`, `vm-rerun`, and `vm-cache-clear`
commands. See the [project policy](../stdlib/on1xfile/README.md#archvirt-launch-policy).
Project VM builds map staging filenames to stable logical paths for runtime
diagnostics. `on1xc aux backends` probes build availability, not access to
`/dev/kvm`.

The [VMM standard-library package](../stdlib/vmm/README.md) checks typed
configuration without native activation. Its optional trusted C companion
can also boot bounded software guests. The two profiles are distinct; the
companion is not a safe-admitted On1x management binding.

## Build and trust boundary

The compiler launch profile currently requires **native x86-64 Linux and
accessible `/dev/kvm`**. It does not silently switch to software emulation
or to another host architecture. Build and install the
[pinned LibStone ArchVirt](../third_party/libstone/cornerstone/ArchVirt/README.md)
into a trusted prefix, then configure On1x with Python 3, Clang and LLD:

```sh
cmake -S on1x -B build/on1x-archvirt \
  -DON1X_WITH_ARCHVIRT=ON -DON1X_ARCHVIRT_ROOT=/absolute/archvirt/prefix
cmake --build build/on1x-archvirt --parallel
ctest --test-dir build/on1x-archvirt --output-on-failure -R '^on1x\.archvirt\.'
```

Launchers and the compiler's separate execution helper depend on the
configured ArchVirt installation or a compatible runtime at deployment.
The compiler itself does not load `libdomvirt` when checking or building.
The guest can use the [freestanding C11
profile](../archvirt/README.md): checked integers, booleans, bounded
aggregate operations, and console output. It receives no host filesystem,
network, environment, argv, or stdin. Floating-point, arbitrary native
linkage, host runtime features and general stdlib availability are not
implied. Guest exit statuses are forwarded, including runtime traps;
runner failures use 125 with a diagnostic. The runner stops an overlong
guest using a five-second watchdog.

The VM boundary applies to guest instructions and
the devices offered by the launcher. It does **not** sandbox `on1xc`, its
host build toolchain, compile-time extensions, the host dynamic loader, or
the ArchVirt implementation itself. Guest output is untrusted data.
Do not use a set-ID launcher or interpret this profile as an audit or as
a hardened multi-tenant hypervisor.
