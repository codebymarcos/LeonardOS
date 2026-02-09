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
CMD_DF_C = src/commands/cmd_df.c
PMM_C = src/memory/pmm.c
VMM_C = src/memory/vmm.c
HEAP_C = src/memory/heap.c
VFS_C = src/fs/vfs.c
RAMFS_C = src/fs/ramfs.c
CMD_LS_C = src/commands/cmd_ls.c
CMD_CAT_C = src/commands/cmd_cat.c
CMD_ECHO_C = src/commands/cmd_echo.c
CMD_PWD_C = src/commands/cmd_pwd.c
CMD_CD_C = src/commands/cmd_cd.c
CMD_MKDIR_C = src/commands/cmd_mkdir.c
CMD_TOUCH_C = src/commands/cmd_touch.c
CMD_RM_C = src/commands/cmd_rm.c
CMD_CP_C = src/commands/cmd_cp.c
CMD_REBOOT_C = src/commands/cmd_reboot.c
CMD_STAT_C = src/commands/cmd_stat.c
CMD_TREE_C = src/commands/cmd_tree.c
CMD_FIND_C = src/commands/cmd_find.c
CMD_GREP_C = src/commands/cmd_grep.c
CMD_ENV_C = src/commands/cmd_env.c
CMD_WC_C = src/commands/cmd_wc.c
CMD_HEAD_C = src/commands/cmd_head.c
CMD_SOURCE_C = src/commands/cmd_source.c
CMD_KEYTEST_C = src/commands/cmd_keytest.c
CMD_IFCONFIG_C = src/commands/cmd_ifconfig.c
CMD_NETSTAT_C = src/commands/cmd_netstat.c
SCRIPT_C = src/shell/script.c
STRING_C = src/common/string.c
IDE_C = src/drivers/disk/ide.c
LEONFS_C = src/fs/leonfs.c
PCI_C = src/drivers/pci/pci.c
RTL8139_C = src/drivers/net/rtl8139.c
NET_CONFIG_C = src/net/net_config.c
ETHERNET_C = src/net/ethernet.c
ARP_C = src/net/arp.c
IPV4_C = src/net/ipv4.c
ICMP_C = src/net/icmp.c
UDP_C = src/net/udp.c
TCP_C = src/net/tcp.c
DNS_C = src/net/dns.c
HTTP_C = src/net/http.c
CMD_PING_C = src/commands/cmd_ping.c
CMD_NSLOOKUP_C = src/commands/cmd_nslookup.c
CMD_WGET_C = src/commands/cmd_wget.c
PIT_C = src/drivers/timer/pit.c
SOCKET_C = src/net/socket.c

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
OBJ_CMD_DF = build/cmd_df.o
OBJ_PMM = build/pmm.o
OBJ_VMM = build/vmm.o
OBJ_HEAP = build/heap.o
OBJ_VFS = build/vfs.o
OBJ_RAMFS = build/ramfs.o
OBJ_CMD_LS = build/cmd_ls.o
OBJ_CMD_CAT = build/cmd_cat.o
OBJ_CMD_ECHO = build/cmd_echo.o
OBJ_CMD_PWD = build/cmd_pwd.o
OBJ_CMD_CD = build/cmd_cd.o
OBJ_CMD_MKDIR = build/cmd_mkdir.o
OBJ_CMD_TOUCH = build/cmd_touch.o
OBJ_CMD_RM = build/cmd_rm.o
OBJ_CMD_CP = build/cmd_cp.o
OBJ_CMD_REBOOT = build/cmd_reboot.o
OBJ_CMD_STAT = build/cmd_stat.o
OBJ_CMD_TREE = build/cmd_tree.o
OBJ_CMD_FIND = build/cmd_find.o
OBJ_CMD_GREP = build/cmd_grep.o
OBJ_CMD_ENV = build/cmd_env.o
OBJ_CMD_WC = build/cmd_wc.o
OBJ_CMD_HEAD = build/cmd_head.o
OBJ_CMD_SOURCE = build/cmd_source.o
OBJ_CMD_KEYTEST = build/cmd_keytest.o
OBJ_CMD_IFCONFIG = build/cmd_ifconfig.o
OBJ_CMD_NETSTAT = build/cmd_netstat.o
OBJ_SCRIPT = build/script.o
OBJ_STRING = build/string.o
OBJ_IDE = build/ide.o
OBJ_LEONFS = build/leonfs.o
OBJ_PCI = build/pci.o
OBJ_RTL8139 = build/rtl8139.o
OBJ_NET_CONFIG = build/net_config.o
OBJ_ETHERNET = build/ethernet.o
OBJ_ARP = build/arp.o
OBJ_IPV4 = build/ipv4.o
OBJ_ICMP = build/icmp.o
OBJ_UDP = build/udp.o
OBJ_TCP = build/tcp.o
OBJ_DNS = build/dns.o
OBJ_HTTP = build/http.o
OBJ_CMD_PING = build/cmd_ping.o
OBJ_CMD_NSLOOKUP = build/cmd_nslookup.o
OBJ_CMD_WGET = build/cmd_wget.o
OBJ_PIT = build/pit.o
OBJ_SOCKET = build/socket.o
OBJ_ALL = $(OBJ_BOOT) $(OBJ_KERNEL) $(OBJ_VGA) $(OBJ_KBD) $(OBJ_SHELL) \
          $(OBJ_GDT) $(OBJ_GDT_FLUSH) $(OBJ_IDT) $(OBJ_ISR) $(OBJ_ISR_STUB) $(OBJ_PIC) \
          $(OBJ_CMD_COMMANDS) $(OBJ_CMD_HELP) $(OBJ_CMD_CLEAR) $(OBJ_CMD_SYSINFO) $(OBJ_CMD_HALT) \
          $(OBJ_CMD_TEST) $(OBJ_CMD_MEM) $(OBJ_CMD_DF) $(OBJ_CMD_LS) $(OBJ_CMD_CAT) $(OBJ_CMD_ECHO) \
          $(OBJ_CMD_PWD) $(OBJ_CMD_CD) $(OBJ_CMD_MKDIR) $(OBJ_CMD_TOUCH) \
          $(OBJ_CMD_RM) $(OBJ_CMD_CP) $(OBJ_CMD_REBOOT) \
          $(OBJ_CMD_STAT) $(OBJ_CMD_TREE) $(OBJ_CMD_FIND) $(OBJ_CMD_GREP) \
          $(OBJ_CMD_ENV) $(OBJ_CMD_WC) $(OBJ_CMD_HEAD) $(OBJ_CMD_SOURCE) $(OBJ_CMD_KEYTEST) \
          $(OBJ_CMD_IFCONFIG) $(OBJ_CMD_NETSTAT) \
          $(OBJ_PMM) $(OBJ_VMM) $(OBJ_HEAP) $(OBJ_VFS) $(OBJ_RAMFS) $(OBJ_STRING) \
          $(OBJ_IDE) $(OBJ_LEONFS) $(OBJ_SCRIPT) \
          $(OBJ_PCI) $(OBJ_RTL8139) $(OBJ_NET_CONFIG) \
          $(OBJ_ETHERNET) $(OBJ_ARP) $(OBJ_IPV4) $(OBJ_ICMP) \
          $(OBJ_UDP) $(OBJ_TCP) $(OBJ_DNS) $(OBJ_HTTP) \
          $(OBJ_CMD_PING) $(OBJ_CMD_NSLOOKUP) $(OBJ_CMD_WGET) \
          $(OBJ_PIT) $(OBJ_SOCKET)

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

