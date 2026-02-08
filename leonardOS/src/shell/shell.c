// LeonardOS - Shell com cores
// Comandos: help, clear, sysinfo, halt

#include "shell.h"
#include "../drivers/vga/vga.h"
#include "../drivers/keyboard/keyboard.h"
#include "../common/colors.h"

// Halt o kernel
static void halt(void) {
    vga_puts_color("Desligando...\n", THEME_WARNING);
    asm volatile("cli");
    asm volatile("hlt");
    while (1);
}

// Exibe informações do sistema
static void sysinfo(void) {
    vga_puts("\n");
    vga_puts_color("  ╔═════════════════════════════════════╗\n", THEME_BORDER);
    vga_puts_color("  ║", THEME_BORDER);
    vga_puts_color("      LeonardOS Sysinfo             ", THEME_TITLE);
    vga_puts_color(" ║\n", THEME_BORDER);
    vga_puts_color("  ╚═════════════════════════════════════╝\n", THEME_BORDER);
    vga_puts("\n");

    vga_puts_color("  OS              ", THEME_LABEL);
    vga_puts_color(": ", THEME_DIM);
    vga_puts_color("LeonardOS v0.1\n", THEME_VALUE);

    vga_puts_color("  Kernel          ", THEME_LABEL);
    vga_puts_color(": ", THEME_DIM);
    vga_puts_color("LeonardOS Kernel (32-bit)\n", THEME_VALUE);

    vga_puts_color("  Architecture    ", THEME_LABEL);
    vga_puts_color(": ", THEME_DIM);
    vga_puts_color("x86 (i386)\n", THEME_VALUE);

    vga_puts_color("  Bootloader      ", THEME_LABEL);
    vga_puts_color(": ", THEME_DIM);
    vga_puts_color("GRUB (Multiboot2)\n", THEME_VALUE);

    vga_puts_color("  Mode            ", THEME_LABEL);
    vga_puts_color(": ", THEME_DIM);
    vga_puts_color("Protected Mode 32-bit\n", THEME_VALUE);

    vga_puts_color("  Memory          ", THEME_LABEL);
    vga_puts_color(": ", THEME_DIM);
    vga_puts_color("VGA Buffer (0xB8000)\n", THEME_VALUE);

    vga_puts_color("  Resolution      ", THEME_LABEL);
    vga_puts_color(": ", THEME_DIM);
    vga_puts_color("80x25 (Text Mode)\n", THEME_VALUE);

    vga_puts_color("  Color Depth     ", THEME_LABEL);
    vga_puts_color(": ", THEME_DIM);
    vga_puts_color("4-bit (16 colors)\n", THEME_VALUE);

    vga_puts("\n");
}

// Exibe ajuda
static void help(void) {
    vga_puts_color("LeonardOS", THEME_TITLE);
    vga_puts_color(" - Comandos:\n", THEME_DEFAULT);

    vga_puts_color("  help    ", THEME_INFO);
    vga_puts_color("- exibe este texto\n", THEME_DEFAULT);

    vga_puts_color("  clear   ", THEME_INFO);
    vga_puts_color("- limpa a tela\n", THEME_DEFAULT);

    vga_puts_color("  sysinfo ", THEME_INFO);
    vga_puts_color("- exibe informações do sistema\n", THEME_DEFAULT);

    vga_puts_color("  halt    ", THEME_INFO);
    vga_puts_color("- desliga o kernel\n", THEME_DEFAULT);
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
        vga_puts_color("Comando desconhecido: ", THEME_ERROR);
        vga_puts_color(cmd, THEME_WARNING);
        vga_puts("\n");
    }
}

// Loop principal do shell
void shell_loop(void) {
    static char cmd_buf[256];
    
    vga_set_color(THEME_DEFAULT);
    vga_puts_color("LeonardOS v0.1", THEME_TITLE);
    vga_puts(" - Digite '");
    vga_puts_color("help", THEME_INFO);
    vga_puts("' para ajuda\n");
    
    while (1) {
        vga_puts_color("> ", THEME_PROMPT);
        vga_set_color(THEME_DEFAULT);
        kbd_read_line(cmd_buf, sizeof(cmd_buf));
        vga_putchar('\n');
        
        if (cmd_buf[0] != 0) {
            process_command(cmd_buf);
        }
    }
}
