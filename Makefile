# Variables
ASM = nasm
CC = gcc
LD = ld

# Flags
CFLAGS = -ffreestanding -m32 -g -fno-pie
LDFLAGS = -T linker.ld -m elf_i386 --oformat binary

# Files
BOOT_SRC = boot.asm
ENTRY_SRC = entry.asm
KERNEL_SRC = kernel.c
IDT_SRC = idt.c
INTERRUPTS_SRC = interrupts.asm
PRINT_SRC = print.c
PRINT_OBJ = print.o

BOOT_BIN = boot.bin
ENTRY_OBJ = entry.o
KERNEL_OBJ = kernel.o
IDT_OBJ = idt.o
INTERRUPTS_OBJ = interrupts.o
KERNEL_BIN = kernel.bin
OS_IMAGE = os-image.bin

# Default target
all: $(OS_IMAGE)

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
	rm -f *.bin *.o $(OS_IMAGE)