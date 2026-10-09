file build/kernel.elf
target remote :1234
set disassembly-flavor intel
set architecture i386
break KernelMain