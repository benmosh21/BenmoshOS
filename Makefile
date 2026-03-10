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
SHELL_SRC = $(SRC_DIR)/shell/shell.c
FAT16_SRC = $(SRC_DIR)/fs/fat16.c
PMM_SRC = $(SRC_DIR)/memory/pmm/pmm.c
VMM_SRC = $(SRC_DIR)/memory/vmm/vmm.c
HEAP_SRC = $(SRC_DIR)/memory/heap/heap.c

# --- Flags ---
# -I flags tell GCC where to look for header files (.h)
INCLUDE_FLAGS = -I$(SRC_DIR)/drivers -I$(SRC_DIR)/cpu -I$(SRC_DIR)/kernel -I$(SRC_DIR)/fs -I$(SRC_DIR)/shell -I$(SRC_DIR)/memory
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
SHELL_OBJ = $(BUILD_DIR)/shell.o
FAT16_OBJ = $(BUILD_DIR)/fat16.o
PMM_OBJ = $(BUILD_DIR)/pmm.o
VMM_OBJ = $(BUILD_DIR)/vmm.o
HEAP_OBJ = $(BUILD_DIR)/heap.o
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

# Compile the FAT16 C code
$(FAT16_OBJ): $(FAT16_SRC)
	$(CC) $(CFLAGS) -c $(FAT16_SRC) -o $(FAT16_OBJ)

# Compile the Shell C code
$(SHELL_OBJ): $(SHELL_SRC)
	$(CC) $(CFLAGS) -c $(SHELL_SRC) -o $(SHELL_OBJ)

# Compile the PMM C code
$(PMM_OBJ): $(PMM_SRC)
	$(CC) $(CFLAGS) -c $(PMM_SRC) -o $(PMM_OBJ)

# Compile the VMM C code
$(VMM_OBJ): $(VMM_SRC)
	$(CC) $(CFLAGS) -c $(VMM_SRC) -o $(VMM_OBJ)

# Compile the Heap C code
$(HEAP_OBJ): $(HEAP_SRC)
	$(CC) $(CFLAGS) -c $(HEAP_SRC) -o $(HEAP_OBJ)

# Link Everything Together
# Note: The order of object files here matters for the linker!
$(KERNEL_BIN): $(ENTRY_OBJ) $(KERNEL_OBJ) $(SYSTEM_OBJ) $(IDT_OBJ) $(INTERRUPTS_OBJ) $(PIC_OBJ) $(ATA_OBJ) $(PRINT_OBJ) $(KEYBOARD_OBJ) $(FAT16_OBJ) $(SHELL_OBJ) $(PMM_OBJ) $(VMM_OBJ) $(HEAP_OBJ)
	$(LD) $(LDFLAGS) -o $(KERNEL_BIN) $(ENTRY_OBJ) $(KERNEL_OBJ) $(SYSTEM_OBJ) $(IDT_OBJ) $(INTERRUPTS_OBJ) $(PIC_OBJ) $(ATA_OBJ) $(PRINT_OBJ) $(KEYBOARD_OBJ) $(FAT16_OBJ) $(SHELL_OBJ) $(PMM_OBJ) $(VMM_OBJ) $(HEAP_OBJ)

# Glue them together into one OS Image
$(OS_IMAGE): $(BOOT_BIN) $(KERNEL_BIN)
	# Create a blank 10MB file
	dd if=/dev/zero of=$(OS_IMAGE) bs=1M count=10
	
	# Format it as a FAT16 file system, explicitly reserving 50 sectors (-R 50)
	mkfs.fat -F 16 -R 50 $(OS_IMAGE)
	
	# Inject the Bootloader Jump (First 3 bytes)
	dd if=$(BOOT_BIN) of=$(OS_IMAGE) bs=1 count=3 conv=notrunc
	
	# Inject the Bootloader Code AND the 0xAA55 signature! (450 bytes)
	dd if=$(BOOT_BIN) of=$(OS_IMAGE) bs=1 skip=62 seek=62 count=450 conv=notrunc
	
	# Inject the 32-bit switch code safely into Sector 1
	dd if=$(BOOT_BIN) of=$(OS_IMAGE) bs=512 skip=1 seek=1 count=1 conv=notrunc
	
	# Inject the C kernel directly into the reserved space at Sector 2
	dd if=$(KERNEL_BIN) of=$(OS_IMAGE) bs=512 seek=2 conv=notrunc
	
	# Copy the test file into the FAT16 file system
	mcopy -i $(OS_IMAGE) test.txt ::my_super_long_test_file.txt	

run: all
	qemu-system-x86_64 -drive format=raw,file=$(OS_IMAGE)

# --- Build for VMware ---
vmdk: all
	# Convert the raw binary image into a VMware Virtual Disk format
	qemu-img convert -f raw -O vmdk $(OS_IMAGE) BenmoshOS.vmdk
	@echo "VMware image created: BenmoshOS.vmdk"

# --- Build for VirtualBox ---
vdi: $(OS_IMAGE)
	# Convert the raw binary image into a VirtualBox Disk Image format
	qemu-img convert -f raw -O vdi $(OS_IMAGE) BenmoshOS.vdi
	@echo "VirtualBox image created: BenmoshOS.vdi"

# Clean up (updated to remove the new images)
clean:
	rm -rf $(BUILD_DIR) $(OS_IMAGE) BenmoshOS.vmdk BenmoshOS.vdi