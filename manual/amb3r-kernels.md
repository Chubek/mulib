# Amb3r reference kernels

[Manifest](../MANIFEST.yaml) · [Integration guide](integration.md)

The Amb3r group contains reference kernels for circuit and digital simulation,
numerical methods, hardware design and verification, layout and routing, canvas
tools, RF, instruments, and waveforms. The manifest lists each header's purpose,
implementation switch, and direct MuLIB includes. Consult the declarations and
source comments in the individual headers for argument, ownership, and limit
details.

These kernels use the shared `ma_*` types and helpers from
[`mulib-amb3r-base.h`](../include/mulib-amb3r-base.h). The base is static-inline
and needs no implementation switch. It does not replace or automatically
adapt the older MuLIB EDA, HWIL, SPICE, or solver interfaces.

For the other headers, define the listed `MULIB_IMPL_*` switches for the
modules you use and their dependency closure in one C11 translation unit,
before including the headers. Include the same headers without implementation
switches in consumers. Link with the C math library (`-lm` where needed).

Registration describes the code present in each header. It does not imply
standards conformance, full file-format support, or integration with existing
MuLIB EDA workflows.

Do not combine the new [`dsim-resolve`](../include/mulib-dsim-resolve.h)
implementation with legacy [`dsim-logic4`](../include/mulib-dsim-logic4.h):
both export `mulib_dsim_resolve`, with incompatible signatures. The repository
checker builds the Amb3r profile separately from the legacy aggregate.
