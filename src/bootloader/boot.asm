; 
; boot.asm - The bootloader for our OS
; This file is responsible for loading the kernel from disk and transitioning to protected mode.
;

[org 0x7c00]
[bits 16]

start:
	jmp main
	

;
; Print a string to the screen.
; Parameters:
; 	- ds:si points to string
;

puts:
	; save regusters we will modify
	push si
	push ax
	
.loop:
	lodsb		; loads next character in al
	or al, al	; verify if next character is null (if result 0 is null)
	jz .done
	
	mov ah, 0x0E
	mov bh, 0
	int 0x10
	
	jmp .loop
	
.done:
	pop ax
	pop si
	ret

;
; MAIN PROGRAM FOR SECTION 1
;

main:
	
	; setup data segments
	xor ax, ax	
	mov ds, ax
	mov es, ax
	
	; setup stack
	mov ss, ax
	mov sp, 0x7c00
	
	;print message
	mov si, msg_hello
	call puts
	
	; go to the next section
	mov bx, 0x7e00
	mov ax, 0x0250
	mov cl, 2
	xor ch, ch
	xor dh, dh
	int 0x13
	
	; IF FAILURE (Carry flag = 1), JUMP TO ERROR
	jc disk_error
	
	; IF SUCCESS (Carry flag = 0), JUMP TO SECTION 2
	jmp bx


;
; ERROR HANDLERS
;

disk_error:
	mov si, disk_error_msg
	call puts
	jmp .halt

	
.halt:
	jmp .halt

	
msg_hello: db 'hello, world!', 10, 13, 0
disk_error_msg: db 'Error: Disk Read Failed!', 10, 13, 0

times 510 -($ - $$) db 0 ; put 0 in every locatoin after the code

dw 0xAA55 ; tell the BIOS that is an OS


sect2_start:
	
	cli					  ; Disable interrupts
	lgdt [gdt_descriptor] ; Load the DGT descriptor

	extern __bss_start
	extern __bss_end
	
	mov eax, cr0    ; 1. Read the current CR0 register
	or eax, 0x1     ; 2. Set the 0th bit to 1 (keep others same)
	mov cr0, eax    ; 3. Write it back to CR0
	
	jmp 0x08:init_pm
	
	gdt_start:
		dq 0 ; 8 bytes of 0
		
	gdt_code:
		dw 0xFFFF		; Limit (bits 0-15)
		dw 0			; Base (bits 0-15) 
		db 0			; Base (bits 16-23)
		db 10011010b	; Access Byte (Present, Ring 0, Code, Exec/Read)
		db 11001111b	; Flags (4 bits) + Limit (4 bits)
		db 0			; Base (bits 24-31)
	
	
	gdt_data:
        dw 0xFFFF		; Limit (bits 0-15)
		dw 0			; Base (bits 0-15) 
		db 0			; Base (bits 16-23)
		db 10010011b ; Access Byte (Present, Ring 0, Data, Read/Write)
		db 11001111b	; Flags (4 bits) + Limit (4 bits)
		db 0			; Base (bits 24-31)
		
	gdt_end:
		
	gdt_descriptor:
		dw gdt_end - gdt_start - 1
		dd gdt_start
		
[bits 32] 	; tell the assembler we are know in 32-bits mode
init_pm:
	
	mov ax, 0x10
	mov ds, ax
	mov ss, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	
	
	mov ebp, 0x90000 ; Move stack far away to safty
	mov esp, ebp
	
	; set the start of the screen
	mov edx, 0xb8000
	
	call clear_screen_pm
	
	mov esi, msg_hello_32bits
	call print_string_pm

	mov esi, msg_hello_asm
	call print_string_pm

	call 0x8000
	
	jmp $
	
clear_screen_pm:
    pusha               ; Save all registers
    mov edx, 0xb8000    ; Start of video memory
    mov ecx, 2000       ; Loop 2000 times (80 cols * 25 rows)

.loop:
    mov byte [edx], ' ' ; Write a space (ASCII 0x20)
    mov byte [edx+1], 0x0F ; Write color (White on Black)
    
    add edx, 2          ; Move to next character cell
    dec ecx             ; Decrease counter
    jnz .loop           ; If counter is not 0, jump back to loop
    
    popa                ; Restore registers
    ret
	
print_string_pm:

	jmp .loop
	
	
.loop:
	lodsb		; loads next character in al
	or al, al	; verify if next character is null (if result 0 is null)
	jz .done
	
	mov [edx], al
	mov [edx + 1], byte 0x0f ; need to find what number is for the right color
	
	add edx, 2
	
	jmp .loop

.done
	ret
	
msg_hello_32bits: db 'we in 32 bits     '
msg_hello_asm: db 'this is from assembly'

times 512+ 512 -($ - $$) db 0 ; put 0 in every locatoin after the code