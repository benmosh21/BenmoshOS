ENTRY(_start)

SECTIONS
{
    /* 1. Set the location counter to your starting address */
    . = 0x8000;

    /* 2. Define the code section */
    .text : ALIGN(4) {
        *(.text.entry)  
        *(.text)        
        *(.text.*)      /* Catch GCC sub-sections */
    }

    /* 3. Define Read-Only Data (WHERE YOUR STRINGS LIVE!) */
    .rodata : ALIGN(4) {
        *(.rodata)
        *(.rodata.*)
    }

    /* 4. Define initialized data */
    .data : ALIGN(4) {
        *(.data)
        *(.data.*)      /* THE GOLDEN FIX: Catches current_color and pointers! */
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