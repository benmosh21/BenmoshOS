;
; interrupts.asm - The assembly code for our Interrupt Service Routines (ISRs)
; This file defines the ISRs for CPU exceptions and a common stub to handle them.
; 
%macro ISR_NOERRCODE 1
    global isr%1        ; 1. Make the label accessible to C
    isr%1:              ; 2. Define the label (e.g., isr0, isr1)
        cli             ; 3. Disable interrupts
        push byte 0     ; 4. Push dummy error code
        push byte %1    ; 5. Push interrupt number
        jmp isr_common_stub ; 6. Jump to our common handler
%endmacro

%macro ISR_ERRCODE 1
    global isr%1
    isr%1:
        ; cli is optional here if you used an Interrupt Gate in IDT
        ; No "push byte 0" here! The CPU already did it.
        push byte %1    ; Push interrupt number
        jmp isr_common_stub
%endmacro

; This file is purely assembly and will be linked with our C code.
[bits 32]
extern isr_handler

; --- ISRs for CPU Exceptions (Interrupts 0-31) ---
; Define ISRs for CPU exceptions (0-31)
ISR_NOERRCODE 0   ; Divide by Zero
ISR_NOERRCODE 1   ; Debug
ISR_NOERRCODE 2   ; Non-Maskable Interrupt
ISR_NOERRCODE 3   ; Breakpoint
ISR_NOERRCODE 4   ; Overflow
ISR_NOERRCODE 5   ; Bound Range Exceeded
ISR_NOERRCODE 6   ; Invalid Opcode
ISR_NOERRCODE 7   ; Device Not Available
ISR_ERRCODE 8   ; Double Fault
ISR_NOERRCODE 9   ; Coprocessor Segment Overrun (reserved)
ISR_ERRCODE 10   ; Invalid TSS
ISR_ERRCODE 11   ; Segment Not Present
ISR_ERRCODE 12   ; Stack-Segment Fault
ISR_ERRCODE 13   ; General Protection Fault
ISR_ERRCODE 14   ; Page Fault
ISR_NOERRCODE 15  ; (Intel reserved. Do not use.)
ISR_NOERRCODE 16  ; x87 Floating-Point Exception
ISR_ERRCODE 17  ; Alignment Check
ISR_NOERRCODE 18  ; Machine Check
ISR_NOERRCODE 19  ; SIMD Floating-Point Exception
ISR_NOERRCODE 20  ; Virtualization Exception
ISR_ERRCODE 21  ; Control Protection Exception
ISR_NOERRCODE 22  ; (Intel reserved. Do not use.)
ISR_NOERRCODE 23  ; (Intel reserved. Do not use.)
ISR_NOERRCODE 24  ; (Intel reserved. Do not use.)
ISR_NOERRCODE 25  ; (Intel reserved. Do not use.)
ISR_NOERRCODE 26  ; (Intel reserved. Do not use.)
ISR_NOERRCODE 27  ; (Intel reserved. Do not use.)
ISR_NOERRCODE 28  ; (Intel reserved. Do not use.)
ISR_NOERRCODE 29  ; (Intel reserved. Do not use.)
ISR_NOERRCODE 30  ; (Intel reserved. Do not use.)
ISR_NOERRCODE 31  ; (Intel reserved. Do not use.)

; --- IRQs (Hardware Interrupts) ---
; Master PIC (IRQ 0-7) -> Maps to IDT entries 32-39
ISR_NOERRCODE 32  ; IRQ0 - Timer
ISR_NOERRCODE 33  ; IRQ1 - Keyboard
ISR_NOERRCODE 34  ; IRQ2 - Cascade (used internally by the PIC)
ISR_NOERRCODE 35  ; IRQ3 - COM2
ISR_NOERRCODE 36  ; IRQ4 - COM1
ISR_NOERRCODE 37  ; IRQ5 - LPT2
ISR_NOERRCODE 38  ; IRQ6 - Floppy Disk
ISR_NOERRCODE 39  ; IRQ7 - LPT1

; Slave PIC (IRQ 8-15) -> Maps to IDT entries 40-47
ISR_NOERRCODE 40  ; IRQ8 - RTC
ISR_NOERRCODE 41  ; IRQ9 - Free for peripherals (legacy SCSI, NIC, etc.)
ISR_NOERRCODE 42  ; IRQ10 - Free for peripherals
ISR_NOERRCODE 43  ; IRQ11 - Free for peripherals
ISR_NOERRCODE 44  ; IRQ12 - PS/2 Mouse
ISR_NOERRCODE 45  ; IRQ13 - FPU / Coprocessor / Internal errors
ISR_NOERRCODE 46  ; IRQ14 - Primary ATA Hard Disk
ISR_NOERRCODE 47  ; IRQ15 - Secondary ATA Hard Disk

isr_common_stub:
    pushad              ; 1. Save all registers
    mov ax, ds          ; 2. Save the Data Segment (we need to switch to kernel mode)
    push eax            ;    Push DS onto the stack
    
    mov ax, 0x10        ; 3. Load the Kernel Data Segment descriptor
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call isr_handler    ; 4. Call the C handler (defined in idt.c)

    pop eax             ; 1. Restore the original Data Segment descriptor
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popad               ; 2. Restore all general-purpose registers (EAX, ECX, etc.)
    add esp, 8          ; 3. Clean up the pushed error code and ISR number
    iret                ; 4. The special "Interrupt Return" instruction
