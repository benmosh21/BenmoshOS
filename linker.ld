ENTRY(main)

SECTIONS
{
    /* 1. Set the location counter to your starting address */
    . = 0x8000;

    /* 2. Define the code section */
    .text : {
        *(.text.entry)  /* Pull main() to the front */
        *(.text)       /* All other code */
    }

    /* 3. Define initialized data */
    .data : {
        *(.data)
    }

    /* 4. Define uninitialized data (BSS) */
    .bss : {
        __bss_start = .;    /* Mark the start of BSS */
        *(.bss)
        __bss_end = .;      /* Mark the end of BSS */
    }

    /* 5. Define the end of the kernel in memory */
    kernel_end = .;
}