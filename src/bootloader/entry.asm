; entry.asm - The entry point of our kernel

[bits 32]
section .text.entry

[extern main]       
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

    ; 1. Calculate the size of the BSS
    mov edi, __bss_start  
    mov ecx, __bss_end    
    sub ecx, edi          
    
    ; 2. Fill it with zeros Safely
    cld                   
    mov al, 0             
    rep stosb             

    ; --- HARDWARE DEBUG: Print 'B' (Green background) ---
    mov byte [0xb8002], 'B'
    mov byte [0xb8003], 0x2F

    ; 3. Hand control to C
    call main

    ; 4. Hang if main returns
    jmp $


gdt_start: 
    dq 0 			; Null Descriptor (Selector 0x00)
    
gdt_kernel_code: 	; (Selector 0x08)
    dw 0xFFFF     	; Limit (Bits 0-15)  = 0xFFFF (Combined with Flags to reach 4GB)
    dw 0x0000     	; Base  (Bits 0-15)  = 0x0000 (Base address starts at 0x00000000)
    db 0x00         ; Base  (Bits 16-23) = 0x00	

    ; Access Byte: 10011010b (0x9A)
    ; Bit 7 (P)   = 1 -> Present in memory
    ; Bit 6-5 (Pr)= 00 -> Descriptor Privilege Level (Ring 0 - Kernel)
    ; Bit 4 (S)   = 1 -> Descriptor Type (1 = Code or Data segment)
    ; Bit 3 (E)   = 1 -> Executable flag (1 = Code segment)
    ; Bit 2 (DC)  = 0 -> Conforming bit (0 = Regular execution protection)
    ; Bit 1 (R)   = 1 -> Readable bit (1 = Code can be read as constants)
    ; Bit 0 (A)   = 0 -> Accessed bit (0 = Automatically flipped to 1 by CPU when loaded)
    db 10011010b     
                    
    ; Flags & Limit 16-19: 11001111b (0xCF)
    ; Upper 4 bits [1100b / 0xC] = Flags:
    ;   Bit 7 (G)  = 1 -> Granularity (1 = Multiplies limit by 4KB pages. 0xFFFFF * 4KB = 4GB)
    ;   Bit 6 (D)  = 1 -> Size flag (1 = 32-bit Protected Mode execution)
    ;   Bit 5 (L)  = 0 -> Long mode flag (0 = Not 64-bit mode)
    ;   Bit 4 (AV) = 0 -> Available for system software use
    ; Lower 4 bits [1111b / 0xF] = Limit (Bits 16-19):
    ;   Appends to the lower 0xFFFF to build the full 20-bit limit 0xFFFFF
    db 11001111b     
    db 0x00          ; Base  (Bits 24-31) = 0x00

gdt_kernel_data:  	; (Selector 0x10)
    dw 0xFFFF       ; Limit (Bits 0-15)  = 0xFFFF
    dw 0x0000       ; Base  (Bits 0-15)  = 0x0000
    db 0x00         ; Base  (Bits 16-23) = 0x00
    
    ; Access Byte: 10010010b (0x92)
    ; Bit 7 (P)   = 1 -> Present in memory
    ; Bit 6-5 (Pr)= 00 -> Descriptor Privilege Level (Ring 0 - Kernel)
    ; Bit 4 (S)   = 1 -> Descriptor Type (1 = Code or Data segment)
    ; Bit 3 (E)   = 0 -> Executable flag (0 = Data segment)
    ; Bit 2 (DC)  = 0 -> Direction bit (0 = Stack grows upwards / regular data segment)
    ; Bit 1 (W)   = 1 -> Writable bit (1 = Data can be written to)
    ; Bit 0 (A)   = 0 -> Accessed bit (0 = Managed by CPU)
    db 10010010b     
                    
    ; Flags & Limit 16-19: 11001111b (0xCF)
    ; Upper 4 bits [1100b / 0xC] = Flags (Granular=1, 32-bit=1, LongMode=0, Available=0)
    ; Lower 4 bits [1111b / 0xF] = Limit Bits 16-19
    db 11001111b     
    db 0x00        	; Base  (Bits 24-31) = 0x00

