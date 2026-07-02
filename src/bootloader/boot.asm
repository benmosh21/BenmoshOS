; =============================================================================
; boot.asm: Two-Stage Bootloader & Hardware Initialization Environment
; Target Architecture: x86 (IA-32) Protected Mode
; Target Boot Interface: Legacy BIOS
; =============================================================================

[org 0x7c00]          ; BIOS loads MBR sector into RAM address 0x0000:0x7C00
[bits 16]             ; Processor initializes in 16-bit Real Mode execution

start:
    jmp BootloaderMain
    nop

; =============================================================================
; FAT16 BIOS Parameter Block (BPB) & Extended Boot Record (EBR)
; Required so operating systems (like Windows/Linux) don't think the disk is corrupted.
; =============================================================================
oem_name:             db 'BMOS    '       ; 8-byte OEM Identifier string
bytes_per_sector:    dw 512              ; Standard sector size
sectors_per_cluster: db 1                ; Sectors per allocation unit
reserved_sectors:    dw 50               ; Sectors reserved for Stage 1, Stage 2, and Kernel
fat_count:           db 2                ; Number of File Allocation Tables (Main + Backup)
root_dir_entries:    dw 512              ; Max entries in root directory
total_sectors_16:    dw 0                ; 0 means use the 32-bit count below instead
media_descriptor:    db 0xF8             ; 0xF8 = Hard Disk
sectors_per_fat:     dw 256              ; Size of each FAT structure in sectors
sectors_per_track:   dw 63               ; Disk geometry: Sectors per track
head_count:          dw 16               ; Disk geometry: Number of heads
hidden_sectors:      dd 0                ; Number of sectors preceding this partition
total_sectors_32:    dd 20480            ; Total volume size (20480 * 512 bytes = 10MB)

; --- Extended Boot Record Metadata ---
drive_number:        db 0x80             ; 0x80 = First hard drive
reserved:            db 0                ; Reserved byte (must be 0)
boot_signature:      db 0x29             ; 0x29 indicates the next three fields are present
volume_id:           dd 0x67676767       ; Random volume serial number
volume_label:        db 'BenmoshOS  '    ; 11-byte volume string label
file_system_type:    db 'FAT16   '       ; 8-byte filesystem type identifier string

; =============================================================================
; Real Mode String Printing Utility
; Input: SI = Pointer to null-terminated string
; =============================================================================
puts:
    push si
    push ax
    
.loop:
    lodsb               ; Load byte at SI into AL, then increment SI
    test al, al         ; Check if AL is 0 (end of string)
    jz .done
    
    mov ah, 0x0E        ; BIOS teletype output function
    mov bh, 0           ; Video page 0
    int 0x10            ; Call BIOS video interrupt
    
    jmp .loop
    
.done:
    pop ax
    pop si
    ret

; =============================================================================
; Stage 1 Execution Entry Point
; Sets up stack/segments and loads Stage 2 from disk.
; =============================================================================
BootloaderMain:

	mov [boot_drive], dl

    xor ax, ax          
    mov ds, ax          
    mov es, ax          
    
    cld                 ; Clear Direction Flag (ensures string functions increment)

    mov ss, ax          ; Set Stack Segment to 0x0000
    mov sp, 0x7c00      ; Stack grows downward from 0x7C00 (safe memory)
    
    mov si, msg_hello
    call puts
    
    ; --- Disk I/O: Load Stage 2 via LBA ---
    mov ah, 0x42        ; BIOS function: Extended Read Sectors from Drive
    ;mov dl, 0x80        ; Drive number (0x80 = hard drive)
	mov dl, [boot_drive]
    mov si, dap         ; Provide pointer to Disk Address Packet
    int 0x13            ; Call BIOS disk interrupt
    

    jc disk_error       ; If Carry Flag is set, an error occurred
    jmp 0x7E00          ; Jump directly to where Stage 2 was loaded

disk_error:
    mov si, disk_error_msg
    call puts
.halt:
    jmp .halt

; =============================================================================
; Disk Address Packet (DAP)
; Memory structure containing the instructions for the BIOS LBA read command.
; =============================================================================
align 4
dap:
    db 0x10             ; Size of the DAP structure (always 16 bytes / 0x10)
    db 0                ; Always 0 (reserved)
    dw 127              ; Number of sectors to read (~63.5KB)
    dw 0x7E00           ; Destination offset memory pointer
    dw 0                ; Destination segment memory pointer (0x0000:0x7E00)
    dq 1                ; Starting sector number in Logical Block Addressing (LBA 1)

msg_hello:       db 'hello, world!', 10, 13, 0
disk_error_msg:  db 'Error: Disk Read Failed!', 10, 13, 0
boot_drive: db 0

; Pad out exactly to 510 bytes, then append standard MBR boot signature
times 510 -($ - $$) db 0 
dw 0xAA55 

; =============================================================================
; SECTION 2 (Stage 2 Environment Initialization)
; Loaded and executed at address 0x0000:0x7E00
; =============================================================================
sect2_start:
    xor ax, ax
    mov es, ax
    mov di, 0x5000      ; Destination memory address for the E820 memory map storage
    mov ebx, 0          ; Continuation value for E820 map loop (must start at 0)
    call load_pmm
    
    cli                 ; Clear Interrupts (disable hardware interrupts before PMODE)
    lgdt [gdt_descriptor] ; Load the address and size of the GDT into the CPU register

    mov eax, cr0        ; Read current Control Register 0
    or eax, 0x1         ; Set Bit 0 (Protection Enable bit)
    mov cr0, eax        ; Write back to CR0. We are now technically in Protected Mode.
    
    ; Far Jump to Protected Mode segment. Clears out any prefetched 16-bit real mode instructions.
    jmp 0x08:init_pm
    
