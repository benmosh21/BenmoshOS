; entry.asm - The entry point of our kernel

[bits 32]
section .text.entry

[extern KernelMain]       
[extern __bss_start]
[extern __bss_end]  

global gdt_start

global _start       

_start:
    ; Reload the GDT
    lgdt [gdt_descriptor] 

    ; Force the CPU to flush its Code Segment (CS) cache
    jmp 0x08:.reload_segments

    .reload_segments:
        ; Force the CPU to flush its Data Segment caches
        mov ax, 0x10
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax
        mov ss, ax

    ; --- HARDWARE DEBUG: Print 'E' (Blue background) ---
    mov byte [0xb8000], 'E'
    mov byte [0xb8001], 0x1F

    ; Calculate the size of the BSS
    mov edi, __bss_start  
    mov ecx, __bss_end    
    sub ecx, edi          
    
    ; Fill it with zeros Safely
    cld                   
    mov al, 0             
    rep stosb             

    ; --- HARDWARE DEBUG: Print 'B' (Green background) ---
    mov byte [0xb8002], 'B'
    mov byte [0xb8003], 0x2F

    ; Hand control to C
    call KernelMain

    ; Hang if KernelMain returns
    jmp $


gdt_start:

gdt_null: 
    dq 0 ; Descriptor 0 - must be zero

gdt_kernel_code: 
    dw 0xFFFF             ; Limit (15:0)
    dw 0x0000             ; Base (15:0)
    db 0x00               ; Base (23:16)
    db 10011010b          ; Access: Present(1), DPL(00), S-bit(1), Exec(1), Conforming(0), Read/Write(1), Accessed(0)
    db 11001111b          ; Flags: 4KB granularity, 32-bit segment
    db 0x00               ; Base (31:24)

gdt_kernel_data: 
    dw 0xFFFF             ; Limit (15:0)
    dw 0x0000             ; Base (15:0)
    db 0x00               ; Base (23:16)
    db 10010010b          ; Access: Present(1), DPL(00), S-bit(1), Exec(0), ExpandDown(0), Read/Write(1), Accessed(0)
    db 11001111b          ; Flags
    db 0x00               ; Base (31:24)

gdt_user_code: 
    dw 0xFFFF             ; Limit (15:0)
    dw 0x0000             ; Base (15:0)
    db 0x00               ; Base (23:16)
    db 11111010b          ; Access: Present(1), DPL(11), S-bit(1), Exec(1), Conforming(0), Read/Write(1), Accessed(0)
    db 11001111b          ; Flags
    db 0x00               ; Base (31:24)

gdt_user_data: 
    dw 0xFFFF             ; Limit (15:0)
    dw 0x0000             ; Base (15:0)
    db 0x00               ; Base (23:16)
    db 11110010b          ; Access: Present(1), DPL(11), S-bit(1), Exec(0), ExpandDown(0), Read/Write(1), Accessed(0)
    db 11001111b          ; Flags
    db 0x00               ; Base (31:24)
    
gdt_tss:
    dw 0x0067             ; Limit (15:0) - 104 bytes for TSS
    dw 0x0000             ; Base (15:0)
    db 0x00               ; Base (23:16)
    db 10001001b          ; Access: Present(1), DPL(00), S-bit(0), Exec(0), ExpandDown(0), Read/Write(1), Accessed(1)
    db 00000000b          ; Flags: 4KB granularity, 32-bit segment
    db 0x00               ; Base (31:24)
    
gdt_end:
    
gdt_descriptor:
    ; The GDT register tracking pointer loaded via the LGDT assembly instruction
    dw gdt_end - gdt_start - 1  	; Size limit of the GDT array minus 1 byte
    dd gdt_start                 	; Core 32-bit linear address pointing to gdt_start base memory
    