OUTPUT_FORMAT(elf64-x86-64)
ENTRY(genoa_entry)

PHDRS
{
    limine_requests PT_LOAD;
    text PT_LOAD;
    rodata PT_LOAD;
    data PT_LOAD;
}

SECTIONS
{
    . = 0xffffffff80000000;

    .limine_requests : {
        __limine_requests_start = .;
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
        __limine_requests_end = .;
    } :limine_requests

    . = ALIGN(0x1000);

    .text : {
        __text_start = .;
        *(.text .text.*)
        __text_end = .;
    } :text

    . = ALIGN(0x1000);

    .rodata : {
        __rodata_start = .;
        *(.rodata .rodata.*)
        __rodata_end = .;
    } :rodata

    . = ALIGN(0x1000);

    .data : {
        __data_start = .;
        *(.data .data.*)
        __data_end = .;
    } :data


    .bss : {
        *(.bss .bss.*)
        *(COMMON)
        data_end = .;
    } :data

    /DISCARD/ : {
        *(.eh_frame*)
        *(.note .note.*)
    }
}