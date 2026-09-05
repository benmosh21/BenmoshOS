#include "pic.h"

// PIC 1 (Master) ports
#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21

// PIC 2 (Slave) ports
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1

// Initialization Control Word (ICW) commands
#define ICW1_INIT 0x11
#define ICW4_8086 0x01

// Wait for I/O operation to complete
#define io_wait() outportb(0x80, 0)

// Remap the PIC to avoid conflicts with CPU exceptions
void pic_remap() {
    unsigned char a1, a2;

    // Save masks
    a1 = inportb(PIC1_DATA);
    a2 = inportb(PIC2_DATA);

    // Start initialization (ICW1)
    outportb(PIC1_COMMAND, ICW1_INIT | ICW1_INIT);
    io_wait();
    outportb(PIC2_COMMAND, ICW1_INIT | ICW1_INIT);
    io_wait();

    // Set vector offsets (ICW2)
    // Master PIC starts at 0x20 (32)
    outportb(PIC1_DATA, 0x20);
    io_wait();
    // Slave PIC starts at 0x28 (40)
    outportb(PIC2_DATA, 0x28);
    io_wait();

    // Tell Master PIC about Slave at IRQ2 (0000 0100) (ICW3)
    outportb(PIC1_DATA, 0x04);
    io_wait();
    // Tell Slave PIC its cascade identity (0000 0010) (ICW3)
    outportb(PIC2_DATA, 0x02);
    io_wait();

    // Set PICs to 8086 mode (ICW4)
    outportb(PIC1_DATA, ICW4_8086);
    io_wait();
    outportb(PIC2_DATA, ICW4_8086);
    io_wait();

    // Restore saved masks
    outportb(PIC1_DATA, 0xFD); // 1111 1101 - Enable only IRQ1 (keyboard)
    io_wait();
    outportb(PIC2_DATA, 0xFF); // 1111 1111 - Disable all IRQs on Slave
    io_wait();
}

// Send End of Interrupt (EOI) signal to PICs
void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outportb(PIC2_COMMAND, 0x20); // Send EOI to Slave PIC
        io_wait();
    }
    outportb(PIC1_COMMAND, 0x20); // Send EOI to Master PIC
    io_wait();
}