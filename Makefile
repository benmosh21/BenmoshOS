# --- Variables ---
ASM = nasm
CC = gcc
LD = ld

# Directories
SRC_DIR = src
BUILD_DIR = build

# Define source paths based on the new structure
BOOT_SRC = $(SRC_DIR)/bootloader/boot.asm
ENTRY_SRC = $(SRC_DIR)/bootloader/entry.asm
KERNEL_SRC = $(SRC_DIR)/kernel/kernel.c
# Assuming idt.c is inside the idt folder you moved to cpu
IDT_SRC = $(SRC_DIR)/cpu/idt/idt.c  
INTERRUPTS_SRC = $(SRC_DIR)/cpu/interrupts.asm
PRINT_SRC = $(SRC_DIR)/drivers/print.c

# --- Flags ---
# -I flags tell GCC where to look for header files (.h)
INCLUDE_FLAGS = -I$(SRC_DIR)/drivers -I$(SRC_DIR)/cpu -I$(SRC_DIR)/kernel
CFLAGS = -ffreestanding -m32 -g -fno-pie $(INCLUDE_FLAGS)
LDFLAGS = -T linker.ld -m elf_i386 --oformat binary

# --- Output Files (placed in build dir) ---
BOOT_BIN = $(BUILD_DIR)/boot.bin
ENTRY_OBJ = $(BUILD_DIR)/entry.o
KERNEL_OBJ = $(BUILD_DIR)/kernel.o
IDT_OBJ = $(BUILD_DIR)/idt.o
INTERRUPTS_OBJ = $(BUILD_DIR)/interrupts.o
PRINT_OBJ = $(BUILD_DIR)/print.o
KERNEL_BIN = $(BUILD_DIR)/kernel.bin
OS_IMAGE = os-image.bin

# --- Targets ---

# Default target
all: $(BUILD_DIR) $(OS_IMAGE)

# Create build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# 1. Build the Bootloader
$(BOOT_BIN): $(BOOT_SRC)
	$(ASM) -f bin $(BOOT_SRC) -o $(BOOT_BIN)

# 2. Compile the Entry Assembly code
$(ENTRY_OBJ): $(ENTRY_SRC)
	$(ASM) -f elf32 $(ENTRY_SRC) -o $(ENTRY_OBJ)

# 3. Compile the Interrupts Assembly code
$(INTERRUPTS_OBJ): $(INTERRUPTS_SRC)
	$(ASM) -f elf32 $(INTERRUPTS_SRC) -o $(INTERRUPTS_OBJ)

# 4. Compile the Kernel C code
$(KERNEL_OBJ): $(KERNEL_SRC)
	$(CC) $(CFLAGS) -c $(KERNEL_SRC) -o $(KERNEL_OBJ)

# 5. Compile the IDT C code
$(IDT_OBJ): $(IDT_SRC)
	$(CC) $(CFLAGS) -c $(IDT_SRC) -o $(IDT_OBJ)

# 6. Compile the Print C code
$(PRINT_OBJ): $(PRINT_SRC)
	$(CC) $(CFLAGS) -c $(PRINT_SRC) -o $(PRINT_OBJ)

# 7. Link Everything Together
# Note: The order of object files here matters for the linker!
$(KERNEL_BIN): $(ENTRY_OBJ) $(KERNEL_OBJ) $(IDT_OBJ) $(INTERRUPTS_OBJ) $(PRINT_OBJ)
	$(LD) $(LDFLAGS) -o $(KERNEL_BIN) $(ENTRY_OBJ) $(KERNEL_OBJ) $(IDT_OBJ) $(INTERRUPTS_OBJ) $(PRINT_OBJ)

# 8. Glue them together into one OS Image
$(OS_IMAGE): $(BOOT_BIN) $(KERNEL_BIN)
	cat $(BOOT_BIN) $(KERNEL_BIN) > $(OS_IMAGE)
	dd if=/dev/zero of=$(OS_IMAGE) bs=512 count=100 oflag=append conv=notrunc

# Run the OS in QEMU
run: $(OS_IMAGE)
	qemu-system-x86_64 -drive format=raw,file=$(OS_IMAGE)

# Clean up
clean:
	rm -rf $(BUILD_DIR) $(OS_IMAGE)