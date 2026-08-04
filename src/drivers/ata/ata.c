#include "ata.h"

int ata_wait_ready() {
	int timeout = 1000000; // Arbitrary large timeout to prevent infinite loops
    // Wait while the BSY bit (bit 7, 0x80) is 1
    while ((inportb(0x1F7) & 0xC0) != 0x40) {
        if (--timeout == 0) {
            return 0; // Drive time out
        }
    }
	return 1; // Drive is ready
}

void ata_read_sector(uint32_t lba, uint8_t *buffer) {
    // 1. WAIT FIRST! Do not touch ports if the drive is busy.
    if (!ata_wait_ready()) {
		// Handle timeout error
        print("Error: ATA drive is not responding or is busy.\n");
		return;
    }

    // 2. Select Drive and LBA
    outportb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outportb(0x1F2, 1);
    outportb(0x1F3, (uint8_t)(lba & 0xFF));
    outportb(0x1F4, (uint8_t)((lba >> 8) & 0xFF));
    outportb(0x1F5, (uint8_t)((lba >> 16) & 0xFF));

    // 3. Send Read Command
    outportb(0x1F7, 0x20);

    // 4. Wait for the drive to say "Data is Ready" (DRQ bit 3)
    while ((inportb(0x1F7) & 0x88) != 0x08);

    // 5. Read the data
    uint16_t* target = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        target[i] = inportw(0x1F0);
    }
}

void ata_write_sector(uint32_t lba, const uint8_t* buffer) {
    // WAIT FIRST! 
    if (!ata_wait_ready()) {
		// Handle timeout error
        print("Error: ATA drive is not responding or is busy.\n");
		return;
    }

    // Select Drive and LBA
    outportb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outportb(0x1F2, 1);
    outportb(0x1F3, (uint8_t)(lba & 0xFF));
    outportb(0x1F4, (uint8_t)((lba >> 8) & 0xFF));
    outportb(0x1F5, (uint8_t)((lba >> 16) & 0xFF));

    // Send Write Command
    outportb(0x1F7, 0x30);

    // Wait for the drive to say "Ready for Data" (DRQ bit 3)
    while ((inportb(0x1F7) & 0x88) != 0x08);

    // Send the data
    const uint16_t* source = (const uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        outportw(0x1F0, source[i]);
    }

    // Flush cache to metal
    outportb(0x1F7, 0xE7);

    // Wait for the flush to finish so we don't leave the drive busy!
    ata_wait_ready();
}
