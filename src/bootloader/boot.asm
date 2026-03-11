; boot.asm - The bootloader for our OS

[org 0x7c00]
[bits 16]

global gdt_start

start:
	jmp short main
	nop

; --- BIOS Parameters Block (BPB) ---
oem_name: 				db 'BenMOS  ' 		; 8 bytes
bytes_per_sector:		dw 512				;
sectors_per_cluster:	db 1 				;
reserved_sectors:		dw 50				; Sector 0 is for the bootloader and the rest are for the kernel
fat_count:				db 2 				; One is the primary FAT, the other is a backup
root_dir_entries: 		dw 512				; the number of entries in the root directory
total_sectors_16: 		dw 0				; we will use the 32-bite count
media_descriptor: 		db 0xF8				; 0xF8 means Hard Drive
sectors_per_fat:		dw 256				; the size of the fat
sectors_per_track: 		dw 63				;
head_count:				dw 16				;
hidden_sectors: 		dd 0				;
total_sectors_32: 		dd 20480			; The total number of sectors on the disk (10MB)

; --- Extended Boot Record (EBR) ---	
drive_number:			db 0x80				; 0x80 means Hard Drive 0
reserved:				db 0				; Must be 0
boot_signature:			db 0x29				;
volume_id:				dd 0x67676767		; Random serial (67 67 67) number
volume_label:			db 'BenmoshOS  '	; 11 bytes
file_system_type:		db 'FAT16   '		; 8 bytes


; Print a string to the screen.
puts:
	push si
	push ax
	
.loop:
	lodsb		
	or al, al	
	jz .done
	
	mov ah, 0x0E
	mov bh, 0
	int 0x10
	
	jmp .loop
	
.done:
	pop ax
	pop si
	ret


main:
	xor ax, ax	
	mov ds, ax
	mov es, ax
	
	mov ss, ax
	mov sp, 0x7c00
	
	mov si, msg_hello
	call puts
	
	; --- EXTENDED LBA READ ---
	mov ah, 0x42
	mov dl, 0x80     ; 0x80 is the First Hard Drive
	mov si, dap      ; Load the address of our Disk Address Packet
	int 0x13
	
	jc disk_error
	jmp 0x7E00


disk_error:
	mov si, disk_error_msg
	call puts
.halt:
	jmp .halt


; --- Disk Address Packet (DAP) ---
align 4
dap:
	db 0x10      ; Size of DAP (16 bytes)
	db 0         ; Unused
	dw 127       ; Number of sectors to read (120 sectors = 60 KB of kernel code)
	dw 0x7E00    ; Offset where to load it in RAM
	dw 0         ; Segment where to load it in RAM
	dq 1         ; LBA sector to start reading from (LBA 1 is immediately after the boot sector)

msg_hello: db 'hello, world!', 10, 13, 0
disk_error_msg: db 'Error: Disk Read Failed!', 10, 13, 0

times 510 -($ - $$) db 0 
dw 0xAA55 

; ==========================================
; SECTION 2 (Loaded by the LBA read)
; ==========================================
sect2_start:

	xor ax, ax
	mov es, ax
	mov di, 0x5000  
	mov ebx, 0
	call load_pmm
	
	cli					  
	lgdt [gdt_descriptor] 
	
	mov eax, cr0    
	or eax, 0x1     
	mov cr0, eax    
	
	jmp 0x08:init_pm
	
	gdt_start:
		dq 0 
		
	gdt_kernel_code:
		dw 0xFFFF		
		dw 0			
		db 0			
		db 10011010b	
		db 11001111b	
		db 0			
	
	gdt_kernel_data:
        dw 0xFFFF		
		dw 0			
		db 0			
		db 10010010b    
		db 11001111b	
		db 0			
	
	gdt_user_code:
		dw 0xFFFF		
		dw 0			
		db 0			
		db 11111010b	
		db 11001111b	
		db 0			
	
	gdt_user_data:
        dw 0xFFFF		
		dw 0			
		db 0			
		db 11110010b    
		db 11001111b	
		db 0			
	
	gdt_tss:
		dw 104          
		dw 0            
		db 0            
		db 10001001b    
		db 00000000b    
		db 0            
	
	gdt_end:
		
	gdt_descriptor:
		dw gdt_end - gdt_start - 1
		dd gdt_start

load_pmm:
	mov eax, 0xE820      
	mov edx, 0x534D4150  
	mov ecx, 24			 
	int 0x15
	
	jc .done_pmm
	cmp eax, 0x534d4150
	jne .done_pmm
	
	add di, 24
	inc bp

	cmp ebx, 0
	je .done_pmm
	jmp load_pmm
	
.done_pmm:
	mov dword [di + 8], 0
	mov dword [di + 12], 0
	ret

		
[bits 32] 	
init_pm:
	
	mov ax, 0x10
	mov ds, ax
	mov ss, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	
	mov ebp, 0x90000 
	mov esp, ebp
	
	mov edx, 0xb8000
	
	call clear_screen_pm
	
	mov esi, msg_hello_32bits
	call print_string_pm

	mov esi, msg_hello_asm
	call print_string_pm

	; JUMP DIRECTLY TO THE C KERNEL ENTRY POINT!
	jmp 0x8000 
	

clear_screen_pm:
    pusha               
    mov edx, 0xb8000    
    mov ecx, 2000       

.loop_clear:
    mov byte [edx], ' ' 
    mov byte [edx+1], 0x0F 
    
    add edx, 2          
    dec ecx             
    jnz .loop_clear     
    
    popa                
    ret
	
print_string_pm:

	jmp .loop_print
	
.loop_print:
	lodsb		
	or al, al	
	jz .done_print
	
	mov [edx], al
	mov [edx + 1], byte 0x0f 
	
	add edx, 2
	
	jmp .loop_print

.done_print:
	ret

msg_hello_32bits: db 'we in 32 bits     ', 0
msg_hello_asm: db 'this is from assembly', 0

times 512+ 512 -($ - $$) db 0