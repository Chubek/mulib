#ifndef ON1X_ARCHVIRT_PROTOCOL_H
#define ON1X_ARCHVIRT_PROTOCOL_H

/* The only guest device used by this profile is the DomIOS serial port.
 * FF FF quotes a literal FF; FF 01 selects stderr; FF 02 + byte exits.
 * The terminal record must be followed by a successful virtual CPU halt. */
#define ON1X_AV_ESCAPE 0xffu
#define ON1X_AV_STDERR 0x01u
#define ON1X_AV_EXIT 0x02u
#define ON1X_AV_READ_I64 0x03u

/* An IN transaction after READ_I64 receives one validated-size decimal line,
 * or one of these non-ASCII-decimal control bytes. */
#define ON1X_AV_INPUT_EOF 0x00u
#define ON1X_AV_INPUT_INVALID 0x01u
#define ON1X_AV_INPUT_UNAVAILABLE 0x02u
#define ON1X_AV_INPUT_MAX 21u /* optional sign + 19 digits + LF */

#define ON1X_AV_BOOT_ADDRESS 0x10000u
#define ON1X_AV_KERNEL_BASE 0x200000u
#define ON1X_AV_KERNEL_LIMIT 0x600000u
#define ON1X_AV_SERIAL_PORT 0x3f8u
#define ON1X_AV_MEMORY_MB 40u

#endif
