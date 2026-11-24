#include <archvirt_boot.h>
#include <on1x_archvirt_guest.h>
#include <on1x_runtime.h>
#include <string.h>

#include "protocol.h"

_Static_assert(sizeof(struct ArchVirtBootInfo) == 96, "DomIOS v1 boot ABI size");
_Static_assert(offsetof(struct ArchVirtBootInfo, serial_base) == 72, "DomIOS v1 serial ABI offset");

int main(void);

static void serial(uint8_t byte)
{
    __asm__ volatile("outb %0, %w1" : : "a"(byte), "d"(ON1X_AV_SERIAL_PORT));
}

static void put(uint8_t byte)
{
    serial(byte);
    if (byte == ON1X_AV_ESCAPE) serial(byte);
}

static void text(const char *s)
{
    if (s) while (*s) put((uint8_t)*s++);
}

static void number(uint64_t a)
{
    uint8_t digits[20];
    size_t count = 0;
    do {
        digits[count++] = (uint8_t)('0' + a % 10);
        a /= 10;
    } while (a);
    while (count) put(digits[--count]);
}

static _Noreturn void finish(int status)
{
    serial(ON1X_AV_ESCAPE);
    serial(ON1X_AV_EXIT);
    serial((uint8_t)status);
    __asm__ volatile("hlt");
    __builtin_unreachable();
}

static void select_stderr(void)
{
    serial(ON1X_AV_ESCAPE);
    serial(ON1X_AV_STDERR);
}

static uint8_t serial_in(void)
{
    uint8_t byte;
    __asm__ volatile("inb %w1, %0" : "=a"(byte) : "d"(ON1X_AV_SERIAL_PORT));
    return byte;
}

static _Noreturn void invalid_parameter(void)
{
    select_stderr();
    text("On1x ArchVirt: invalid i64 parameter\n");
    finish(70);
}

int64_t on1x_archvirt_read_i64(void)
{
    serial(ON1X_AV_ESCAPE);
    serial(ON1X_AV_READ_I64);
    uint8_t c = serial_in();
    if (c == ON1X_AV_INPUT_EOF) finish(0);
    if (c == ON1X_AV_INPUT_UNAVAILABLE) {
        select_stderr();
        text("On1x ArchVirt: parameter input requires interactive launch\n");
        finish(125);
    }
    bool negative = c == '-';
    if (negative || c == '+') c = serial_in();
    uint64_t n = 0, limit = (uint64_t)INT64_MAX + negative;
    unsigned count = 0;
    while (c != '\n') {
        if (c < '0' || c > '9' || ++count > 19 || n > (limit - (c - '0')) / 10)
            invalid_parameter();
        n = n * 10 + (c - '0');
        c = serial_in();
    }
    if (!count) invalid_parameter();
    if (!negative) return (int64_t)n;
    return n == (uint64_t)INT64_MAX + 1 ? INT64_MIN : -(int64_t)n;
}

static _Noreturn void reject_environment(void)
{
    select_stderr();
    text("On1x ArchVirt: capturing closure environments are unavailable\n");
    finish(125);
}

/* The checked C11 emitter represents named/noncapturing callables with a
 * null environment and emits uniform cleanup on them. A guest that allocates
 * captured environments cannot link: there is no on1x_env_alloc here. */
void on1x_env_retain(on1x_env *environment)
{
    if (environment) reject_environment();
}

void on1x_env_release(on1x_env *environment)
{
    if (environment) reject_environment();
}

_Noreturn void on1x_guest_boot(const struct ArchVirtBootInfo *boot)
{
    if ((uintptr_t)boot != ON1X_AV_BOOT_ADDRESS ||
        boot->magic != ARCHVIRT_BOOT_MAGIC || boot->version != 1 ||
        boot->architecture != 62 || boot->flags != 1 ||
        boot->memory_size != ON1X_AV_MEMORY_MB * UINT64_C(1024) * 1024 ||
        boot->serial_base != ON1X_AV_SERIAL_PORT || boot->image_size || boot->cmdline_size) {
        select_stderr();
        text("On1x ArchVirt: incompatible DomIOS boot information\n");
        finish(125);
    }
    finish(main());
}

