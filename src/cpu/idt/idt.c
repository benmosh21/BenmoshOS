/*
 * idt.c - Interrupt Descriptor Table setup and dispatch
 *
 */

#include "idt.h"
#include "../ports/ports.h"
#include "../../kernel/panic/panic.h"
#include "../../kernel/task/task.h"

struct idt_entry idt[256];
struct idt_ptr   idt_ptr;
volatile uint32_t global_ticks = 0;

void set_idt_gate(int num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low  = base & 0xFFFF;
    idt[num].selector    = sel;
    idt[num].zero        = 0;
    idt[num].type_attr   = flags;
    idt[num].offset_high = (base >> 16) & 0xFFFF;
}

/* ISR stub table (defined in interrupts.asm) */
void *isr_stub_table[48] = {
    isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
    isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
    isr32, isr33, isr34, isr35, isr36, isr37, isr38, isr39,
    isr40, isr41, isr42, isr43, isr44, isr45, isr46, isr47
};

void load_idt() {
    idt_ptr.limit = sizeof(struct idt_entry) * 256 - 1;
    idt_ptr.base  = (uint32_t)&idt;

    for (int i = 0; i < 48; i++) {
        set_idt_gate(i, (uint32_t)isr_stub_table[i], 0x08, 0x8E);
    }

    /* int 0x80 syscall gate: DPL=3 (0xEE) so ring3 code can trigger it */
    set_idt_gate(0x80, (uint32_t)isr128, 0x08, 0xEE);

    // Set the IDT pointer and load it into the CPU's IDTR register using the lidt instruction.
    __asm__ volatile ("{lidt (%0) | lidt [%0]}" : : "r" (&idt_ptr));
}

void isr_handler(struct interrupt_registers regs) {
    switch (regs.int_no) {

        /* ---- CPU Exceptions ---- */
        case 0x00:
            kernel_panic("Divide by Zero Exception at EIP: 0x%x", regs.eip);
            for (;;) __asm__ volatile("hlt");

        case 0x01:
            puts("\n[EXCEPTION] #DB Debug\n");
            for (;;) __asm__ volatile("hlt");

        case 0x06:
            kernel_panic("Invalid Opcode Exception at EIP: 0x%x", regs.eip);
            for (;;) __asm__ volatile("hlt");

        case 0x08:
            kernel_panic("Double Fault! The CPU could not invoke an exception handler.");
            for (;;) __asm__ volatile("hlt");

        case 0x0D: {
            kernel_panic("#GP General Protection Fault! Error: 0x%x, EIP: 0x%x", regs.err_code, regs.eip);
            for (;;) __asm__ volatile("hlt");
        }

        case 0x0E: {
            uint32_t faulting_address;
            __asm__ volatile("{mov %%cr2, %0 | mov %0, cr2}" : "=r" (faulting_address));

            int present = regs.err_code & 0x1;       /* Bit 0: 0 = not present, 1 = protection violation */
            int write   = regs.err_code & 0x2;       /* Bit 1: 1 = write access */
            int user    = regs.err_code & 0x4;       /* Bit 2: 1 = occurred in user mode (Ring 3) */

            /* 
             * User Stack Demand Paging:
             * Stack resides in range 0x00900000 - 0x00A00000 (1MB max stack ceiling).
             * Fault must be caused by a Not-Present page (!present).
             */
            if (!present && faulting_address >= 0x00900000 && faulting_address < 0x00A00000) {
                uint32_t new_frame = pmm_alloc_frame();
                if (!new_frame) {
                    kernel_panic("Out of physical memory during stack expansion!");
                }

                /* Map the 4KB boundary containing the fault address: User + Writable (flags = 7) */
                uint32_t page_aligned_addr = faulting_address & ~0xFFF;
                vmm_map_page_current(new_frame, page_aligned_addr, 7);

                /* Diagnostic print: confirms the page fault handler caught and fixed the fault */
                printf("[#PF Handled] Allocated frame 0x%x for stack page 0x%x\n", 
                       new_frame, page_aligned_addr);

                /* Page mapped successfully - iret will re-execute the faulting instruction */
                return;
            }

            /* Unhandled / Illegal Page Fault: print details and halt */
            kernel_panic("Fatal Page Fault at 0x%x (EIP: 0x%x, Error: 0x%x [%s%s%s])", 
                         faulting_address, regs.eip, regs.err_code,
                         present ? "PROTECTION " : "NOT-PRESENT ",
                         write   ? "WRITE "      : "READ ",
                         user    ? "USER"        : "KERNEL");
            break;
        }
        
        /* ---- Hardware IRQs ---- */
        case 0x20:
            /* Timer IRQ0*/
            global_ticks++;

            pic_send_eoi(0);

            // Swap to the next task (EOI is sent uniformly below)
            task_yield();

            break;

        case 0x21:
            /* Keyboard IRQ1 */
            keyboard_handler();
            break;

        case 0x2C:
            /* PS/2 Mouse IRQ12 — consume byte to clear IRQ */
            inportb(0x60);
            break;

        default:
            /* Silently ignore unknown IRQs (many are spurious) */
            if (regs.int_no < 32) {
                puts("\n[EXCEPTION] Unhandled #");
                print_int(regs.int_no);
                puts("\n");
            }
            break;
    }

    /* Send End-Of-Interrupt to PIC for all hardware IRQs */
    if (regs.int_no >= 32 && regs.int_no < 48) {
        pic_send_eoi(regs.int_no - 32);
    }
}