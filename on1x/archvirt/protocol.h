#ifndef ON1X_ARCHVIRT_PROTOCOL_H
#define ON1X_ARCHVIRT_PROTOCOL_H

/* The only guest device used by this profile is the DomIOS serial port.
 * FF FF quotes a literal FF; FF 01 selects stderr; FF 02 + byte exits.
 * The terminal record must be followed by a successful virtual CPU halt. */
#define ON1X_AV_ESCAPE 0xffu
#define ON1X_AV_STDERR 0x01u
#define ON1X_AV_EXIT 0x02u

#define ON1X_AV_BOOT_ADDRESS 0x10000u
#define ON1X_AV_KERNEL_BASE 0x200000u
#define ON1X_AV_KERNEL_LIMIT 0x600000u
#define ON1X_AV_SERIAL_PORT 0x3f8u
#define ON1X_AV_MEMORY_MB 40u

#endif
