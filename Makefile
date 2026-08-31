# =============================================================================
# BenmoshOS Core Makefile
# =============================================================================

# --- Variables ---
ASM = nasm
CC = gcc
LD = ld

# Directories
SRC_DIR = src
BUILD_DIR = build

BOOT_SRC = $(SRC_DIR)/bootloader/boot.asm
ENTRY_SRC = $(SRC_DIR)/bootloader/entry.asm
KERNEL_SRC = $(SRC_DIR)/kernel/kernel.c
IDT_SRC = $(SRC_DIR)/cpu/idt/idt.c
PIC_SRC = $(SRC_DIR)/cpu/pic/pic.c
ATA_SRC = $(SRC_DIR)/drivers/ata/ata.c
INTERRUPTS_SRC = $(SRC_DIR)/cpu/interrupts.asm
PRINT_SRC = $(SRC_DIR)/drivers/print/print.c
KEYBOARD_SRC = $(SRC_DIR)/drivers/keyboard/keyboard.c
SYSTEM_SRC = $(SRC_DIR)/kernel/system.c
TSS_SRC = $(SRC_DIR)/kernel/tss.c
SHELL_SRC = $(SRC_DIR)/shell/shell.c
FAT16_SRC = $(SRC_DIR)/fs/fat16.c
PMM_SRC = $(SRC_DIR)/memory/pmm/pmm.c
VMM_SRC = $(SRC_DIR)/memory/vmm/vmm.c
HEAP_SRC = $(SRC_DIR)/memory/heap/heap.c
STRING_SRC = $(SRC_DIR)/libc/strings.c
STDIO_SRC = $(SRC_DIR)/libc/stdio.c
STDLIB_SRC = $(SRC_DIR)/libc/stdlib.c

# --- Flags ---
INCLUDE_FLAGS = -I$(SRC_DIR)/drivers -I$(SRC_DIR)/cpu -I$(SRC_DIR)/kernel -I$(SRC_DIR)/fs -I$(SRC_DIR)/shell -I$(SRC_DIR)/memory -I$(SRC_DIR)/libc
CFLAGS = -ffreestanding -m32 -g -O0 -Wall -fno-pie -masm=intel $(INCLUDE_FLAGS)
LDFLAGS = -T linker.ld -m elf_i386 --no-warn-rwx-segments --oformat binary -Map=$(BUILD_DIR)/kernel.map

# --- Output Files (placed in build dir) ---
BOOT_BIN = $(BUILD_DIR)/boot.bin
ENTRY_OBJ = $(BUILD_DIR)/entry.o
KERNEL_OBJ = $(BUILD_DIR)/kernel.o
SYSTEM_OBJ = $(BUILD_DIR)/system.o
TSS_OBJ = $(BUILD_DIR)/tss.o
IDT_OBJ = $(BUILD_DIR)/idt.o
PIC_OBJ = $(BUILD_DIR)/pic.o
ATA_OBJ = $(BUILD_DIR)/ata.o
INTERRUPTS_OBJ = $(BUILD_DIR)/interrupts.o
PRINT_OBJ = $(BUILD_DIR)/print.o
KEYBOARD_OBJ = $(BUILD_DIR)/keyboard.o
KERNEL_BIN = $(BUILD_DIR)/kernel.bin
KERNEL_ELF = $(BUILD_DIR)/kernel.elf
SHELL_OBJ = $(BUILD_DIR)/shell.o
FAT16_OBJ = $(BUILD_DIR)/fat16.o
PMM_OBJ = $(BUILD_DIR)/pmm.o
VMM_OBJ = $(BUILD_DIR)/vmm.o
HEAP_OBJ = $(BUILD_DIR)/heap.o
STRING_OBJ = $(BUILD_DIR)/string.o
STDIO_OBJ = $(BUILD_DIR)/stdio.o
STDLIB_OBJ = $(BUILD_DIR)/stdlib.o
OS_IMAGE = BenmoshOS.bin

# --- Targets ---

all: $(BUILD_DIR) $(OS_IMAGE)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BOOT_BIN): $(BOOT_SRC)
	mkdir -p $(BUILD_DIR)
	$(ASM) -f bin $(BOOT_SRC) -o $(BOOT_BIN)

$(ENTRY_OBJ): $(ENTRY_SRC)
	mkdir -p $(BUILD_DIR)
	$(ASM) -f elf32 $(ENTRY_SRC) -o $(ENTRY_OBJ)

$(INTERRUPTS_OBJ): $(INTERRUPTS_SRC)
	mkdir -p $(BUILD_DIR)
	$(ASM) -f elf32 $(INTERRUPTS_SRC) -o $(INTERRUPTS_OBJ)

$(KERNEL_OBJ): $(KERNEL_SRC)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(KERNEL_SRC) -o $(KERNEL_OBJ)

$(SYSTEM_OBJ): $(SYSTEM_SRC)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(SYSTEM_SRC) -o $(SYSTEM_OBJ)

$(IDT_OBJ): $(IDT_SRC)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(IDT_SRC) -o $(IDT_OBJ)

$(PIC_OBJ): $(PIC_SRC)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(PIC_SRC) -o $(PIC_OBJ)

$(ATA_OBJ): $(ATA_SRC)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(ATA_SRC) -o $(ATA_OBJ)

$(PRINT_OBJ): $(PRINT_SRC)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(PRINT_SRC) -o $(PRINT_OBJ)

$(KEYBOARD_OBJ): $(KEYBOARD_SRC)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(KEYBOARD_SRC) -o $(KEYBOARD_OBJ)

$(FAT16_OBJ): $(FAT16_SRC)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(FAT16_SRC) -o $(FAT16_OBJ)