; =============================================================================
; Physical Memory Mapping Engine (E820 BIOS map)
; Maps out usable RAM blocks vs reserved system hardware zones.
; =============================================================================
load_pmm:
    mov eax, 0xE820     ; Magic function code for advanced memory detection
    mov edx, 0x534D4150 ; Magic constant string validation ('SMAP')
    mov ecx, 24         ; Ask for up to 24 bytes of data per map entry
    int 0x15            ; Call BIOS system services interrupt
    
    jc .done_pmm        ; Exit loop if carry flag sets (end of list or error)
    cmp eax, 0x534d4150 ; Verify that EAX contains 'SMAP' upon return
    jne .done_pmm       
    
    add di, 24          ; Move destination memory index to next entry slot
    inc bp              ; Keep track of total entry count in BP register

    cmp ebx, 0          ; If EBX returns to 0, the map parsing is finished
    je .done_pmm        
    jmp load_pmm        ; Otherwise, request next memory block
    
.done_pmm:
    mov dword [di + 8], 0  ; Place a trailing explicit 0 mapping boundary marker
    mov dword [di + 12], 0 
    ret

; =============================================================================
; 32-Bit IA-32 Protected Mode Workspace Environment
; =============================================================================
[bits 32]               ; Assemble code below using 32-bit instructions
init_pm:
    
    mov ax, 0x10        ; 0x10 points to our GDT Data Descriptor entry (index 2)
    mov ds, ax          ; Update all data segment registers to point to the data descriptor
    mov ss, ax          ; Update stack segment to use the protected mode descriptor
    mov es, ax          
    mov fs, ax          
    mov gs, ax          

    cld                 ; Ensure string loops increment forward in 32-bit mode
    
    mov ebp, 0x90000    ; Establish protected mode execution stack base
    mov esp, ebp        ; Update active stack pointer register
    
    mov edx, 0xb8000    ; Linear address pointer targeting VGA MMIO Text Buffer
    
    call clear_screen_pm
    
    mov esi, msg_hello_32bits
    call print_string_pm

    mov esi, msg_hello_asm
    call print_string_pm

    jmp 0x8000          ; Jump directly to the loaded C kernel address space
    
; =============================================================================
; VGA Memory-Mapped I/O Screen Clearing Utility
; Loops through video memory and fills it with blank spaces.
; =============================================================================
clear_screen_pm:
    pusha               ; Save all current 32-bit registers
    mov edx, 0xb8000    ; Reset pointer to top-left of VGA text layout memory
    mov ecx, 2000       ; 80 columns * 25 rows = 2000 characters total

.loop_clear:
    mov byte [edx], ' '    ; Write space character code
    mov byte [edx+1], 0x0F ; Write text attributes (White text on black background)
    
    add edx, 2          ; Advance pointer by 2 bytes (1 byte character, 1 byte color)
    dec ecx             
    jnz .loop_clear     
    
    popa                ; Restore all registers
    ret
    
; =============================================================================
; VGA Memory-Mapped I/O Protected Mode Text Printer
; Input: ESI = Address of source string
; =============================================================================
print_string_pm:
    jmp .loop_print
    
.loop_print:
    lodsb               ; Load character byte from ESI into AL, increments ESI
    or al, al           ; Check if character is 0 (null terminator)
    jz .done_print
    
    mov [edx], al          ; Move character byte directly to current VGA window slot
    mov [edx + 1], byte 0x0f ; Affix color parameters (white on black)
    
    add edx, 2          ; Advance VGA window cursor tracking variable by two bytes
    
    jmp .loop_print

.done_print:
    ret

msg_hello_32bits: db 'we in 32 bits     ', 0
msg_hello_asm:    db 'this is from assembly', 0

; =============================================================================
; Global Descriptor Table (GDT) Specifications
; Configures flattening segments that give the CPU access to a full 4GB memory range.
; =============================================================================


gdt_start: 
    dq 0                ; Null Descriptor: Required hardware safety buffer (Selector 0x00)
    
gdt_kernel_code:        ; Code Segment Descriptor (Selector 0x08)
    dw 0xFFFF           ; Segment Limit bits 0-15 (0xFFFF means max size)
    dw 0x0000           ; Base Address bits 0-15 (Starts at memory address 0)
    db 0x00             ; Base Address bits 16-23
    db 10011010b        ; Access Byte: Present(1), Privilege(00 for Ring 0), Descriptor Type(1), Executable(1), Conforming(0), Readable(1), Accessed(0)
    db 11001111b        ; Flags/Limit: Granularity(1=4KB blocks), Size(1=32-bit PMODE), LongMode(0), Reserved(0) + Limit bits 16-19 (0xF)
    db 0x00             ; Base Address bits 24-31

gdt_kernel_data:        ; Data Segment Descriptor (Selector 0x10)
    dw 0xFFFF           ; Segment Limit bits 0-15
    dw 0x0000           ; Base Address bits 0-15
    db 0x00             ; Base Address bits 16-23
    db 10010010b        ; Access Byte: Present(1), Privilege(00), Descriptor Type(1), Executable(0), Direction(0), Writable(1), Accessed(0)
    db 11001111b        ; Flags/Limit: Granularity(1=4KB), Size(1=32-bit), LongMode(0), Reserved(0) + Limit bits 16-19 (0xF)
    db 0x00             ; Base Address bits 24-31
    
gdt_end:
    
gdt_descriptor:
    dw gdt_end - gdt_start - 1 ; Total size of the GDT array minus 1 byte
    dd gdt_start        ; Linear, physical pointer to the absolute start address of the GDT

; Pad Stage 2 file layout to ensure strict multi-sector alignment matching
times 512+ 512 -($ - $$) db 0