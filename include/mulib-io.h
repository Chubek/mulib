/** @file mulib-io.h
 * @brief Explicit opt-in x86 port I/O and descriptor-table loads.
 * Define MULIB_IMPL_IO once and MULIB_IO_ENABLE_PRIVILEGED for ring-zero code.
 * Without opt-in operations return -1 and do not execute privileged instructions.
 */
#ifndef MULIB_IO_H
#define MULIB_IO_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Read a byte from an x86 I/O port. Returns -1 if unavailable. */
int mulib_io_in8(uint16_t port, uint8_t *value);
/** @brief Write a byte to an x86 I/O port. Returns -1 if unavailable. */
int mulib_io_out8(uint16_t port, uint8_t value);
/** @brief Read/write a 32-bit port (including PCI CF8/CFC configuration ports).
 * Concurrent configuration access requires caller serialization.
 */
int mulib_io_in32(uint16_t port, uint32_t *value);
int mulib_io_out32(uint16_t port, uint32_t value);
/** @brief Load 10-byte GDTR/IDTR operand; idt must be 0 (GDT) or 1 (IDT).
 * Does not reload segment registers, CS, or TR. Requires x86-64 ring zero.
 */
int mulib_io_load_table(const uint8_t descriptor[10], unsigned idt);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_IO) && !defined(MULIB_IO_IMPLEMENTED)
#define MULIB_IO_IMPLEMENTED
#if defined(MULIB_IO_ENABLE_PRIVILEGED) && (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
#define MULIB_IO_NATIVE_ 1
#endif
int mulib_io_in32(uint16_t port, uint32_t *value) {
    if (!value) return -1;
#ifdef MULIB_IO_NATIVE_
    __asm__ volatile ("inl %1, %0" : "=a"(*value) : "Nd"(port) : "memory"); return 0;
#else
    (void)port; return -1;
#endif
}
int mulib_io_out32(uint16_t port, uint32_t value) {
#ifdef MULIB_IO_NATIVE_
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port) : "memory"); return 0;
#else
    (void)port; (void)value; return -1;
#endif
}
int mulib_io_in8(uint16_t port, uint8_t *value) {
    if (!value) return -1;
#ifdef MULIB_IO_NATIVE_
    __asm__ volatile ("inb %1, %0" : "=a"(*value) : "Nd"(port) : "memory"); return 0;
#else
    (void)port; return -1;
#endif
}
int mulib_io_out8(uint16_t port, uint8_t value) {
#ifdef MULIB_IO_NATIVE_
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port) : "memory"); return 0;
#else
    (void)port; (void)value; return -1;
#endif
}
int mulib_io_load_table(const uint8_t d[10], unsigned idt) {
    if (!d || idt > 1) return -1;
#if defined(MULIB_IO_NATIVE_) && defined(__x86_64__)
    if (idt) { __asm__ volatile ("lidt (%0)" : : "r"(d) : "memory"); }
    else { __asm__ volatile ("lgdt (%0)" : : "r"(d) : "memory"); }
    return 0;
#else
    return -1;
#endif
}
#endif
