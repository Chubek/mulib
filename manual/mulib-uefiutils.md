# mulib-uefiutils

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_UEFIUTILS` and `MULIB_IMPL_UEFI` once.
[Example](../examples/modules/uefiutils.c). All storage is caller-owned.

## Device paths and boot variables

`path_check` checks node framing (minimum four bytes, bounded 16-bit length),
End Instance/End Entire rules and FilePath node termination. It accepts unknown
nodes without interpreting their payloads. A complete path must end exactly at
End Entire; it is not a semantic validator of every hardware/vendor node.
`file_path` emits a standard Media/FilePath node plus End Entire from native
CHAR16 units, excluding NUL. It limits node size to UINT16_MAX. This short-form
path does not identify a volume; callers may need a full device path for loading.

`option_open` decodes the packed `EFI_LOAD_OPTION`: UINT32 attributes, UINT16
FilePathListLength, NUL-terminated CHAR16 description, device-path list and
arbitrary optional bytes. A file-path list can contain multiple complete paths.
Wire strings remain byte pointers since these fields can be unaligned.
`option_build` serializes the same fields. Unknown attribute bits are retained;
firmware/version-specific policy remains with the caller.

`boot_name` writes `Boot####` using four uppercase hex digits and a trailing
CHAR16 NUL. `boot_number` decodes an indexed little-endian UINT16 from BootOrder
or BootNext bytes, rejecting odd/truncated input. These functions don't mutate
NVRAM. Use the UEFI variable wrappers and global-variable GUID for that.

Builders and `utf8` accept NULL output for sizing. `required` includes any
terminator; insufficient capacity returns BUFFER without partial writes.
Readers are atomic on error. Inputs must remain immutable and must not overlap
output or metadata destinations. Results: 0 OK, -1 INVALID, -2 FORMAT, -3 BUFFER.

## Shell and loaded images

`shell` obtains `EFI_SHELL_PARAMETERS_PROTOCOL` from the current image with
**HandleProtocol**, not LocateProtocol. The native layout is Argv, Argc, StdIn,
StdOut, StdErr. Arguments have already been tokenized by the Shell; don't split
them again. The redirected handles can differ from system-console handles.
The protocol is optional and its absence is returned as a native EFI status.

`loaded_image` obtains `EFI_LOADED_IMAGE_PROTOCOL` and exposes binary load
options, image metadata and unload callback. Returned pointers are borrowed and
become invalid when firmware unloads the image. Both helpers observe the UEFI
context's boot lifecycle.

`utf8` converts bounded native UTF-16 to UTF-8, validating surrogate pairs and
rejecting embedded NUL. This utility accepts supplementary characters; firmware
interfaces documented as UCS-2 can impose stricter character limits. It doesn't
claim all firmware accepts arbitrary Unicode. A Shell command executor, full
Shell protocol, filesystem abstraction and shell-language parser are not included.

## References

UEFI 2.x device paths and EFI_LOAD_OPTION; UEFI Shell 2.2 parameters protocol.
Cross-checked with [EDK II ShellParameters.h](https://github.com/tianocore/edk2/blob/master/MdePkg/Include/Protocol/ShellParameters.h)
and [LoadedImage.h](https://github.com/tianocore/edk2/blob/master/MdePkg/Include/Protocol/LoadedImage.h).
Accessed 2026-11-06; original implementation. Tests cover truncated paths,
binary optional data, Shell lookup, Unicode and native layout offsets.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-uefiutils.h`](../include/mulib-uefiutils.h) |
| Implementation switch | `MULIB_IMPL_UEFIUTILS` |
| Repository-local includes | [mulib-uefi](mulib-uefi.md) |
| Complete example | [`examples/modules/uefiutils.c`](../examples/modules/uefiutils.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/uefiutils.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
