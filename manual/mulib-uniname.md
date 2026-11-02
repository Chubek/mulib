# mulib-uniname

Unicode names and reverse lookup using the ICU database, including algorithmic
Hangul syllable and CJK ideograph names.

## Integration

Define `MULIB_IMPL_UNINAME`, `MULIB_IMPL_UNIPROC`, `MULIB_IMPL_DYNLOAD`, and
`MULIB_IMPL_UTF8` before including `mulib-uniname.h`. Link `-ldl` where needed.
Open a `mulib_uniproc_t` context first. `examples/modules/uniname.c` shows a
name/codepoint round trip. ICU is optional at build time and required at runtime.

## API

`mulib_uniname_name(context, codepoint, extended)` returns a malloc-owned,
NUL-terminated name. `extended=0` requests the standard Unicode character name;
unnamed characters return NULL. `extended=1` uses ICU extended names, which also
describe controls, unassigned positions, noncharacters and surrogate positions.
Values above U+10FFFF and extended flags other than 0/1 fail.

`mulib_uniname_lookup(context, name, extended, &codepoint)` returns 0 on success,
-1 on unknown names, invalid arguments, unavailable symbols or native errors.
The output is unchanged on failure. ICU performs case-insensitive name matching;
the extended-name syntax is enabled only by `extended=1`. This API does not
search named sequences, emoji short names, localized descriptions or all name
aliases. Lookup returns one codepoint, which can be a non-scalar position when
extended names are explicitly requested.

Free returned names with `free`. Name strings are independent of the context;
the context is borrowed during each operation. Calls sharing the context should
be serialized. Database coverage follows the installed ICU Unicode version.
Normal names are not fabricated for unnamed codepoints. NULL means absence or
error and is not an allocated empty string.

## Verification

`tests/test_unicode_text.c` covers forward/reverse names, supplementary emoji,
algorithmic Hangul/CJK, unknown names and invalid codepoints. Run
`python3 tools/check_text.py --require-backends` to require an installed ICU.
The implementation resolves the documented ICU `u_charName` and
`u_charFromName` C APIs; it vendors neither implementation code nor data.
