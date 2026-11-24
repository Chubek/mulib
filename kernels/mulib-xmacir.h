/** @file mulib-xmacir.h
 * @brief Bounded X-macro interchange for straight-line U32 code or byte blobs.
 *
 * No external dependencies. Define MULIB_IMPL_XMACIR in one C translation unit
 * on an ASCII-compatible host. This module neither runs m4 nor emits or runs C.
 */
#ifndef MULIB_XMACIR_H
#define MULIB_XMACIR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MULIB_XMACIR_NAME_CAPACITY 64
#define MULIB_XMACIR_MAX_STACK 256
#define MULIB_XMACIR_MAX_INSTRUCTIONS 65536UL
#define MULIB_XMACIR_MAX_BYTES (6UL * 1024 * 1024)
#define MULIB_XMACIR_MAX_TEXT (16UL * 1024 * 1024)
#define MULIB_XMACIR_BYTES_PER_LINE 32

typedef enum {
  MULIB_XMACIR_U32 = 1, MULIB_XMACIR_BLOB
} mulib_xmacir_mode_t;

typedef enum {
  MULIB_XMACIR_PUSH, MULIB_XMACIR_ADD, MULIB_XMACIR_SUB, MULIB_XMACIR_MUL,
  MULIB_XMACIR_AND, MULIB_XMACIR_OR, MULIB_XMACIR_XOR, MULIB_XMACIR_DUP,
  MULIB_XMACIR_DROP, MULIB_XMACIR_SWAP, MULIB_XMACIR_RET
} mulib_xmacir_opcode_t;

typedef struct {
  mulib_xmacir_opcode_t opcode;
  unsigned long value; /* Used by PUSH only; limited to 0..4294967295. */
} mulib_xmacir_instruction_t;

typedef struct {
  mulib_xmacir_mode_t mode;
  char entry[MULIB_XMACIR_NAME_CAPACITY], bridge[MULIB_XMACIR_NAME_CAPACITY];
  char context_tag[MULIB_XMACIR_NAME_CAPACITY], error_tag[MULIB_XMACIR_NAME_CAPACITY];
  mulib_xmacir_instruction_t *instructions;
  size_t count;
  unsigned char *bytes;
  size_t length;
} mulib_xmacir_program_t;

typedef struct {
  size_t offset;
  const char *message; /* Static message; NULL on success. */
} mulib_xmacir_error_t;

/**
 * Parse exactly length ASCII bytes, without a required NUL terminator.
 * One call per LF-separated line; the final LF is optional. Spaces and tabs
 * are allowed around tokens, and blank lines are ignored. No comments,
 * quotes, embedded NUL, preprocessing directives, or raw C are permitted.
 *
 * XM_U32(entry) is followed by XM_PUSH(decimal) and/or nullary XM_ADD,
 * XM_SUB, XM_MUL, XM_AND, XM_OR, XM_XOR, XM_DUP, XM_DROP and XM_SWAP calls,
 * then one terminal XM_RET() with exactly one stack value. Binary operations
 * pop right then left; all arithmetic is unsigned modulo 2^32.
 *
 * XM_BLOB(entry,bridge,context_tag,error_tag) is followed by zero or more
 * XM_BYTES(h...) calls (0..32 bytes of lowercase, paired hex digits per call)
 * and one terminal XM_END(). The entry and bridge must be distinct. Names
 * are 1..63 ASCII bytes, beginning with a letter and followed only by letters,
 * digits or underscores. C keywords, selected reserved/library names, bare
 * m4 builtins and the XM_ and m4_ prefixes are excluded. Embedders must avoid
 * collisions with their own names and macros. Supply only canonical formatted
 * text to trusted m4 rules, define rule macros only under XM_ or m4_, and
 * parse the rewritten text again before use.
 *
 * Returns 1 and assigns an independently owned program, or 0 and leaves out
 * unchanged. Start with a zeroed destination; free any previous result
 * before reusing it. Source must remain unchanged during the call. error is optional;
 * offsets are byte offsets into source (possibly EOF).
 */
int mulib_xmacir_parse(const char *source, size_t length, mulib_xmacir_program_t *out,
                       mulib_xmacir_error_t *error);

