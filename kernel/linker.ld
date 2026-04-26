ENTRY(_start)
SECTIONS
{
    . = 0x100000;
    kernel_start = .;

    .text : {
        *(.text)
    }
    . = ALIGN(4096);

    .rodata : {
        *(.rodata)
    }
    . = ALIGN(4096);

    .data : {
        *(.data)
    }
    . = ALIGN(4096);

    .bss : {
        bss_start = .;
        *(.bss)
        *(COMMON)
    }
    . = ALIGN(4096);

    kernel_end = .;
}