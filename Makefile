# =============================================================================
# BenmoshOS Core Makefile
# Professional Automated Build System
# =============================================================================

# --- Toolchain Configuration ---
ASM = nasm
CC = gcc
LD = ld

# --- Directories ---
SRC_DIR = src
BUILD_DIR = build

# --- Compiler & Linker Flags ---
# -ffreestanding: Tells GCC we are building an OS, not a user app (no standard library)
# -m32: Compile for 32-bit x86 architecture
# -fno-pie: Disables Position Independent Executable (crucial for raw kernels)
INCLUDE_FLAGS = -I$(SRC_DIR)/drivers -I$(SRC_DIR)/cpu -I$(SRC_DIR)/kernel -I$(SRC_DIR)/fs -I$(SRC_DIR)/shell -I$(SRC_DIR)/memory -I$(SRC_DIR)/klib
CFLAGS = -ffreestanding -m32 -g -O0 -Wall -fno-pie -masm=intel $(INCLUDE_FLAGS)

# LDFLAGS: Tells the linker how to arrange the OS in memory using linker.ld
LDFLAGS_BIN = -T linker.ld -m elf_i386 --no-warn-rwx-segments -z noexecstack --oformat binary -Map=$(BUILD_DIR)/kernel.map
LDFLAGS_ELF = -T linker.ld -m elf_i386 --no-warn-rwx-segments -z noexecstack

# =============================================================================
# Source Files & Object Mappings
# =============================================================================

# 1. Bootloader
BOOT_SRC = $(SRC_DIR)/bootloader/boot.asm
BOOT_BIN = $(BUILD_DIR)/boot.bin

# 2. Kernel Core Objects (Order matters: ENTRY_OBJ must be first!)
ENTRY_OBJ = $(BUILD_DIR)/entry.o
OBJS = $(ENTRY_OBJ) \
       $(BUILD_DIR)/kernel.o \
       $(BUILD_DIR)/panic.o \
       $(BUILD_DIR)/syscall.o \
       $(BUILD_DIR)/gdt.o \
       $(BUILD_DIR)/tss.o \
       $(BUILD_DIR)/task_c.o \
       $(BUILD_DIR)/task_asm.o \
       $(BUILD_DIR)/idt.o \
       $(BUILD_DIR)/pic.o \
       $(BUILD_DIR)/interrupts.o \
       $(BUILD_DIR)/ata.o \
       $(BUILD_DIR)/print.o \
       $(BUILD_DIR)/keyboard.o \
       $(BUILD_DIR)/fat16.o \
       $(BUILD_DIR)/shell.o \
       $(BUILD_DIR)/pmm.o \
       $(BUILD_DIR)/vmm.o \
       $(BUILD_DIR)/heap.o \
       $(BUILD_DIR)/string.o \
       $(BUILD_DIR)/stdio.o \
       $(BUILD_DIR)/stdlib.o

# --- Output Binaries ---
KERNEL_BIN = $(BUILD_DIR)/kernel.bin
KERNEL_ELF = $(BUILD_DIR)/kernel.elf
OS_IMAGE   = BenmoshOS.bin
ISO_IMAGE  = $(BUILD_DIR)/BenmoshOS.iso

# =============================================================================
# Build Targets
# =============================================================================

# Default target: builds the final disk image
all: $(OS_IMAGE)

# --- Bootloader (Dynamic Sector Injection) ---
$(BOOT_BIN): $(BOOT_SRC) $(KERNEL_BIN)
	@mkdir -p $(@D)
	$(eval KERNEL_SIZE=$(shell wc -c < $(KERNEL_BIN)))
	$(eval SECTORS=$(shell echo $$(( ($(KERNEL_SIZE) + 511) / 512 + 1 )) ))
	@echo "==> Bootloader: Compiling and injecting $(SECTORS) sectors for BenmoshOS"
	$(ASM) -f bin -DSECTORS_TO_LOAD=$(SECTORS) $< -o $@

# --- Kernel Binary & Debug ELF ---
$(KERNEL_BIN): $(OBJS)
	@echo "==> Linking Kernel..."
	$(LD) $(LDFLAGS_BIN) -o $@ $(OBJS)
	$(LD) $(LDFLAGS_ELF) -o $(KERNEL_ELF) $(OBJS)

# --- Assembly Compilation Rules ---
$(BUILD_DIR)/entry.o: $(SRC_DIR)/bootloader/entry.asm
	@mkdir -p $(@D)
	$(ASM) -f elf32 $< -o $@

$(BUILD_DIR)/interrupts.o: $(SRC_DIR)/cpu/interrupts.asm
	@mkdir -p $(@D)
	$(ASM) -f elf32 $< -o $@

$(BUILD_DIR)/task_asm.o: $(SRC_DIR)/kernel/task/task.asm
	@mkdir -p $(@D)
	$(ASM) -f elf32 $< -o $@

