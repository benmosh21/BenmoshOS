#include "ata.h"

// Helper function to wait for drive readiness and check for errors
int ata_wait_ready() {
    int timeout = 1000000;
    while (timeout--) {
        uint8_t status = inportb(0x1F7);
        if (status & 0x01) {
            return 0; // Error bit is set
        }
        if (!(status & 0x80) && (status & 0x40)) {
            return 1; // BSY is clear and RDY is set
        }
    }
    return 0; // Timeout
}

// Helper function to wait for Data Request (DRQ)
int ata_wait_drq() {
    int timeout = 1000000;
    while (timeout--) {
        uint8_t status = inportb(0x1F7);
        if (status & 0x01) {
            return 0; // Error bit is set
        }
        if (!(status & 0x80) && (status & 0x08)) {
            return 1; // BSY is clear and DRQ is set
        }
    }
    return 0; // Timeout
}

int ata_read_sector(uint32_t lba, uint8_t *buffer) {
    // Wait for the driver to be ready
    if (!ata_wait_ready()) {
        puts("Error: ATA drive is not responding, busy, or error state.\n");
        return 1; // Failure
    }
   

    // Select Drive and LBA
    outportb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outportb(0x1F2, 1);
    outportb(0x1F3, (uint8_t)(lba & 0xFF));
    outportb(0x1F4, (uint8_t)((lba >> 8) & 0xFF));
    outportb(0x1F5, (uint8_t)((lba >> 16) & 0xFF));

    // Send Read Command
    outportb(0x1F7, 0x20);

    if (!ata_wait_drq()) {
        puts("Error: ATA read DRQ timeout or error.\n");
        return 1;
    }

    // Read the data
    uint16_t* target = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        target[i] = inportw(0x1F0);
    }

    return 0;
}

int ata_write_sector(uint32_t lba, const uint8_t* buffer) {
    // Wait for the driver to be ready
    if (!ata_wait_ready()) {
        puts("Error: ATA drive is not responding, busy, or error state.\n");
        return 1;
    }

    // Select Drive and LBA
    outportb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outportb(0x1F2, 1);
    outportb(0x1F3, (uint8_t)(lba & 0xFF));
    outportb(0x1F4, (uint8_t)((lba >> 8) & 0xFF));
    outportb(0x1F5, (uint8_t)((lba >> 16) & 0xFF));

    // Send Write Command
    outportb(0x1F7, 0x30);

    if (!ata_wait_drq()) {
        puts("Error: ATA write DRQ timeout or error.\n");
        return 1;
    }

    // Send the data
    const uint16_t* source = (const uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        outportw(0x1F0, source[i]);
    }

    // Wait for the write to complete
    if (!ata_wait_ready()) {
        puts("Error: ATA write completion timeout or error.\n");
        return 1;
    }

    // Flush cache to metal
    outportb(0x1F7, 0xE7);

    if (!ata_wait_ready()) {
        puts("Error: ATA cache flush failed.\n");
        return 1;
    }

    return 0; // Success
}
