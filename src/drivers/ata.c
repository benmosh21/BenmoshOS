#include "ata.h"

void ata_read_sector(uint32_t lba, uint8_t *buffer) {
    // Send the highest 4 bits of the LBA to the drive select port (0x1F6)
    // We bitwise OR with 0xE0 to select the "Master" drive and enable LBA mode.
    outportb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    
    // 2. Send the sector count (1 sector) to port 0x1F2
    outportb(0x1F2, 1);
    
    // 3. Send the LBA to the low, mid, and high ports
    outportb(0x1F3, (uint8_t)(lba & 0xFF));
    outportb(0x1F4, (uint8_t)((lba >> 8) & 0xFF));
    outportb(0x1F5, (uint8_t)((lba >> 16) & 0xFF));
    
    // 4. Send the "Read Sectors" command (0x20) to the command port (0x1F7)
    outportb(0x1F7, 0x20);
    
    // 5. Poll the status port (0x1F7) until the drive is ready
    // Bit 3 (0x08) goes high when the drive has data ready to be read.
    uint8_t status;
    do {
        status = inportb(0x1F7);
    } while ((status & 0x08) == 0);
    
    // 6. Read 256 16-bit words (512 bytes) from the data port (0x1F0)
    uint16_t *target = (uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        target[i] = inportw(0x1F0);
    }
}