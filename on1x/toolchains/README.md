# On1x SDK toolchain policy

The SDK does not treat a provider name, `driver_id`, command, environment
variable, or `PATH` entry as executable authority.

An SDK 2 toolchain provider copies a validated target profile, logical driver
identity, version, roles, priority, and SHA-256 identity into the shared
registry. Separately, a host-owned `On1xSDK_ToolchainResolver` may authorize an
exact canonical absolute path for that driver/role/digest tuple. Resolution:

1. matches the complete validated target and one requested tool role;
2. deterministically chooses the unique highest-priority provider;
3. requires an exact host binding and digest match;
4. returns an owned inert resolution record.

It never searches, opens, shells out to, or executes the path. Equal-ranked
providers fail as ambiguous. Relative paths, `.`/`..`, empty components, shell
metacharacters, arbitrary command strings, and mismatched digests reject.
Changing installation roots requires a new host binding; provider metadata
remains path-independent.

No host C compiler is labeled a native On1x backend. Current executable backend
artifacts are the verified HIR/MIR adapters documented in
[`../backends/README.md`](../backends/README.md).

## Probe an explicit host toolchain

`on1x-toolchain-probe.py` exercises a native Linux x86-64/glibc installation
before you select its tools for an On1x build. The existing Archon
[`on1x-native-build.py --probe`](../tools/on1x-native-build.py) checks the
configured Archon backend and resolves tool paths; this separate probe
actually compiles, assembles, links, and runs a fixed fixture. It needs
Python 3.9+, a GCC- or Clang-compatible C driver, and GNU-compatible `as`
and `ld`. Pass all three as trusted absolute executable paths:

```sh
python3 on1x/toolchains/on1x-toolchain-probe.py \
  --target x86_64-unknown-linux-gnu \
  --cc /usr/bin/gcc --assembler /usr/bin/as --linker /usr/bin/ld
```

The probe checks the reported compiler target and compiles C11 layout
assertions for signed `char`, LP64, binary32/binary64, and one mixed record.
It assembles a separate GNU-syntax routine with the selected assembler. The
selected linker must combine both objects, then the driver must link the
result with its usual startup objects and glibc using the native helper's
`-B` / `-fuse-ld=bfd` selection and non-PIE mode. The probe reads the ELF
headers of both inputs and both link outputs. It executes only its own
fixture, checking that an integer and a double reach the correct AMD64 SysV
registers and that the double result and libc call work. Clang may use its
integrated assembler for the C translation unit; the separate assembly
routine always goes through the selected external assembler.

Success writes one `on1x.toolchain-probe/1` JSON record to stdout. It includes
the canonical target, profile name, compiler-reported target, and the three
tools' canonical paths, version banners, and SHA-256 digests. The probe adds
no timestamps or staging paths; output repeats byte-for-byte for unchanged
paths, bytes, and banners. Installation paths are host-specific. Failure writes a
stage diagnostic to stderr, exits nonzero, and emits no record. Temporary
artifacts are private and removed; use a writable, executable `TMPDIR`.

Use only trusted tools and a trusted system installation: this command
executes the selected programs and the generated fixture without a sandbox.
The driver and system startup files, headers, libraries, and loader are
transitive dependencies; their contents are not locked by this record. The
probe does not accept user source, tool arguments, flags, sysroots, manifests,
or extension metadata. The record is **evidence only**: neither the SDK
resolver nor existing drivers ingest it, and it grants no path binding or
FFI admission. For the Archon path, still use `on1x-native-build.py` with its
own target/ABI checks and the same explicit `--cc`, `--assembler`, and
`--linker` paths. Cross-target execution, PIE/shared objects, runtime or
kernel ABI certification, and the embedded Cephyr toolchain are outside
this probe.

### Tests

```sh
python3 -B -m unittest discover -s on1x/toolchains/tests -v
```

The suite uses real GCC/Clang and GNU binutils when available. It also checks
that a misreported target, unsigned `char`, the wrong ELF class, absent
linker output, bypass of the selected linker, a mismatched calling convention,
and tool-byte drift cannot produce evidence.

See [verification notes](VERIFICATION.md) for the tested host and build profiles.