gdt_user_code:
    ; User Code Segment (Selector 0x18, becomes 0x1B with RPL=3)
    dw 0xFFFF       ; Limit (Bits 0-15)  = 0xFFFF
    dw 0x0000       ; Base  (Bits 0-15)  = 0x0000
    db 0x00         ; Base  (Bits 16-23) = 0x00
    
    ; Access Byte: 11111010b (0xFA)
    ; Bit 7 (P)   = 1 -> Present in memory
    ; Bit 6-5 (Pr)= 11 -> Descriptor Privilege Level (Ring 3 - User Mode)
    ; Bit 4 (S)   = 1 -> Descriptor Type (1 = Code or Data segment)
    ; Bit 3 (E)   = 1 -> Executable flag (1 = Code segment)
    ; Bit 2 (DC)  = 0 -> Conforming bit (0 = Enforce strict privilege separation)
    ; Bit 1 (R)   = 1 -> Readable bit (1 = User code can read static constants)
    ; Bit 0 (A)   = 0 -> Accessed bit
    db 11111010b     
                    
    ; Flags & Limit 16-19: 11001111b (0xCF)
    ; Upper 4 bits [1100b / 0xC] = Flags (Granular=1, 32-bit=1, LongMode=0, Available=0)
    ; Lower 4 bits [1111b / 0xF] = Limit Bits 16-19
    db 11001111b     
    db 0x00        	; Base  (Bits 24-31) = 0x00

gdt_user_data:
    ; User Data Segment (Selector 0x20, becomes 0x23 with RPL=3)
    dw 0xFFFF       ; Limit (Bits 0-15)  = 0xFFFF
    dw 0x0000       ; Base  (Bits 0-15)  = 0x0000
    db 0x00         ; Base  (Bits 16-23) = 0x00
    
    ; Access Byte: 11110010b (0xF2)
    ; Bit 7 (P)   = 1 -> Present in memory
    ; Bit 6-5 (Pr)= 11 -> Descriptor Privilege Level (Ring 3 - User Mode)
    ; Bit 4 (S)   = 1 -> Descriptor Type (1 = Code or Data segment)
    ; Bit 3 (E)   = 0 -> Executable flag (0 = Data segment)
    ; Bit 2 (DC)  = 0 -> Direction bit (0 = Normal expansion)
    ; Bit 1 (W)   = 1 -> Writable bit (1 = User space tasks can modify their data fields)
    ; Bit 0 (A)   = 0 -> Accessed bit
    db 11110010b     
                    
    ; Flags & Limit 16-19: 11001111b (0xCF)
    ; Upper 4 bits [1100b / 0xC] = Flags (Granular=1, 32-bit=1, LongMode=0, Available=0)
    ; Lower 4 bits [1111b / 0xF] = Limit Bits 16-19
    db 11001111b     
    db 0x00         ; Base  (Bits 24-31) = 0x00

gdt_tss:
    ; Task State Segment Descriptor (Selector 0x28, becomes 0x2B with RPL=3)
    ; Note: Limit is size (104) minus 1 = 103, which is 0x0067 in hexadecimal
    dw 0x0067       ; Limit (Bits 0-15)  = 0x0067
    dw 0x0000       ; Base  (Bits 0-15)  = 0x0000 (Dynamic tracking placeholder initialized in C)
    db 0x00         ; Base  (Bits 16-23) = 0x00 (Dynamic tracking placeholder initialized in C)
    
    ; Access Byte: 10001001b (0x89)
    ; Bit 7 (P)   = 1  -> Present in memory
    ; Bit 6-5 (Pr)= 00 -> Descriptor Privilege Level (Ring 0 - Must be restricted to kernel execution)
    ; Bit 4 (S)   = 0  -> Descriptor Type (0 = System Descriptor, required for TSS gates)
    ; Bit 3-0 (Ty)= 1001b (0x9) -> Type Code (0x9 = Available 32-bit Task State Segment)
    db 10001001b     
                    
    ; Flags & Limit 16-19: 00000000b (0x00)
    ; Upper 4 bits [0000b / 0x0] = Flags:
    ;   Bit 7 (G)  = 0 -> Byte Granularity (We do not want 4KB multipliers on a 104-byte structure)
    ;   Bit 6 (D)  = 0 -> Kept 0 for TSS structures
    ;   Bit 5 (L)  = 0 -> Kept 0
    ;   Bit 4 (AV) = 0 -> Kept 0
    ; Lower 4 bits [0000b / 0x0] = Limit Bits 16-19 (0x0 since total size fits under 0xFFFF)
    db 00000000b     
    db 0x00         ; Base  (Bits 24-31) = 0x00 (Dynamic tracking placeholder initialized in C)           

gdt_end:
    
gdt_descriptor:
    ; The GDT register tracking pointer loaded via the LGDT assembly instruction
    dw gdt_end - gdt_start - 1  	; Size limit of the GDT array minus 1 byte
    dd gdt_start                 	; Core 32-bit linear address pointing to gdt_start base memory
    