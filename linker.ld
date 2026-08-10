ENTRY(_start)

SECTIONS {
    . = 0x100000;

    _kernel_start = .;

    .text : {
        *(.multiboot)
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
    }

    _kernel_end = .;
}