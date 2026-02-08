CC = gcc
LD = ld

CFLAGS = -Wall -Wextra -ffreestanding -fno-stack-protector \
         -fno-builtin -fno-common -m32 -c -I src
ASFLAGS = --32
LDFLAGS = -T config/leonardos.ld -nostdlib -m elf_i386 -n

BOOT_ASM = src/boot/boot.s
KERNEL_C = src/kernel/kernel.c
VGA_C = src/drivers/vga/vga.c
KBD_C = src/drivers/keyboard/keyboard.c
SHELL_C = src/shell/shell.c

OBJ_BOOT = build/boot.o
OBJ_KERNEL = build/kernel.o
OBJ_VGA = build/vga.o
OBJ_KBD = build/keyboard.o
OBJ_SHELL = build/shell.o
OBJ_ALL = $(OBJ_BOOT) $(OBJ_KERNEL) $(OBJ_VGA) $(OBJ_KBD) $(OBJ_SHELL)

TARGET = build/kernel.elf
ISO = build/leonardos.iso
GRUB_CFG = config/grub.cfg

.PHONY: all run iso clean

all: $(TARGET)

build/boot.o: $(BOOT_ASM)
	@mkdir -p build
	as $(ASFLAGS) $< -o $@

build/kernel.o: $(KERNEL_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/vga.o: $(VGA_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/keyboard.o: $(KBD_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/shell.o: $(SHELL_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

$(TARGET): $(OBJ_ALL)
	$(LD) $(LDFLAGS) $^ -o $@

iso: $(TARGET)
	@mkdir -p build/isodir/boot/grub
	@cp $(TARGET) build/isodir/boot/kernel.elf
	@cp $(GRUB_CFG) build/isodir/boot/grub/
	@grub-mkrescue -o $(ISO) build/isodir 2>/dev/null
	@echo "ISO: $(ISO)"

run: iso
	qemu-system-x86_64 -cdrom $(ISO) -m 256

clean:
	rm -rf build/
