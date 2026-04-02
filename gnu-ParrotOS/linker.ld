ENTRY(_start)

SECTIONS {
    . = 0x7C00;

    .text : {
        *(.text)
    }

    .rodata : { 
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss COMMON)
    }

    . = ALIGN(0x2000);
}
