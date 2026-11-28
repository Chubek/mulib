.section .text.boot,"ax",@progbits
.global _start
.type _start,@function
_start:
    cld
    mov $0x700000, %rsp
    /* RDI is the guest-physical pointer to the DomIOS boot information. */
    call on1x_guest_boot
    ud2
.size _start, .-_start
.section .note.GNU-stack,"",@progbits
