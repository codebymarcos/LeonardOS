// LeonardOS - Shell ASCII Minimalista
// Apenas: help, clear, halt

#include "shell.h"
#include "../drivers/vga/vga.h"
#include "../drivers/keyboard/keyboard.h"

// Halt o kernel
static void halt(void) {
    vga_puts("Desligando...\n");
    asm volatile("cli");
    asm volatile("hlt");
    while (1);
}

// Exibe informações do sistema
static void sysinfo(void) {
    vga_puts("\n");
    vga_puts("  ╔═════════════════════════════════════╗\n");
    vga_puts("  ║      LeonardOS Sysinfo             ║\n");
    vga_puts("  ╚═════════════════════════════════════╝\n");
    vga_puts("\n");
    vga_puts("  OS              : LeonardOS v0.1\n");
    vga_puts("  Kernel          : LeonardOS Kernel (32-bit)\n");
    vga_puts("  Architecture    : x86 (i386)\n");
    vga_puts("  Bootloader      : GRUB (Multiboot2)\n");
    vga_puts("  Mode            : Protected Mode 32-bit\n");
    vga_puts("  Memory          : VGA Buffer (0xB8000)\n");
    vga_puts("  Resolution      : 80x25 (Text Mode)\n");
    vga_puts("  Color Depth     : 4-bit (16 colors)\n");
    vga_puts("\n");
}

// Exibe ajuda
static void help(void) {
    vga_puts("LeonardOS - Comandos:\n");
    vga_puts("  help    - exibe este texto\n");
    vga_puts("  clear   - limpa a tela\n");
    vga_puts("  sysinfo - exibe informações do sistema\n");
    vga_puts("  halt    - desliga o kernel\n");
}

// Compara strings
static int strcmp_simple(const char *a, const char *b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return *a - *b;
}

// Processa um comando
static void process_command(const char *cmd) {
    while (*cmd == ' ') cmd++;
    
    if (strcmp_simple(cmd, "help") == 0) {
        help();
    } else if (strcmp_simple(cmd, "clear") == 0) {
        vga_clear();
    } else if (strcmp_simple(cmd, "sysinfo") == 0) {
        sysinfo();
    } else if (strcmp_simple(cmd, "halt") == 0) {
        halt();
    } else if (cmd[0] != 0) {
        vga_puts("Comando desconhecido: ");
        vga_puts(cmd);
        vga_puts("\n");
    }
}

// Loop principal do shell
void shell_loop(void) {
    static char cmd_buf[256];
    
    vga_puts("LeonardOS v0.1 - Digite 'help' para ajuda\n");
    
    while (1) {
        vga_puts("> ");
        kbd_read_line(cmd_buf, sizeof(cmd_buf));
        vga_putchar('\n');
        
        if (cmd_buf[0] != 0) {
            process_command(cmd_buf);
        }
    }
}