/** Free a parsed program and clear it. NULL is harmless. */
void mulib_xmacir_free(mulib_xmacir_program_t *program);

/**
 * Validate a parsed or directly constructed program; returns 1 on success.
 * The read-only borrow lasts for the call; arrays must be readable for their
 * declared counts. Start a directly constructed program zeroed. Use only
 * instructions/count with U32 and only bytes/length with BLOB. Non-PUSH
 * values are ignored. BLOB tags omit the C keyword "struct".
 * For instruction errors, error.offset is the instruction index (or count
 * for a missing return); other model errors use offset zero.
 */
int mulib_xmacir_validate(const mulib_xmacir_program_t *program, mulib_xmacir_error_t *error);

/**
 * Validate and serialize a read-only borrowed program. Returns 1 and sets
 * out and length to a malloc-owned, NUL-terminated ASCII string and its size
 * excluding the terminator. Free the string with free(). Both output
 * pointers are required and are left unchanged on failure. Canonical text
 * has no whitespace except LF, no leading decimal zeroes, and full 32-byte
 * blob lines except for a possible shorter final chunk.
 */
int mulib_xmacir_format(const mulib_xmacir_program_t *program, char **out, size_t *length,
                        mulib_xmacir_error_t *error);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_XMACIR_H */

#if defined(MULIB_IMPL_XMACIR) && !defined(MULIB_XMACIR_IMPLEMENTED)
#define MULIB_XMACIR_IMPLEMENTED
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* Private allocator seam: injected allocations must be free()-compatible. */
#ifndef MULIB_XMACIR_MALLOC_
#define MULIB_XMACIR_MALLOC_ malloc
#endif

typedef struct {
  const char *name;
  unsigned need;
  int delta;
} mulib_xmacir_op_info_;

static const mulib_xmacir_op_info_ mulib_xmacir_ops_[] = {
  { "XM_PUSH", 0, 1 }, { "XM_ADD", 2, -1 }, { "XM_SUB", 2, -1 },
  { "XM_MUL", 2, -1 }, { "XM_AND", 2, -1 }, { "XM_OR", 2, -1 },
  { "XM_XOR", 2, -1 }, { "XM_DUP", 1, 1 }, { "XM_DROP", 1, -1 },
  { "XM_SWAP", 2, 0 }, { "XM_RET", 1, 0 }
};

typedef struct {
  const char *source;
  size_t length, pos;
  mulib_xmacir_error_t *error;
} mulib_xmacir_reader_;

static int
mulib_xmacir_report_(mulib_xmacir_error_t *error, size_t offset, const char *message)
{
  if (error) {
    error->offset = offset;
    error->message = message;
  }
  return message == NULL;
}

