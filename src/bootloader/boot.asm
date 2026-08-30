; =============================================================================
; boot.asm: Two-Stage Bootloader, move to protected mode and set the GDT
; =============================================================================

[org 0x7c00]          ; BIOS loads MBR sector into RAM address 0x0000:0x7C00
[bits 16]             ; Processor initializes in 16-bit Real Mode execution

start:
    jmp BootloaderMain
    nop


; =============================================================================
; FAT16 BIOS Parameter Block (BPB) & Extended Boot Record (EBR)
; =============================================================================
oem_name:             db 'BMOS    '       ; 8-byte OEM Identifier string
bytes_per_sector:     dw 512              ; Sector size
sectors_per_cluster:  db 1                ; Sectors per allocation unit
reserved_sectors:     dw 50               ; Sectors reserved for Stage 1, Stage 2, and Kernel
fat_count:            db 2                ; Number of File Allocation Tables (Main + Backup)
root_dir_entries:     dw 512              ; Max entries in root directory
total_sectors_16:     dw 0                ; 0 means use the 32-bit count below instead
media_descriptor:     db 0xF8             ; 0xF8 = Hard Disk
sectors_per_fat:      dw 256              ; Size of each FAT structure in sectors
sectors_per_track:    dw 63               ; Disk geometry: Sectors per track
head_count:           dw 16               ; Disk geometry: Number of heads
hidden_sectors:       dd 0                ; Number of sectors preceding this partition
total_sectors_32:     dd 20480            ; Total volume size (20480 * 512 bytes = 10MB)

; --- Extended Boot Record Metadata ---
drive_number:        db 0x80             ; 0x80 = First hard drive
reserved:            db 0                ; Reserved byte (must be 0)
boot_signature:      db 0x29             ; 0x29 indicates the next three fields are present
volume_id:           dd 0x67676767       ; The volume serial number
volume_label:        db 'BenmoshOS  '    ; 11-byte volume string label
file_system_type:    db 'FAT16   '       ; 8-byte filesystem type identifier string


; =============================================================================
; Real Mode String Printing Utility
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
; =============================================================================
BootloaderMain:
    xor ax, ax          
    mov ds, ax          
    mov [boot_drive], dl
    
    mov es, ax          
    mov ss, ax          ; Set Stack Segment to 0x0000
    mov sp, 0x7c00      ; Stack grows downward from 0x7C00 (safe memory)
    cld                 ; Clear Direction Flag

    mov si, msg_hello
    call puts
    
    ; --- Disk I/O: Load Stage 2 & Kernel via Clean CHS Loop ---
    mov ax, 0x07E0      ; Destination segment (0x07E0:0x0000 maps to physical 0x7E00)
    mov es, ax
    xor bx, bx          ; Destination offset inside ES is always 0
    
    ; Setup our geometry tracking variables (Starting at Cylinder 0, Head 0, Sector 2)
    mov byte [current_cylinder], 0
    mov byte [current_head], 0
    mov byte [current_sector], 2
    mov word [sectors_left], 127   ; Read 127 sectors (~63.5KB) into memory

; read 127 sectors for the kernel to be in
.read_loop:
    mov ah, 0x02        ; BIOS Function: Read Sectors From Drive
    mov al, 1           ; Read exactly 1 sector at a time
    mov ch, [current_cylinder]
    mov dh, [current_head]
    mov cl, [current_sector]
    mov dl, [boot_drive]
    int 0x13            ; Call BIOS disk services to read the es:bx sector (the bx is 0 so it just 16*es)
    jc disk_error       ; If carry flag is set, handle system read failure
    
    ; Advance target buffer destination by 512 bytes via segment arithmetic
    mov ax, es
    add ax, 0x0020      ; 0x0020 paragraphs * 16 bytes = 512 bytes for the size of a segment
    mov es, ax
    
    ; Decrement remaining sectors loop counter
    dec word [sectors_left]
    jz .done_reading
    
    ; Increment sector index
    inc byte [current_sector]
    cmp byte [current_sector], 64  ; Max sectors per track is 63. If it hits 64, wrap around.
    jl .read_loop
    
    ; Track boundary overflow: reset sector index back to 1 and advance drive head
    mov byte [current_sector], 1
    inc byte [current_head]
    cmp byte [current_head], 16    ; Max heads is 16 (0-15). If it hits 16, wrap around.
    jl .read_loop
    
    ; Head boundary overflow: reset head index back to 0 and advance drive cylinder
    mov byte [current_head], 0
    inc byte [current_cylinder]
    jmp .read_loop

.done_reading:
    xor ax, ax
    mov es, ax
    jmp 0x7E00          ; Jump directly to the loaded Stage 2 entry space

disk_error:
    mov si, disk_error_msg
    call puts
.halt:
    jmp .halt

msg_hello:       db 'hello, world! from first section', 10, 13, 0
disk_error_msg:  db 'Error: Disk Read Failed!', 10, 13, 0

boot_drive:       db 0
current_cylinder: db 0
current_head:     db 0
current_sector:   db 0
sectors_left:     dw 0

; Pad out exactly to 446 bytes to place the mandatory MBR partition structure
times 446 -($ - $$) db 0 

; --- Clean MBR Partition Table Layout (Required for genisoimage validation) ---
db 0x80                 ; Bootable flag (Active)
db 0x01, 0x01, 0x00     ; Partition starting CHS coordinate markers
db 0x06                 ; Partition type code (FAT16)
db 0x0F, 0x3F, 0x09     ; Partition ending CHS coordinate markers
dd 1                    ; Starting LBA sector offset
dd 20479                ; Precise sector size (20480 max sectors - 1 MBR sector = 20479)

times 48 db 0           ; Zero out remaining three partition entries safely
dw 0xAA55               ; Standard MBR boot sector identifier signature

; =============================================================================
; SECTION 2 (Stage 2 Environment Initialization)
; Loaded and executed at address 0x0000:0x7E00
; =============================================================================
sect2_start:
    xor ax, ax
    mov es, ax
    mov di, 0x5000      ; Destination memory address for the E820 memory map storage
    mov ebx, 0          ; Continuation value for E820 map loop (must start at 0)
    xor bp, bp
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

gdt_null: 
    dq 0 ; Descriptor 0 - must be zero


; the following GDT entries are 8 bytes each, defining the base, limit, access rights, and flags for each segment:
; The base is split into three parts: Base (15:0), Base (23:16), and Base (31:24). The limit is also split into two parts: Limit (15:0) and the upper 4 bits of the limit in the flags byte.
; The limit of 0xFFFF and base of 0x00000000, so it will give as limit * 4KB = 4GB of addressable space, which is the maximum for a 32-bit segment.
; The access byte, the bits are MSB to LSB: Present (1), DPL (2 bits), Descriptor Type (1 for code/data), Executable (1 for code),
;      Direction/Conforming (1 for data segments, 0 for code segments), Readable/Writable (1 for readable code or writable data), Accessed (0 initially).
; The flags byte, the bits are MSB to LSB: Granularity (1 for 4KB), Size (1 for 32-bit), Long Mode (0 for 32-bit), Available (0), and the upper 4 bits of the limit.

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
    
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1      ; Limit
    dd gdt_start                    ; Base
    
; Pad Stage 2 file layout to ensure strict multi-sector alignment matching
times 512+ 512 -($ - $$) db 0