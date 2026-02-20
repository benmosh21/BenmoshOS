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
ATA_SRC = $(SRC_DIR)/drivers/ata.c
INTERRUPTS_SRC = $(SRC_DIR)/cpu/interrupts.asm
PRINT_SRC = $(SRC_DIR)/drivers/print/print.c
KEYBOARD_SRC = $(SRC_DIR)/drivers/keyboard/keyboard.c
SYSTEM_SRC = $(SRC_DIR)/kernel/system.c

# --- Flags ---
# -I flags tell GCC where to look for header files (.h)
INCLUDE_FLAGS = -I$(SRC_DIR)/drivers -I$(SRC_DIR)/cpu -I$(SRC_DIR)/kernel
CFLAGS = -ffreestanding -m32 -g -fno-pie $(INCLUDE_FLAGS)
LDFLAGS = -T linker.ld -m elf_i386 --oformat binary

# --- Output Files (placed in build dir) ---
BOOT_BIN = $(BUILD_DIR)/boot.bin
ENTRY_OBJ = $(BUILD_DIR)/entry.o
KERNEL_OBJ = $(BUILD_DIR)/kernel.o
SYSTEM_OBJ = $(BUILD_DIR)/system.o
IDT_OBJ = $(BUILD_DIR)/idt.o
PIC_OBJ = $(BUILD_DIR)/pic.o
ATA_OBJ = $(BUILD_DIR)/ata.o
INTERRUPTS_OBJ = $(BUILD_DIR)/interrupts.o
PRINT_OBJ = $(BUILD_DIR)/print.o
KEYBOARD_OBJ = $(BUILD_DIR)/keyboard.o
KERNEL_BIN = $(BUILD_DIR)/kernel.bin
OS_IMAGE = os-image.bin

# --- Targets ---

# Default target
all: $(BUILD_DIR) $(OS_IMAGE)

# Create build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build the Bootloader
$(BOOT_BIN): $(BOOT_SRC)
	$(ASM) -f bin $(BOOT_SRC) -o $(BOOT_BIN)

# Compile the Entry Assembly code
$(ENTRY_OBJ): $(ENTRY_SRC)
	$(ASM) -f elf32 $(ENTRY_SRC) -o $(ENTRY_OBJ)

#  Compile the Interrupts Assembly code
$(INTERRUPTS_OBJ): $(INTERRUPTS_SRC)
	$(ASM) -f elf32 $(INTERRUPTS_SRC) -o $(INTERRUPTS_OBJ)

#  Compile the Kernel C code
$(KERNEL_OBJ): $(KERNEL_SRC)
	$(CC) $(CFLAGS) -c $(KERNEL_SRC) -o $(KERNEL_OBJ)

#  Compile the System C code
$(SYSTEM_OBJ): $(SYSTEM_SRC)
	$(CC) $(CFLAGS) -c $(SYSTEM_SRC) -o $(SYSTEM_OBJ)

#  Compile the IDT C code
$(IDT_OBJ): $(IDT_SRC)
	$(CC) $(CFLAGS) -c $(IDT_SRC) -o $(IDT_OBJ)

# Compile the PIC C code
$(PIC_OBJ): $(PIC_SRC)
	$(CC) $(CFLAGS) -c $(PIC_SRC) -o $(PIC_OBJ)

# Compile the ATA C code
$(ATA_OBJ): $(ATA_SRC)
	$(CC) $(CFLAGS) -c $(ATA_SRC) -o $(ATA_OBJ)

# Compile the Print C code
$(PRINT_OBJ): $(PRINT_SRC)
	$(CC) $(CFLAGS) -c $(PRINT_SRC) -o $(PRINT_OBJ)

# Compile the Keyboard C code
$(KEYBOARD_OBJ): $(KEYBOARD_SRC)
	$(CC) $(CFLAGS) -c $(KEYBOARD_SRC) -o $(KEYBOARD_OBJ)

# Link Everything Together
# Note: The order of object files here matters for the linker!
$(KERNEL_BIN): $(ENTRY_OBJ) $(KERNEL_OBJ) $(SYSTEM_OBJ) $(IDT_OBJ) $(INTERRUPTS_OBJ) $(PIC_OBJ) $(ATA_OBJ) $(PRINT_OBJ) $(KEYBOARD_OBJ)
	$(LD) $(LDFLAGS) -o $(KERNEL_BIN) $(ENTRY_OBJ) $(KERNEL_OBJ) $(SYSTEM_OBJ) $(IDT_OBJ) $(INTERRUPTS_OBJ) $(PIC_OBJ) $(ATA_OBJ) $(PRINT_OBJ) $(KEYBOARD_OBJ)

# Glue them together into one OS Image
$(OS_IMAGE): $(BOOT_BIN) $(KERNEL_BIN)
	# Concatenate the bootloader (Sectors 0 and 1) and the kernel (Sector 2+)
	cat $(BOOT_BIN) $(KERNEL_BIN) > $(OS_IMAGE)
	
	# Pad the disk with empty space so QEMU recognizes it as a hard drive (10MB)
	dd if=/dev/zero bs=512 count=20480 >> $(OS_IMAGE)
# Run the OS in QEMU
run: all
	qemu-system-x86_64 -drive format=raw,file=$(OS_IMAGE)

# Clean up
clean:
	rm -rf $(BUILD_DIR) $(OS_IMAGE)