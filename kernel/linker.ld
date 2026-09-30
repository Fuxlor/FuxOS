ENTRY(kmain)

SECTIONS
{
    . = 0x10000;

    .text   : { *(.text.kmain) *(.text*) }
    .rodata : { *(.rodata*) }
    .data   : { *(.data*) }

    .bss : {
        __bss_start = .;
        *(COMMON)
        *(.bss)
        *(.bss*)
        __bss_end = .;
    }

    /DISCARD/ : { *(.eh_frame) *(.comment) *(.note*) }
}