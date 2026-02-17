[bits 32]
[extern main]       ; We need to call main
[extern __bss_start]; Linker symbol
[extern __bss_end]  ; Linker symbol

global _start       ; We want the linker to know this is the entry point

_start:
    ; 1. Calculate the size of the BSS
    mov edi, __bss_start  ; Point EDI to the start of BSS
    mov ecx, __bss_end    ; Put the end address in ECX
    sub ecx, edi          ; ECX = End - Start (this is the size)
    
    ; 2. Fill it with zeros
    mov al, 0             ; The value to write (0)
    rep stosb             ; Repeat "Store String Byte" ECX times

    ; 3. Hand control to C
    call main

    ; 4. Hang if main returns
    jmp $