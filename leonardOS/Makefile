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
GDT_C = src/cpu/gdt.c
GDT_ASM = src/cpu/gdt_flush.s
IDT_C = src/cpu/idt.c
ISR_C = src/cpu/isr.c
ISR_ASM = src/cpu/isr_stub.s
PIC_C = src/drivers/pic/pic.c
CMD_COMMANDS_C = src/commands/commands.c
CMD_HELP_C = src/commands/cmd_help.c
CMD_CLEAR_C = src/commands/cmd_clear.c
CMD_SYSINFO_C = src/commands/cmd_sysinfo.c
CMD_HALT_C = src/commands/cmd_halt.c
CMD_TEST_C = src/commands/cmd_test.c
CMD_MEM_C = src/commands/cmd_mem.c
PMM_C = src/memory/pmm.c

OBJ_BOOT = build/boot.o
OBJ_KERNEL = build/kernel.o
OBJ_VGA = build/vga.o
OBJ_KBD = build/keyboard.o
OBJ_SHELL = build/shell.o
OBJ_GDT = build/gdt.o
OBJ_GDT_FLUSH = build/gdt_flush.o
OBJ_IDT = build/idt.o
OBJ_ISR = build/isr.o
OBJ_ISR_STUB = build/isr_stub.o
OBJ_PIC = build/pic.o
OBJ_CMD_COMMANDS = build/commands.o
OBJ_CMD_HELP = build/cmd_help.o
OBJ_CMD_CLEAR = build/cmd_clear.o
OBJ_CMD_SYSINFO = build/cmd_sysinfo.o
OBJ_CMD_HALT = build/cmd_halt.o
OBJ_CMD_TEST = build/cmd_test.o
OBJ_CMD_MEM = build/cmd_mem.o
OBJ_PMM = build/pmm.o
OBJ_ALL = $(OBJ_BOOT) $(OBJ_KERNEL) $(OBJ_VGA) $(OBJ_KBD) $(OBJ_SHELL) \
          $(OBJ_GDT) $(OBJ_GDT_FLUSH) $(OBJ_IDT) $(OBJ_ISR) $(OBJ_ISR_STUB) $(OBJ_PIC) \
          $(OBJ_CMD_COMMANDS) $(OBJ_CMD_HELP) $(OBJ_CMD_CLEAR) $(OBJ_CMD_SYSINFO) $(OBJ_CMD_HALT) \
          $(OBJ_CMD_TEST) $(OBJ_CMD_MEM) $(OBJ_PMM)

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

build/gdt.o: $(GDT_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/gdt_flush.o: $(GDT_ASM)
	@mkdir -p build
	as $(ASFLAGS) $< -o $@

build/idt.o: $(IDT_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/isr.o: $(ISR_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/isr_stub.o: $(ISR_ASM)
	@mkdir -p build
	as $(ASFLAGS) $< -o $@

build/pic.o: $(PIC_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/commands.o: $(CMD_COMMANDS_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_help.o: $(CMD_HELP_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_clear.o: $(CMD_CLEAR_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_sysinfo.o: $(CMD_SYSINFO_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_halt.o: $(CMD_HALT_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_test.o: $(CMD_TEST_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_mem.o: $(CMD_MEM_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/pmm.o: $(PMM_C)
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
