# Networking module integration

The 92 networking modules are integrated into the root MuLIB inventory and
standard build. Their supported subsets are documented in the individual
manuals; this is not a complete network manager or TCP/IP implementation.

## Inventory

`MANIFEST.yaml` contains all networking module metadata and dependencies.
Combined C declarations/implementations, dependency strata, manual references,
and the generated MuLL bridge include these headers. No separate manifest
fragment needs to be merged.

When adding APIs, follow `CONTRIBUTING.md` and regenerate the same inventories.

## Single-header integration

A module emits implementation only if its `MULIB_IMPL_NAME` is defined. All transitive dependency implementations must also be enabled explicitly **in exactly one translation unit**. See `tests/test_ustack.c` or `examples/ustack-demo.c` for the dependency switches. Other translation units only include the headers (C linkage declarations are compatible with C++).

### Compile examples

`./tests/run.sh` runs the repository's CMake/CTest suites, including 98 networking
test sources. `./tests/check-cxx.sh` verifies cross-TU C++17 consumption (also
covered by CTest). `./examples/build.sh` builds 12 networking demonstrations.
The 92 per-module examples are in `examples/modules/` and are checked by
`python3 tools/check.py`. Assertions must remain enabled for checked examples.

### Important limitations

The Netlink and rtnetlink basis is real Linux kernel API use; specialized modules provide scoped query/control operations, not complete protocol coverage. `nm-secret-agent` only supports safe Unregister; `polkit-client` only checks service presence, and these are **not** complete credential/authorization clients. The `ustack` packet path implements ARP, IPv4, ICMPv4 echo and UDP with callback-driven Ethernet I/O. A general-purpose TCP stack, IPv6 end-to-end stack, DHCP lease orchestration and secure Wi-Fi authentication are not implemented. Consult each manual before relying on an API in production.

### Safety

Never run mutation examples on a host production network. Test network setup only inside disposable network namespaces with explicit CAP_NET_ADMIN/CAP_NET_RAW. Kernel/device APIs vary by release; `-ENOSYS`, `-EPROTONOSUPPORT`, or other negative errno indicates an unsupported operation or failure. No code bypasses Linux privileges.

### Validation conducted

The networking tests pass strict warning checks and Clang ASan/UBSan. See
[VALIDATION.md](VALIDATION.md) for verification scope and remaining limitations.
Real Wi-Fi association, privileged mutations, and full daemon interoperability
are not certified by the offline and read-only suites.
