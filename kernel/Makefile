# Unknown OS - Kernel Makefile
# Author: byrex <byrex.me@proton.me>

# Toolchain
CC = x86_64-elf-gcc
AS = nasm
LD = x86_64-elf-ld
OBJCOPY = x86_64-elf-objcopy

# Flags
CFLAGS = -std=gnu11 -ffreestanding -fno-pie -fno-stack-protector \
         -fno-builtin -fno-rtti -fno-exceptions \
         -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
         -mcmodel=kernel -mgeneral-regs-only \
         -Wall -Wextra -Werror -O2 -g \
         -Iinclude -Iarch/x86_64

ASFLAGS = -f elf64
LDFLAGS = -T linker.ld -nostdlib -z max-page-size=0x1000

# Directories
SRC_DIR = src
ARCH_DIR = arch/x86_64
MM_DIR = mm
SCHED_DIR = sched
DRIVERS_DIR = drivers
FS_DIR = fs
GUI_DIR = gui

# Source files
C_SRCS = $(SRC_DIR)/kernel.c \
         $(MM_DIR)/pmm.c \
         $(MM_DIR)/vmm.c \
         $(MM_DIR)/heap.c \
         $(DRIVERS_DIR)/serial.c \
         $(DRIVERS_DIR)/vga.c \
         $(DRIVERS_DIR)/fb.c \
         $(DRIVERS_DIR)/ps2.c \
         $(DRIVERS_DIR)/mouse.c \
         $(DRIVERS_DIR)/pci.c \
         $(SCHED_DIR)/task.c \
         $(SCHED_DIR)/syscalls.c \
         $(ARCH_DIR)/isr.c \
         $(ARCH_DIR)/irq.c \
         $(ARCH_DIR)/apic.c \
         $(FS_DIR)/vfs.c \
         $(FS_DIR)/tarfs.c \
         $(FS_DIR)/ramfs.c \
         $(FS_DIR)/unk_loader.c \
         $(GUI_DIR)/compositor.c \
         $(GUI_DIR)/apps.c

ASM_SRCS = $(ARCH_DIR)/start.asm \
           $(ARCH_DIR)/gdt.asm \
           $(ARCH_DIR)/idt.asm \
           $(ARCH_DIR)/paging.asm \
           $(ARCH_DIR)/context.asm

# Objects
C_OBJS = $(C_SRCS:.c=.o)
ASM_OBJS = $(ASM_SRCS:.asm=.o)
ALL_OBJS = $(C_OBJS) $(ASM_OBJS)

# Output
KERNEL = kernel.elf

# Default target
all: $(KERNEL)

# Link kernel
$(KERNEL): $(ALL_OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(ALL_OBJS)

# Compile C files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble ASM files
%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

# Clean
clean:
	rm -f $(ALL_OBJS) $(KERNEL)

# Install to boot directory
install: $(KERNEL)
	mkdir -p ../iso/boot
	cp $(KERNEL) ../iso/boot/kernel.elf

# QEMU test
qemu: $(KERNEL)
	qemu-system-x86_64 -kernel $(KERNEL) -serial stdio -display gtk

# Debug with GDB
qemu-gdb: $(KERNEL)
	qemu-system-x86_64 -kernel $(KERNEL) -serial stdio -display gtk -s -S

.PHONY: all clean install qemu qemu-gdb