static int
mulib_xmacir_alpha_(unsigned char c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int
mulib_xmacir_digit_(unsigned char c)
{
  return c >= '0' && c <= '9';
}

static int
mulib_xmacir_word_byte_(unsigned char c)
{
  return mulib_xmacir_alpha_(c) || mulib_xmacir_digit_(c) || c == '_';
}

static int
mulib_xmacir_name_(const char *name, size_t n)
{
  static const char *const reserved[] = {
    /* C90 through C23, plus common compiler extensions and built-in macros. */
    "alignas", "alignof", "asm", "auto", "bool", "break", "case", "char", "const",
    "constexpr", "continue", "default", "do", "double", "else", "enum", "extern",
    "false", "float", "for", "goto", "if", "inline", "int", "long", "nullptr",
    "register", "restrict", "return", "short", "signed", "sizeof", "static",
    "static_assert", "struct", "switch", "thread_local", "true", "typedef",
    "typeof", "typeof_unqual", "union", "unsigned", "void", "volatile", "while",
    "linux", "unix",
    /* POSIX m4 and default GNU m4 builtin names, even when used as bare atoms. */
    "builtin", "changecom", "changequote", "debugfile", "debugmode", "decr",
    "define", "defn", "divert", "divnum", "dnl", "dumpdef", "errprint", "esyscmd",
    "eval", "format", "ifdef", "ifelse", "include", "incr", "index", "indir",
    "len", "m4exit", "m4wrap", "maketemp", "mkstemp", "patsubst", "popdef",
    "pushdef", "regexp", "shift", "sinclude", "substr", "syscmd", "sysval",
    "traceoff", "traceon", "translit", "undefine", "undivert",
    /* Common standard header and linkage collisions in generated C. */
    "main", "FILE", "EOF", "BUFSIZ", "FILENAME_MAX", "FOPEN_MAX", "L_tmpnam",
    "NAN", "INFINITY", "HUGE_VAL", "HUGE_VALF", "HUGE_VALL", "CLOCKS_PER_SEC",
    "NULL", "WEOF", "I", "complex", "imaginary", "offsetof", "assert", "NDEBUG",
    "EDOM", "EILSEQ", "ERANGE", "unreachable", "ckd_add", "ckd_sub", "ckd_mul",
    "errno", "errno_t", "rsize_t", "RSIZE_MAX", "size_t", "ptrdiff_t", "wchar_t",
    "wint_t", "max_align_t", "nullptr_t", "char8_t", "char16_t", "char32_t",
    "fpos_t", "va_list", "jmp_buf", "sig_atomic_t", "clock_t", "time_t", "tm",
    "mbstate_t", "localeconv", "setlocale", "lconv", "stdin", "stdout", "stderr",
    "abort", "abs", "aligned_alloc", "asctime", "atexit", "atof", "atoi", "atol",
    "atoll", "at_quick_exit", "bsearch", "calloc", "clearerr", "clock", "ctime",
    "difftime", "div", "div_t", "exit", "fclose", "feof", "ferror", "fflush",
    "fgetc", "fgetpos", "fgets", "fopen", "fprintf", "fputc", "fputs", "fread",
    "free", "free_aligned_sized", "free_sized", "freopen", "fscanf", "fseek",
    "fsetpos", "ftell", "fwrite", "getc",
    "getchar", "getenv", "gets", "gmtime", "labs", "ldiv", "ldiv_t", "llabs",
    "lldiv", "lldiv_t", "localtime", "longjmp", "malloc", "mktime", "perror",
    "printf", "putc", "putchar", "puts", "qsort", "quick_exit", "raise", "rand",
    "realloc", "remove", "rename", "rewind", "scanf", "setbuf", "setjmp",
    "setvbuf", "signal", "snprintf", "sprintf", "srand", "sscanf", "system",
    "time", "timespec", "timespec_get", "timespec_getres", "tmpfile", "tmpnam",
    "ungetc", "vfprintf", "vfscanf",
    "vprintf", "vscanf", "vsnprintf", "vsprintf", "vsscanf",
    "call_once", "once_flag", "fenv_t", "fexcept_t", "float_t", "double_t",
    "btowc", "mblen", "mbrlen", "mbrtoc8", "mbrtoc16", "mbrtoc32", "mbrtowc",
    "mbtowc", "wctob", "wctomb", "wcrtomb", "c8rtomb", "c16rtomb", "c32rtomb",
    "fgetwc", "fgetws", "fputwc", "fputws", "getwc", "getwchar", "putwc",
    "putwchar", "fwide", "fwprintf", "fwscanf", "swprintf", "swscanf", "ungetwc",
    "vfwprintf", "vfwscanf", "vswprintf", "vswscanf", "vwprintf", "vwscanf",
    "wprintf", "wscanf", "feclearexcept", "fegetenv", "fegetexceptflag",
    "fegetround", "feholdexcept", "feraiseexcept", "fesetenv", "fesetexceptflag",
    "fesetround", "fetestexcept", "feupdateenv"
  };
  static const char *const prefixes[] = {
    "XM_", "m4_", "ATOMIC_", "BITINT_", "CHAR_", "DBL_", "DEC", "EXIT_", "FE_",
    "FLT_", "FP_", "INT", "LC_", "LDBL_", "LLONG_", "LONG_", "MATH_", "MB_",
    "ONCE_", "PRI", "PTRDIFF_", "SCHAR_", "SCN", "SEEK_", "SHRT_", "SIG",
    "SIZE_", "TIME_", "TSS_", "UCHAR_", "UINT", "ULLONG_", "ULONG_", "USHRT_",
    "WCHAR_", "WINT_", "atomic_", "cnd_", "mtx_", "stdc_", "thrd_", "tss_", "va_"
  };
  static const char *const future[] = {
    "is", "to", "str", "mem", "mbs", "wcs", "wmem"
  };
  static const char *const math[] = {
    "acos", "acosh", "asin", "asinh", "atan", "atan2", "atanh", "cabs", "cacos",
    "cacosh", "carg", "casin", "casinh", "catan", "catanh", "cbrt", "ccos",
    "ccosh", "ceil", "cexp", "cimag", "clog", "conj", "copysign", "cos", "cosh",
    "cpow", "cproj", "creal", "csin", "csinh", "csqrt", "ctan", "ctanh", "erf",
    "erfc", "exp", "exp2", "expm1", "fabs", "fdim", "floor", "fma", "fmax",
    "fmin", "fmod", "frexp", "hypot", "ilogb", "ldexp", "lgamma", "llrint",
    "llround", "log", "log10", "log1p", "log2", "logb", "lrint", "lround",
    "modf", "nan", "nearbyint", "nextafter", "nexttoward", "pow", "remainder",
    "remquo", "rint", "round", "scalbln", "scalbn", "signbit", "sin", "sinh",
    "sqrt", "tan", "tanh", "tgamma", "trunc"
  };
  size_t i, k;
  if (n == 0 || n >= MULIB_XMACIR_NAME_CAPACITY ||
      !mulib_xmacir_alpha_((unsigned char)name[0])) return 0;
  for (i = 1; i < n; ++i)
    if (!mulib_xmacir_word_byte_((unsigned char)name[i])) return 0;
  for (i = 0; i < sizeof(reserved) / sizeof(*reserved); ++i)
    if (n == strlen(reserved[i]) && memcmp(name, reserved[i], n) == 0) return 0;
  for (i = 0; i < sizeof(prefixes) / sizeof(*prefixes); ++i) {
    k = strlen(prefixes[i]);
    if (n >= k && memcmp(name, prefixes[i], k) == 0) return 0;
  }
  for (i = 0; i < sizeof(future) / sizeof(*future); ++i) {
    k = strlen(future[i]);
    if (n > k && name[k] >= 'a' && name[k] <= 'z' &&
        memcmp(name, future[i], k) == 0) return 0;
  }
  for (i = 0; i < sizeof(math) / sizeof(*math); ++i) {
    k = strlen(math[i]);
    if ((n == k || (n == k + 1 && (name[k] == 'f' || name[k] == 'l'))) &&
        memcmp(name, math[i], k) == 0) return 0;
  }
  if (n > 2 && name[n - 2] == '_' && name[n - 1] == 't' &&
      ((n >= 3 && memcmp(name, "int", 3) == 0) ||
       (n >= 4 && memcmp(name, "uint", 4) == 0))) return 0;
  return 1;
}

static int
mulib_xmacir_model_name_(const char *name)
{
  const char *end = (const char *)memchr(name, '\0', MULIB_XMACIR_NAME_CAPACITY);
  return end != NULL && mulib_xmacir_name_(name, (size_t)(end - name));
}

static const char *
mulib_xmacir_step_(const mulib_xmacir_instruction_t *instruction, unsigned *depth)
{
  const mulib_xmacir_op_info_ *op;
  if ((unsigned)instruction->opcode > MULIB_XMACIR_RET) return "invalid U32 opcode";
  op = &mulib_xmacir_ops_[instruction->opcode];
  if (instruction->opcode == MULIB_XMACIR_PUSH && instruction->value > 4294967295UL)
    return "U32 literal out of range";
  if (*depth < op->need) return "U32 stack underflow";
  if (instruction->opcode == MULIB_XMACIR_RET && *depth != 1)
    return "XM_RET requires exactly one value";
  if (op->delta > 0 && *depth == MULIB_XMACIR_MAX_STACK) return "U32 stack overflow";
  *depth = (unsigned)((int)*depth + op->delta);
  return NULL;
}

int
mulib_xmacir_validate(const mulib_xmacir_program_t *program, mulib_xmacir_error_t *error)
{
  if (!program || !mulib_xmacir_model_name_(program->entry))
    return mulib_xmacir_report_(error, 0, "invalid program or entry identifier");
  if (program->mode == MULIB_XMACIR_U32) {
    unsigned depth = 0;
    size_t i;
    if (program->bytes || program->length || program->bridge[0] ||
        program->context_tag[0] || program->error_tag[0])
      return mulib_xmacir_report_(error, 0, "U32 program has BLOB fields");
    if (!program->instructions || program->count == 0 ||
        program->count > MULIB_XMACIR_MAX_INSTRUCTIONS ||
        program->count > (size_t)-1 / sizeof(*program->instructions))
      return mulib_xmacir_report_(error, 0, "invalid U32 instruction array or count");
    for (i = 0; i < program->count; ++i) {
      const mulib_xmacir_instruction_t *instruction = &program->instructions[i];
      const char *problem = mulib_xmacir_step_(instruction, &depth);
      if (problem) return mulib_xmacir_report_(error, i, problem);
      if (instruction->opcode == MULIB_XMACIR_RET) {
        if (i + 1 != program->count)
          return mulib_xmacir_report_(error, i, "XM_RET is not terminal");
        return mulib_xmacir_report_(error, 0, NULL);
      }
    }
    return mulib_xmacir_report_(error, program->count, "missing XM_RET");
  }
  if (program->mode == MULIB_XMACIR_BLOB) {
    if (!mulib_xmacir_model_name_(program->bridge) ||
        !mulib_xmacir_model_name_(program->context_tag) ||
        !mulib_xmacir_model_name_(program->error_tag) ||
        strcmp(program->entry, program->bridge) == 0)
      return mulib_xmacir_report_(error, 0, "invalid BLOB bridge or tag identifier");
    if (program->instructions || program->count || program->length > MULIB_XMACIR_MAX_BYTES ||
        (program->length && !program->bytes))
      return mulib_xmacir_report_(error, 0, "invalid BLOB byte array or length");
#if UCHAR_MAX > 255
    {
      size_t i;
      for (i = 0; i < program->length; ++i)
        if (program->bytes[i] > 255)
          return mulib_xmacir_report_(error, 0, "BLOB contains a non-octet value");
    }
#endif
    return mulib_xmacir_report_(error, 0, NULL);
  }
  return mulib_xmacir_report_(error, 0, "invalid program mode");
}

static unsigned char
mulib_xmacir_peek_(const mulib_xmacir_reader_ *r)
{
  return r->pos < r->length ? (unsigned char)r->source[r->pos] : 0;
}

static void
mulib_xmacir_space_(mulib_xmacir_reader_ *r, int newlines)
{
  unsigned char c;
  while ((c = mulib_xmacir_peek_(r)) == ' ' || c == '\t' || (newlines && c == '\n'))
    ++r->pos;
}

static size_t
mulib_xmacir_word_(mulib_xmacir_reader_ *r)
{
  size_t start;
  mulib_xmacir_space_(r, 0);
  start = r->pos;
  while (mulib_xmacir_word_byte_(mulib_xmacir_peek_(r))) ++r->pos;
  return r->pos - start;
}

static int
mulib_xmacir_equal_(const char *word, size_t n, const char *literal)
{
  return n == strlen(literal) && memcmp(word, literal, n) == 0;
}

static int
mulib_xmacir_expect_(mulib_xmacir_reader_ *r, unsigned char c, const char *message)
{
  mulib_xmacir_space_(r, 0);
  if (mulib_xmacir_peek_(r) != c) return mulib_xmacir_report_(r->error, r->pos, message);
  ++r->pos;
  return 1;
}

static int
mulib_xmacir_close_(mulib_xmacir_reader_ *r)
{
  if (!mulib_xmacir_expect_(r, ')', "expected ')'")) return 0;
  mulib_xmacir_space_(r, 0);
  if (r->pos == r->length) return 1;
  if (mulib_xmacir_peek_(r) != '\n')
    return mulib_xmacir_report_(r->error, r->pos, "expected end of line");
  ++r->pos;
  return 1;
}

static int
mulib_xmacir_read_name_(mulib_xmacir_reader_ *r, char *out)
{
  size_t n = mulib_xmacir_word_(r);
  size_t start = r->pos - n;
  if (!mulib_xmacir_name_(r->source + start, n))
    return mulib_xmacir_report_(r->error, start, "invalid or reserved identifier");
  memcpy(out, r->source + start, n);
  out[n] = '\0';
  return 1;
}

static int
mulib_xmacir_decimal_(mulib_xmacir_reader_ *r, unsigned long *out)
{
  unsigned long value = 0;
  unsigned char c;
  mulib_xmacir_space_(r, 0);
  if (!mulib_xmacir_digit_(mulib_xmacir_peek_(r)))
    return mulib_xmacir_report_(r->error, r->pos, "expected a decimal U32 literal");
  while (mulib_xmacir_digit_(c = mulib_xmacir_peek_(r))) {
    unsigned long digit = (unsigned long)(c - '0');
    if (value > (4294967295UL - digit) / 10)
      return mulib_xmacir_report_(r->error, r->pos, "U32 literal out of range");
    value = value * 10 + digit;
    ++r->pos;
  }
  *out = value;
  return 1;
}

static int
mulib_xmacir_hex_(unsigned char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

/* First pass validates and counts without allocating. The second pass fills
 * the exact arrays, using the same scanner on unchanged input. */
static int
mulib_xmacir_read_(mulib_xmacir_reader_ *r, mulib_xmacir_program_t *program)
{
  size_t start, n;
  unsigned depth = 0;
  mulib_xmacir_space_(r, 1);
  if (r->pos == r->length)
    return mulib_xmacir_report_(r->error, r->pos, "missing IR header");
  start = r->pos;
  n = mulib_xmacir_word_(r);
  if (mulib_xmacir_equal_(r->source + start, n, "XM_U32"))
    program->mode = MULIB_XMACIR_U32;
  else if (mulib_xmacir_equal_(r->source + start, n, "XM_BLOB"))
    program->mode = MULIB_XMACIR_BLOB;
  else return mulib_xmacir_report_(r->error, start, "expected XM_U32 or XM_BLOB");

  if (!mulib_xmacir_expect_(r, '(', "expected '('") ||
      !mulib_xmacir_read_name_(r, program->entry)) return 0;
  if (program->mode == MULIB_XMACIR_BLOB) {
    if (!mulib_xmacir_expect_(r, ',', "expected ','") ||
        !mulib_xmacir_read_name_(r, program->bridge)) return 0;
    if (strcmp(program->entry, program->bridge) == 0)
      return mulib_xmacir_report_(r->error, r->pos - strlen(program->bridge),
                                  "BLOB entry and bridge must differ");
    if (!mulib_xmacir_expect_(r, ',', "expected ','") ||
        !mulib_xmacir_read_name_(r, program->context_tag) ||
        !mulib_xmacir_expect_(r, ',', "expected ','") ||
        !mulib_xmacir_read_name_(r, program->error_tag)) return 0;
  }
  if (!mulib_xmacir_close_(r)) return 0;

  program->count = program->length = 0;
  for (;;) {
    mulib_xmacir_space_(r, 1);
    start = r->pos;
    if (start == r->length)
      return mulib_xmacir_report_(r->error, start, program->mode == MULIB_XMACIR_U32 ?
                                  "missing XM_RET" : "missing XM_END");
    n = mulib_xmacir_word_(r);
    if (program->mode == MULIB_XMACIR_U32) {
      mulib_xmacir_instruction_t instruction;
      const char *problem;
      unsigned op;
      for (op = 0; op <= MULIB_XMACIR_RET; ++op)
        if (mulib_xmacir_equal_(r->source + start, n, mulib_xmacir_ops_[op].name)) break;
      if (op > MULIB_XMACIR_RET)
        return mulib_xmacir_report_(r->error, start, "unknown U32 operation");
      if (program->count >= MULIB_XMACIR_MAX_INSTRUCTIONS ||
          program->count >= (size_t)-1 / sizeof(instruction))
        return mulib_xmacir_report_(r->error, start, "U32 instruction limit reached");
      instruction.opcode = (mulib_xmacir_opcode_t)op;
      instruction.value = 0;
      if (!mulib_xmacir_expect_(r, '(', "expected '('") ||
          (op == MULIB_XMACIR_PUSH && !mulib_xmacir_decimal_(r, &instruction.value)) ||
          !mulib_xmacir_close_(r)) return 0;
      problem = mulib_xmacir_step_(&instruction, &depth);
      if (problem) return mulib_xmacir_report_(r->error, start, problem);
      if (program->instructions) program->instructions[program->count] = instruction;
      ++program->count;
      if (op == MULIB_XMACIR_RET) break;
    } else {
      int high, low;
      unsigned char chunk[MULIB_XMACIR_BYTES_PER_LINE];
      size_t k = 0;
      if (mulib_xmacir_equal_(r->source + start, n, "XM_END")) {
        if (!mulib_xmacir_expect_(r, '(', "expected '('") || !mulib_xmacir_close_(r)) return 0;
        break;
      }
      if (!mulib_xmacir_equal_(r->source + start, n, "XM_BYTES"))
        return mulib_xmacir_report_(r->error, start, "unknown BLOB operation");
      if (!mulib_xmacir_expect_(r, '(', "expected '('") ||
          !mulib_xmacir_expect_(r, 'h', "expected 'h' prefix")) return 0;
      while ((high = mulib_xmacir_hex_(mulib_xmacir_peek_(r))) >= 0) {
        if (k == MULIB_XMACIR_BYTES_PER_LINE)
          return mulib_xmacir_report_(r->error, r->pos, "BLOB chunk exceeds 32 bytes");
        ++r->pos;
        low = mulib_xmacir_hex_(mulib_xmacir_peek_(r));
        if (low < 0)
          return mulib_xmacir_report_(r->error, r->pos, "expected paired lowercase hex digits");
        ++r->pos;
        chunk[k++] = (unsigned char)(high * 16 + low);
      }
      if (!mulib_xmacir_close_(r)) return 0;
      if (k > MULIB_XMACIR_MAX_BYTES - program->length || k > (size_t)-1 - program->length)
        return mulib_xmacir_report_(r->error, start, "BLOB byte limit reached");
      if (k && program->bytes) memcpy(program->bytes + program->length, chunk, k);
      program->length += k;
    }
  }
  mulib_xmacir_space_(r, 1);
  if (r->pos != r->length)
    return mulib_xmacir_report_(r->error, r->pos, "content follows terminal call");
  return 1;
}

void
mulib_xmacir_free(mulib_xmacir_program_t *program)
{
  if (program) {
    mulib_xmacir_program_t empty = { (mulib_xmacir_mode_t)0, "", "", "", "", NULL, 0, NULL, 0 };
    free(program->instructions);
    free(program->bytes);
    *program = empty;
  }
}

int
mulib_xmacir_parse(const char *source, size_t length, mulib_xmacir_program_t *out,
                   mulib_xmacir_error_t *error)
{
  mulib_xmacir_program_t program = { (mulib_xmacir_mode_t)0, "", "", "", "", NULL, 0, NULL, 0 };
  mulib_xmacir_reader_ r;
  if (!out || (!source && length))
    return mulib_xmacir_report_(error, 0, "invalid source or output");
  if (length > MULIB_XMACIR_MAX_TEXT)
    return mulib_xmacir_report_(error, MULIB_XMACIR_MAX_TEXT, "IR text limit reached");
  r.source = source;
  r.length = length;
  r.pos = 0;
  r.error = error;
  if (!mulib_xmacir_read_(&r, &program)) return 0;
  if (program.count) {
    program.instructions = (mulib_xmacir_instruction_t *)
      MULIB_XMACIR_MALLOC_(program.count * sizeof(*program.instructions));
    if (!program.instructions)
      return mulib_xmacir_report_(error, 0, "unable to allocate U32 instructions");
  }
  if (program.length) {
    program.bytes = (unsigned char *)MULIB_XMACIR_MALLOC_(program.length);
    if (!program.bytes) {
      mulib_xmacir_free(&program);
      return mulib_xmacir_report_(error, 0, "unable to allocate BLOB bytes");
    }
  }
  if (program.instructions || program.bytes) {
    r.pos = 0;
    if (!mulib_xmacir_read_(&r, &program)) {
      mulib_xmacir_free(&program);
      return 0;
    }
  }
  *out = program;
  return mulib_xmacir_report_(error, 0, NULL);
}

typedef struct {
  char *out;
  size_t length;
  int overflow;
} mulib_xmacir_writer_;

static void
mulib_xmacir_write_(mulib_xmacir_writer_ *w, const char *text, size_t length)
{
  if (w->overflow || length > (size_t)-1 - w->length) {
    w->overflow = 1;
    return;
  }
  if (w->out) memcpy(w->out + w->length, text, length);
  w->length += length;
}

static void
mulib_xmacir_string_(mulib_xmacir_writer_ *w, const char *text)
{
  mulib_xmacir_write_(w, text, strlen(text));
}

static void
mulib_xmacir_render_(const mulib_xmacir_program_t *program, mulib_xmacir_writer_ *w)
{
  static const char hex[] = "0123456789abcdef";
  size_t i;
  mulib_xmacir_string_(w, program->mode == MULIB_XMACIR_U32 ? "XM_U32(" : "XM_BLOB(");
  mulib_xmacir_string_(w, program->entry);
  if (program->mode == MULIB_XMACIR_BLOB) {
    mulib_xmacir_string_(w, ",");
    mulib_xmacir_string_(w, program->bridge);
    mulib_xmacir_string_(w, ",");
    mulib_xmacir_string_(w, program->context_tag);
    mulib_xmacir_string_(w, ",");
    mulib_xmacir_string_(w, program->error_tag);
  }
  mulib_xmacir_string_(w, ")\n");

  if (program->mode == MULIB_XMACIR_U32) {
    for (i = 0; i < program->count; ++i) {
      const mulib_xmacir_instruction_t *instruction = &program->instructions[i];
      mulib_xmacir_string_(w, mulib_xmacir_ops_[instruction->opcode].name);
      mulib_xmacir_string_(w, "(");
      if (instruction->opcode == MULIB_XMACIR_PUSH) {
        char digits[10];
        size_t at = sizeof(digits);
        unsigned long value = instruction->value;
        do {
          digits[--at] = (char)('0' + value % 10);
          value /= 10;
        } while (value);
        mulib_xmacir_write_(w, digits + at, sizeof(digits) - at);
      }
      mulib_xmacir_string_(w, ")\n");
    }
  } else {
    i = 0;
    while (i < program->length) {
      char digits[2 * MULIB_XMACIR_BYTES_PER_LINE];
      size_t at = 0;
      while (i < program->length && at < sizeof(digits)) {
        unsigned char byte = program->bytes[i++];
        digits[at++] = hex[byte >> 4];
        digits[at++] = hex[byte & 15];
      }
      mulib_xmacir_string_(w, "XM_BYTES(h");
      mulib_xmacir_write_(w, digits, at);
      mulib_xmacir_string_(w, ")\n");
    }
    mulib_xmacir_string_(w, "XM_END()\n");
  }
}

int
mulib_xmacir_format(const mulib_xmacir_program_t *program, char **out, size_t *length,
                    mulib_xmacir_error_t *error)
{
  mulib_xmacir_writer_ w = { NULL, 0, 0 };
  if (!out || !length) return mulib_xmacir_report_(error, 0, "invalid format outputs");
  if (!mulib_xmacir_validate(program, error)) return 0;
  mulib_xmacir_render_(program, &w);
  if (w.overflow || w.length == (size_t)-1)
    return mulib_xmacir_report_(error, 0, "IR output size overflow");
  w.out = (char *)MULIB_XMACIR_MALLOC_(w.length + 1);
  if (!w.out) return mulib_xmacir_report_(error, 0, "unable to allocate IR output");
  w.length = 0;
  mulib_xmacir_render_(program, &w);
  w.out[w.length] = '\0';
  *out = w.out;
  *length = w.length;
  return mulib_xmacir_report_(error, 0, NULL);
}
#endif /* MULIB_IMPL_XMACIR */
