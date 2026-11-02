# mulib-sysfonts

System font discovery and matching, plus private application-font registration,
using a runtime-loaded Fontconfig library. This works wherever a compatible
Fontconfig runtime and configuration are installed, including Unix and configured
Windows/macOS installations; it is not a DirectWrite/CoreText backend.

## Integration

Define `MULIB_IMPL_SYSFONTS` and `MULIB_IMPL_DYNLOAD` in one C11 translation unit.
Include `mulib-sysfonts.h`; link `-ldl` where needed. No Fontconfig development
headers or link-time Fontconfig dependency is required.
See `examples/modules/sysfonts.c`.

`open(NULL)` selects `libfontconfig.so.1`, `libfontconfig.1.dylib` or
`libfontconfig-1.dll` according to platform. An explicit library path overrides
this. It creates an owned configuration using `FcInitLoadConfigAndFonts`;
NULL indicates unavailable/incompatible runtime, configuration or allocation
failure. `close` destroys that configuration and releases the loader reference.
The native module is pinned until process exit so Fontconfig's shared caches and
default-substitution configuration remain mapped and reusable. Native module
pinning support is required. The wrapper does not call global `FcFini` or replace
the process's current configuration (Fontconfig may lazily initialize it).

## Records and operations

`mulib_sysfonts_font_t` owns copied `path`, `family` and `style` strings plus the
Fontconfig face index (including named-instance bits when present). Zero-init a
record before matching. Pass the returned path/index to fontdraw or fontshape.
Records remain valid after closing the context; destroy each with
`mulib_sysfonts_font_destroy`.

* `match(fonts, query, &record)` accepts Fontconfig name syntax, for example
  `sans:weight=bold:lang=ar`. It performs configuration substitution, default
  substitution and best-match selection. It is a best match, not a promise that
  a literal family exists or that every requested character is covered.
* `list(fonts, query, &records, &count)` enumerates matching configured faces;
  NULL query means all. Free each record and then `free(records)`. Ordering is
  Fontconfig's native enumeration order. An empty set is a successful NULL array.
* `add(fonts, path)` registers a file as an application font in this private
  configuration. The native library validates it. This does not install or copy
  a font into system directories. `clear` removes application registrations;
  it does not remove system fonts or delete files.

Result codes are 0/-1; matching/enumeration leave outputs unchanged on failure.
Input strings are borrowed during calls. Contexts require caller synchronization.
Only the face metadata above is copied; arbitrary Fontconfig properties are not
exposed by this focused API. Allocation products are checked.

## Verification and ABI

`tests/test_font_stack.c` exercises enumeration, matching, record ownership,
application registration and integration with shaping/rasterization. Optional
backend tests can be required with `MULIB_TEXT_REQUIRE_BACKENDS=1` or
`tools/check_text.py --require-backends`. Runtime-dependent tests otherwise
report a skip when a library is absent.

The implementation uses documented Fontconfig C functions and the stable public
`FcFontSet` layout: <https://www.freedesktop.org/software/fontconfig/fontconfig-devel/>.
No native implementation source is copied or bundled.