$(SHELL_OBJ): $(SHELL_SRC)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(SHELL_SRC) -o $(SHELL_OBJ)

$(PMM_OBJ): $(PMM_SRC)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(PMM_SRC) -o $(PMM_OBJ)

$(VMM_OBJ): $(VMM_SRC)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(VMM_SRC) -o $(VMM_OBJ)

$(HEAP_OBJ): $(HEAP_SRC)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(HEAP_SRC) -o $(HEAP_OBJ)

$(TSS_OBJ): $(TSS_SRC)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(TSS_SRC) -o $(TSS_OBJ)

$(STRING_OBJ): $(STRING_SRC)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(STRING_SRC) -o $(STRING_OBJ)

$(STDIO_OBJ): $(STDIO_SRC)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(STDIO_SRC) -o $(STDIO_OBJ)

$(STDLIB_OBJ): $(STDLIB_SRC)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(STDLIB_SRC) -o $(STDLIB_OBJ)

$(KERNEL_BIN): $(ENTRY_OBJ) $(KERNEL_OBJ) $(SYSTEM_OBJ) $(TSS_OBJ) $(IDT_OBJ) $(INTERRUPTS_OBJ) $(PIC_OBJ) $(ATA_OBJ) $(PRINT_OBJ) $(KEYBOARD_OBJ) $(FAT16_OBJ) $(SHELL_OBJ) $(PMM_OBJ) $(VMM_OBJ) $(HEAP_OBJ) $(STDIO_OBJ) $(STRING_OBJ) $(STDLIB_OBJ)
	$(LD) $(LDFLAGS) -o $(KERNEL_BIN) $(ENTRY_OBJ) $(KERNEL_OBJ) $(SYSTEM_OBJ) $(TSS_OBJ) $(IDT_OBJ) $(INTERRUPTS_OBJ) $(PIC_OBJ) $(ATA_OBJ) $(PRINT_OBJ) $(KEYBOARD_OBJ) $(FAT16_OBJ) $(SHELL_OBJ) $(PMM_OBJ) $(VMM_OBJ) $(HEAP_OBJ) $(STDIO_OBJ) $(STRING_OBJ) $(STDLIB_OBJ)
	$(LD) -T linker.ld -m elf_i386 -o $(KERNEL_ELF) $(ENTRY_OBJ) $(KERNEL_OBJ) $(SYSTEM_OBJ) $(TSS_OBJ) $(IDT_OBJ) $(INTERRUPTS_OBJ) $(PIC_OBJ) $(ATA_OBJ) $(PRINT_OBJ) $(KEYBOARD_OBJ) $(FAT16_OBJ) $(SHELL_OBJ) $(PMM_OBJ) $(VMM_OBJ) $(HEAP_OBJ) $(STDIO_OBJ) $(STRING_OBJ) $(STDLIB_OBJ)

$(OS_IMAGE): $(BOOT_BIN) $(KERNEL_BIN)
	dd if=/dev/zero of=$(OS_IMAGE) bs=1M count=10
	mkfs.fat -F 16 -R 128 $(OS_IMAGE)
	dd if=$(BOOT_BIN) of=$(OS_IMAGE) bs=1 count=3 conv=notrunc
	dd if=$(BOOT_BIN) of=$(OS_IMAGE) bs=1 skip=62 seek=62 count=450 conv=notrunc
	dd if=$(BOOT_BIN) of=$(OS_IMAGE) bs=512 skip=1 seek=1 count=1 conv=notrunc
	dd if=$(KERNEL_BIN) of=$(OS_IMAGE) bs=512 seek=2 conv=notrunc
	mcopy -i $(OS_IMAGE) test.txt ::my_super_long_test_file.txt
	
run: all
	qemu-system-x86_64 -drive format=raw,file=$(OS_IMAGE)

qemu_debug: all
	qemu-system-i386 -drive format=raw,file=$(OS_IMAGE) -s -S

vmdk: all
	qemu-img convert -f raw -O vmdk $(OS_IMAGE) BenmoshOS.vmdk
	@echo "VMware image created: BenmoshOS.vmdk"

vdi: $(OS_IMAGE)
	qemu-img convert -f raw -O vdi $(OS_IMAGE) BenmoshOS.vdi
	@echo "VirtualBox image created: BenmoshOS.vdi"

# --- ISO Compilation Targets ---
iso: $(BOOT_BIN) $(KERNEL_BIN)
	mkdir -p $(BUILD_DIR)/iso/boot
	dd if=/dev/zero of=$(BUILD_DIR)/iso/boot/floppy.img bs=1024 count=2880
	mkfs.fat -F 16 -s 1 -R 128 $(BUILD_DIR)/iso/boot/floppy.img
	dd if=$(BOOT_BIN) of=$(BUILD_DIR)/iso/boot/floppy.img bs=1 count=3 conv=notrunc
	dd if=$(BOOT_BIN) of=$(BUILD_DIR)/iso/boot/floppy.img bs=1 skip=62 seek=62 count=450 conv=notrunc
	dd if=$(BOOT_BIN) of=$(BUILD_DIR)/iso/boot/floppy.img bs=512 skip=1 seek=1 count=1 conv=notrunc
	dd if=$(KERNEL_BIN) of=$(BUILD_DIR)/iso/boot/floppy.img bs=512 seek=2 conv=notrunc
	mcopy -i $(BUILD_DIR)/iso/boot/floppy.img test.txt ::my_super_long_test_file.txt
	genisoimage -R -J -c boot/bootcat -b boot/floppy.img -o build/os.iso $(BUILD_DIR)/iso

run_iso: iso
	qemu-system-x86_64 -cdrom build/os.iso

clean:
	rm -rf $(BUILD_DIR) $(OS_IMAGE) BenmoshOS.vmdk BenmoshOS.vdi