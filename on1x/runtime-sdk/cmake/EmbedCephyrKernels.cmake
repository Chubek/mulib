# Build the translation unit from the actual MuLIB implementation, not a copy
# of the algorithms or a runtime include path. Embed the fixed config too.
function(append_hex name hex)
  file(APPEND "${output}.tmp" "static const unsigned char ${name}[] = {\n")
  string(LENGTH "${hex}" _length)
  set(_offset 0)
  while(_offset LESS _length)
    string(SUBSTRING "${hex}" ${_offset} 32 _line)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," _line "${_line}")
    file(APPEND "${output}.tmp" "  ${_line}\n")
    math(EXPR _offset "${_offset} + 32")
  endwhile()
  file(APPEND "${output}.tmp" "};\n")
endfunction()

set(_prefix [=[
#define MULIB_IMPL_CLAMP
#define mulib_clamp_i64 on1x_stdffi_v1_clamp
#define mulib_clamp_add_i64 on1x_stdffi_v1_add
#define mulib_clamp_sub_i64 on1x_stdffi_v1_sub
]=])
set(_suffix [=[

_Static_assert(sizeof(int64_t) == 8 && sizeof(void *) == 8, "stdffi target ABI drift");
_Static_assert(_Generic(&on1x_stdffi_v1_clamp,
    int64_t (*)(int64_t, int64_t, int64_t): 1, default: 0), "stdffi clamp signature drift");
_Static_assert(_Generic(&on1x_stdffi_v1_add,
    int64_t (*)(int64_t, int64_t): 1, default: 0), "stdffi add signature drift");
_Static_assert(_Generic(&on1x_stdffi_v1_sub,
    int64_t (*)(int64_t, int64_t): 1, default: 0), "stdffi sub signature drift");
]=])
string(HEX "${_prefix}" _prefix_hex)
string(HEX "${_suffix}" _suffix_hex)
# Even building just the embedding library (without on1xc or its generated
# recipe tables) must enforce the same implementation pin as the MIR path.
file(READ "${review}" _review)
string(JSON _expected GET "${_review}" clamp sha256)
file(READ "${clamp}" _clamp_source)
string(SHA256 _actual "${_clamp_source}")
if(NOT _actual STREQUAL _expected)
  message(FATAL_ERROR "Cannot embed stdffi.clamp: implementation differs from the reviewed SHA-256")
endif()
string(HEX "${_clamp_source}" _clamp_hex)
file(READ "${config}" _config_hex HEX)
file(WRITE "${output}.tmp" "/* Generated at build time. */\n")
append_hex(clamp_source "${_prefix_hex}${_clamp_hex}${_suffix_hex}")
append_hex(baseline_config "${_config_hex}")
file(RENAME "${output}.tmp" "${output}")