build/cmd_df.o: $(CMD_DF_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/pmm.o: $(PMM_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/vmm.o: $(VMM_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/heap.o: $(HEAP_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/vfs.o: $(VFS_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/ramfs.o: $(RAMFS_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_ls.o: $(CMD_LS_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_cat.o: $(CMD_CAT_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_echo.o: $(CMD_ECHO_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_pwd.o: $(CMD_PWD_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_cd.o: $(CMD_CD_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_mkdir.o: $(CMD_MKDIR_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_touch.o: $(CMD_TOUCH_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_rm.o: $(CMD_RM_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_cp.o: $(CMD_CP_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_reboot.o: $(CMD_REBOOT_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_stat.o: $(CMD_STAT_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_tree.o: $(CMD_TREE_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_find.o: $(CMD_FIND_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_grep.o: $(CMD_GREP_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_env.o: $(CMD_ENV_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_wc.o: $(CMD_WC_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_head.o: $(CMD_HEAD_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_source.o: $(CMD_SOURCE_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_keytest.o: $(CMD_KEYTEST_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_ifconfig.o: $(CMD_IFCONFIG_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_netstat.o: $(CMD_NETSTAT_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/script.o: $(SCRIPT_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/string.o: $(STRING_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/ide.o: $(IDE_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/leonfs.o: $(LEONFS_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/pci.o: $(PCI_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/rtl8139.o: $(RTL8139_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/net_config.o: $(NET_CONFIG_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/ethernet.o: $(ETHERNET_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/arp.o: $(ARP_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/ipv4.o: $(IPV4_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/icmp.o: $(ICMP_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/udp.o: $(UDP_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/tcp.o: $(TCP_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/dns.o: $(DNS_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/http.o: $(HTTP_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_ping.o: $(CMD_PING_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_nslookup.o: $(CMD_NSLOOKUP_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/cmd_wget.o: $(CMD_WGET_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/pit.o: $(PIT_C)
	@mkdir -p build
	$(CC) $(CFLAGS) $< -o $@

build/socket.o: $(SOCKET_C)
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

DISK_IMG = build/disk.img

$(DISK_IMG):
	@mkdir -p build
	qemu-img create -f raw $(DISK_IMG) 254M

run: iso $(DISK_IMG)
	qemu-system-x86_64 -cdrom $(ISO) -m 256 -drive file=$(DISK_IMG),format=raw,if=ide \
		-netdev user,id=net0 -device rtl8139,netdev=net0

clean:
	rm -rf build/
