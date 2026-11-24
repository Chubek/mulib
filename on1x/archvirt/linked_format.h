#ifndef ON1X_ARCHVIRT_LINKED_FORMAT_H
#define ON1X_ARCHVIRT_LINKED_FORMAT_H

/* Native launcher ELF followed by guest ELF bytes followed by a v1 footer.
 * ELF loaders ignore the overlay. The bytewise footer avoids native padding
 * and makes the format independent of the C compiler used for the launcher. */
#define ON1X_VM_FOOTER_MAGIC "ON1XVM1!"
#define ON1X_VM_FOOTER_MAGIC_SIZE 8u
#define ON1X_VM_FOOTER_POLICY 8u
#define ON1X_VM_FOOTER_LENGTH 9u
#define ON1X_VM_FOOTER_SIZE 17u
#define ON1X_VM_GUEST_MAX (8u * 1024u * 1024u)
#define ON1X_VM_LAUNCHER_MAX (32u * 1024u * 1024u)

enum { ON1X_VM_ALWAYS = 1, ON1X_VM_OPT_IN = 2 };

#endif
