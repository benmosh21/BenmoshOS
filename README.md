# 🖥️ BenmoshOS
> **Making an OS, but I know nothing about it.**

![Build Status](https://img.shields.io/badge/build-passing-brightgreen)
![Platform](https://img.shields.io/badge/platform-x86-blue)

## 📖 About
This is a me bored and insane making (traing very bad and failing even worse with ai) a somting that is an os (not even sure about that), but I have no idea how to do it.

I am learning as I go, documenting the process of moving from "I have no idea what I'm doing".

## 📂 Project Structure
The project is organized into the following modules:
```text
src/
├── boot/       # Bootloader (Assembly)
├── kernel/     # Core Kernel logic (C)
├── cpu/        # Interrupts and GDT/IDT (Assembly/C)
├── drivers/    # Hardware drivers (Screen, Keyboard)
└── build/      # Compiled binaries (Ignored by git)
```

## 🛠️ Prerequisites
To build and run this OS, you will need the following tools installed (Linux/WSL recommended):
* **NASM** (Assembler)
* **GCC** (C Compiler)
* **Make** (Build system)
* **QEMU** (Emulator)
* **dd** (Disk image creation)
* **mkfs.fat** (FAT filesystem creation)


## 🚀 How to Build & Run

**1. Build the OS image:**
```bash
make
```

**2. Run the OS in QEMU:**
```bash
make run
```

**3. Clean build artifacts:**
```bash 
make clean
```

## 📚 Resources & Inspiration
### 🌐 Documentation & Wikis
* **[OSDev Wiki](https://wiki.osdev.org/Main_Page)**: The most comprehensive resource available. It covers everything from GDT and IDT to memory management and file systems.

### 📖 Tutorials
* **[Bran's Kernel Development Tutorial](https://www.osdever.net/bkernel/)**: A classic guide that helps bridge the gap between a bootloader and a functional C kernel.
* **[The Little OS Book](https://littleosbook.github.io/)**: A great modern introduction to OS development concepts.