# --- C Compilation Rules ---
$(BUILD_DIR)/kernel.o:   $(SRC_DIR)/kernel/kernel.c          ; @mkdir -p $(@D); $(CC) $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/panic.o:    $(SRC_DIR)/kernel/panic/panic.c     ; @mkdir -p $(@D); $(CC) $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/syscall.o:  $(SRC_DIR)/kernel/syscall/syscall.c ; @mkdir -p $(@D); $(CC) $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/gdt.o:      $(SRC_DIR)/cpu/gdt/gdt.c            ; @mkdir -p $(@D); $(CC) $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/tss.o:      $(SRC_DIR)/kernel/tss/tss.c         ; @mkdir -p $(@D); $(CC) $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/task_c.o:   $(SRC_DIR)/kernel/task/task.c       ; @mkdir -p $(@D); $(CC) $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/idt.o:      $(SRC_DIR)/cpu/idt/idt.c            ; @mkdir -p $(@D); $(CC) $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/pic.o:      $(SRC_DIR)/cpu/pic/pic.c            ; @mkdir -p $(@D); $(CC) $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/ata.o:      $(SRC_DIR)/drivers/ata/ata.c        ; @mkdir -p $(@D); $(CC) $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/print.o:    $(SRC_DIR)/drivers/print/print.c    ; @mkdir -p $(@D); $(CC) $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/keyboard.o: $(SRC_DIR)/drivers/keyboard/keyboard.c ; @mkdir -p $(@D); $(CC) $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/fat16.o:    $(SRC_DIR)/fs/fat16.c               ; @mkdir -p $(@D); $(CC) $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/shell.o:    $(SRC_DIR)/shell/shell.c            ; @mkdir -p $(@D); $(CC) $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/pmm.o:      $(SRC_DIR)/memory/pmm/pmm.c         ; @mkdir -p $(@D); $(CC) $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/vmm.o:      $(SRC_DIR)/memory/vmm/vmm.c         ; @mkdir -p $(@D); $(CC) $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/heap.o:     $(SRC_DIR)/memory/heap/heap.c       ; @mkdir -p $(@D); $(CC) $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/string.o:   $(SRC_DIR)/klib/string.c            ; @mkdir -p $(@D); $(CC) $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/stdio.o:    $(SRC_DIR)/klib/stdio.c             ; @mkdir -p $(@D); $(CC) $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/stdlib.o:   $(SRC_DIR)/klib/stdlib.c            ; @mkdir -p $(@D); $(CC) $(CFLAGS) -c $< -o $@

# =============================================================================
# Disk Image Generation
# =============================================================================
$(OS_IMAGE): $(BOOT_BIN) $(KERNEL_BIN)
	@echo "==> Building Disk Image: $(OS_IMAGE)"
	dd if=/dev/zero of=$@ bs=1M count=10 2>/dev/null
	mkfs.fat -F 16 -R 128 $@ >/dev/null
	dd if=$(BOOT_BIN) of=$@ bs=1 count=3 conv=notrunc 2>/dev/null
	dd if=$(BOOT_BIN) of=$@ bs=1 skip=62 seek=62 count=450 conv=notrunc 2>/dev/null
	dd if=$(BOOT_BIN) of=$@ bs=512 skip=1 seek=1 count=1 conv=notrunc 2>/dev/null
	dd if=$(KERNEL_BIN) of=$@ bs=512 seek=2 conv=notrunc 2>/dev/null
	mcopy -i $@ test.txt ::my_super_long_test_file.txt

# =============================================================================
# Emulation & VM Targets
# =============================================================================
run: all
	@echo "==> Starting QEMU..."
	qemu-system-x86_64 -drive format=raw,file=$(OS_IMAGE)

qemu_debug: all
	@echo "==> Starting QEMU in Debug Mode (Waiting for GDB)..."
	qemu-system-i386 -drive format=raw,file=$(OS_IMAGE) -s -S -no-reboot -d int,cpu_reset -D qemu.log

vmdk: all
	qemu-img convert -f raw -O vmdk $(OS_IMAGE) BenmoshOS.vmdk
	@echo "==> VMware image created: BenmoshOS.vmdk"

vdi: $(OS_IMAGE)
	qemu-img convert -f raw -O vdi $(OS_IMAGE) BenmoshOS.vdi
	@echo "==> VirtualBox image created: BenmoshOS.vdi"

iso: $(OS_IMAGE)
	@echo "==> Building Bootable ISO Image..."
	@mkdir -p $(BUILD_DIR)/iso_root
	cp $(OS_IMAGE) $(BUILD_DIR)/iso_root/bootdisk.img
	genisoimage -quiet -R -J \
	    -b bootdisk.img \
	    -hard-disk-boot \
	    -c boot.cat \
	    -boot-load-size 4 \
	    -o $(ISO_IMAGE) \
	    $(BUILD_DIR)/iso_root
	@echo "==> ISO created: $(ISO_IMAGE)"

run_iso: iso
	@echo "==> Starting QEMU with CD-ROM..."
	qemu-system-x86_64 -drive format=raw,media=disk,file=$(ISO_IMAGE)

# =============================================================================
# Clean
# =============================================================================
clean:
	@echo "==> Cleaning Build Files..."
	rm -rf $(BUILD_DIR) $(OS_IMAGE) BenmoshOS.vmdk BenmoshOS.vdi qemu.log

.PHONY: all run qemu_debug vmdk vdi iso run_iso clean