/* Pinned ArchVirt x86_64 DomIOS-v1 physical address space. */
ENTRY(_start)
PHDRS {
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}
SECTIONS {
    . = 0x200000;
    .text : { KEEP(*(.text.boot)) *(.text .text.*) *(.rodata .rodata.*) } :text
    . = ALIGN(4096);
    .data : { *(.data .data.*) } :data
    .bss : { *(.bss .bss.*) *(COMMON) } :data
    ASSERT(. <= 0x600000, "On1x DomIOS guest exceeds the kernel address range")
    /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
}
