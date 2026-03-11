; entry.asm - The entry point of our kernel

[bits 32]
section .text.entry

[extern main]       
[extern __bss_start]
[extern __bss_end]  

global _start       

_start:
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