static _Noreturn void arithmetic_error(const char *message)
{
    select_stderr();
    text("On1x arithmetic error: ");
    text(message);
    put('\n');
    finish(70);
}

int64_t on1x_add_i64(int64_t a, int64_t b)
{
    int64_t result;
    if (__builtin_add_overflow(a, b, &result)) arithmetic_error("addition overflow");
    return result;
}

int64_t on1x_sub_i64(int64_t a, int64_t b)
{
    int64_t result;
    if (__builtin_sub_overflow(a, b, &result)) arithmetic_error("subtraction overflow");
    return result;
}

int64_t on1x_mul_i64(int64_t a, int64_t b)
{
    int64_t result;
    if (__builtin_mul_overflow(a, b, &result)) arithmetic_error("multiplication overflow");
    return result;
}

int64_t on1x_div_i64(int64_t a, int64_t b)
{
    if (!b || (a == INT64_MIN && b == -1)) arithmetic_error("invalid division");
    return a / b;
}

int64_t on1x_mod_i64(int64_t a, int64_t b)
{
    if (!b || (a == INT64_MIN && b == -1)) arithmetic_error("invalid modulo");
    return a % b;
}

int64_t on1x_neg_i64(int64_t a)
{
    if (a == INT64_MIN) arithmetic_error("negation overflow");
    return -a;
}

void on1x_print_i64(int64_t a)
{
    uint64_t magnitude = (uint64_t)a;
    if (a < 0) {
        put('-');
        magnitude = 0 - magnitude;
    }
    number(magnitude);
    put('\n');
}

void on1x_print_bool(bool a) { on1x_print_string(a ? "true" : "false"); }
void on1x_print_string(const char *a) { text(a); put('\n'); }

void on1x_assert(bool a)
{
    if (a) return;
    select_stderr();
    text("On1x assertion failed\n");
    finish(70);
}

static _Noreturn void value_error(const char *message, const char *file, size_t line)
{
    select_stderr();
    text(file);
    put(':');
    number(line);
    text(": On1x value error: ");
    text(message);
    put('\n');
    finish(70);
}

void on1x_bounds_index(int64_t index, int64_t length, const char *file, size_t line)
{
    if (index < 0 || index >= length) value_error("index out of bounds", file, line);
}

void on1x_bounds_slice(int64_t lo, int64_t hi, int64_t length, const char *file, size_t line)
{
    if (lo < 0 || hi < lo || hi > length) value_error("slice bounds out of range", file, line);
}

void on1x_invalid_tag(const char *file, size_t line)
{
    value_error("invalid sum discriminant", file, line);
}

void *memset(void *p, int c, size_t n)
{
    unsigned char *dst = p;
    for (size_t i = 0; i < n; ++i) dst[i] = (unsigned char)c;
    return p;
}

void *memcpy(void *restrict p, const void *restrict q, size_t n)
{
    unsigned char *dst = p;
    const unsigned char *src = q;
    for (size_t i = 0; i < n; ++i) dst[i] = src[i];
    return p;
}

void *memmove(void *p, const void *q, size_t n)
{
    unsigned char *dst = p;
    const unsigned char *src = q;
    if ((uintptr_t)dst <= (uintptr_t)src) {
        for (size_t i = 0; i < n; ++i) dst[i] = src[i];
    } else {
        while (n) { --n; dst[n] = src[n]; }
    }
    return p;
}

int strcmp(const char *a, const char *b)
{
    const unsigned char *left = (const unsigned char *)a, *right = (const unsigned char *)b;
    while (*left && *left == *right) { ++left; ++right; }
    return (int)*left - (int)*right;
}
