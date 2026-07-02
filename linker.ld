ENTRY(_start)

SECTIONS
{
    /* Set the location counter to your starting address */
    . = 0x8000;

    .text : ALIGN(4) {
        *(.text.entry)  
        *(.text)        
        *(.text.*)      /* Catch GCC sub-sections */
    }

    .rodata : ALIGN(4) {
        *(.rodata)
        *(.rodata.*)
    }

    .data : ALIGN(4) {
        *(.data)
        *(.data.*)      
    }

    /* 5. Define uninitialized data (BSS) */
    .bss : ALIGN(4) {
        __bss_start = .;    
        *(.bss)
        *(.bss.*)
        *(COMMON)           
        __bss_end = .;      
    }

    kernel_end = .;
}