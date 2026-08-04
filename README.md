# BenmoshOS
https://github.com/benmosh21/BenmoshOS/

BenmoshOS is a custom 32-bit x86 operating system built entirely from scratch. 

The core focus of this project is low-level system architecture, memory management, and hardware privilege separation. It features a custom bootloader, hardware paging, a dynamic kernel heap, and a fully functional Ring 0 to Ring 3 user-mode transition. The OS is designed to run on real hardware or in the QEMU emulator, providing a solid foundation for learning about operating system development and x86 architecture.

## Current Features

* **Custom Bootloader:** A two-stage bootloader written in raw assembly. It utilizes Extended LBA (Logical Block Addressing) via `int 0x13` to bypass legacy CHS track limits, loading the C kernel directly into memory.
* **32-bit Protected Mode:** Safely transitions the CPU from 16-bit Real Mode to 32-bit Protected Mode using a custom Global Descriptor Table (GDT).
* **Advanced Memory Management:**
  * **PMM (Physical Memory Manager):** Parses the BIOS `E820` memory map to track available hardware RAM frames.
  * **VMM (Virtual Memory Manager):** Implements x86 hardware paging, isolating kernel space from user space with specific Read/Write/User execution flags.
  * **Kernel Heap:** Dynamic memory allocation (`malloc` and `free`) for kernel data structures.
* **Hardware Interrupts:** Custom IDT (Interrupt Descriptor Table) and remapped PIC (Programmable Interrupt Controller) to handle CPU exceptions and hardware IRQs (like the keyboard).
* **VGA Text Driver:** Custom print routines with dynamic screen clearing, buffered scrolling, and color attribute mapping directly to `0xB8000`.
* **Filesystem:** Includes a FAT16 parser for reading files off the hard drive.
* **Privilege Separation & Security:** Implements a Task State Segment (TSS) and user-mode GDT entries to securely drop CPU execution from Ring 0 (Kernel Mode) to Ring 3 (User Mode).
* **System Call Interface:** A secure `int 0x80` dispatcher that allows trapped Ring 3 user programs to safely request kernel services.

## Physical Memory Map

This is the hardcoded architectural layout of BenmoshOS in RAM:

| Memory Address | Purpose |
| :--- | :--- |
| `0x00007C00` | Bootloader Stage 1 (Loaded by BIOS) |
| `0x00007E00` | Bootloader Stage 2 (Loaded via Extended LBA) |
| `0x00008000` | Kernel Entry Point (`entry.asm` and C Kernel) |
| `0x00090000` | Kernel Stack (`esp0`) |
| `0x000B8000` | VGA Text Buffer (Video Memory) |
| `0x003FF000` | VMM Scratchpad (Temporary Page Table Mapping) |
| `0x00400000` | Kernel Heap Start |



## Build Instructions

### Prerequisites
To compile and run this operating system, you will need a Linux environment (or WSL) with the following tools installed:
* `nasm` (Netwide Assembler)
* `gcc` (Specifically an `i686-elf` cross-compiler)
* `make`
* `qemu-system-i386` (For hardware emulation)
* `VBoxManage` (Optional: part of VirtualBox, used for VDI conversion)

### Build Commands

#### 1. `make run`
This is the primary development command. It performs the following steps:
* Compiles all assembly files (`.asm`) into object files or flat binaries.
* Compiles all C source files (`.c`) into `i686-elf` object files.
* Links the objects using `linker.ld` to create the final kernel binary.
* Combines the bootloader and kernel into a single disk image and launches it in **QEMU**.

#### 2. `make clean`
Essential for maintaining build integrity. This command:
* Deletes all compiled object files (`.o`) and binary files (`.bin`) in the `build/` directory.
* **Why it matters:** It ensures that when you change a header file (`.h`), the entire project is rebuilt from scratch, preventing "stale code" bugs where the compiler fails to notice a change in a shared definition.

#### 3. `make vdi`
This command prepares BenmoshOS for professional virtualization environments like VirtualBox.
* It takes the raw disk image (`benmoshos.bin`) and converts it into a **Virtual Disk Image (VDI)** format.
* **Why it matters:** Running an OS in VirtualBox allows you to test hardware features that QEMU might abstract away, such as more complex interrupt timing and stricter memory checks. It is a critical step before claiming your OS is "hardware-ready."

#### 4. `make vmdk`
This command prepares BenmoshOS for **VMware** environments.
* It converts the raw binary disk image into a **Virtual Machine Disk (VMDK)**.
* **Why it matters:** VMDK is a professional, industry-standard format. Having this target demonstrates that your OS is portable and ready to be deployed across different hypervisors, which is a key requirement for production-grade low-level software.
