// LeonardOS - Comando: sysinfo
// Exibe informações do sistema

#include "cmd_sysinfo.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"

void cmd_sysinfo(const char *args) {
    (void)args;

    vga_puts("\n");
    vga_puts_color("  ╔═════════════════════════════════════╗\n", THEME_BORDER);
    vga_puts_color("  ║", THEME_BORDER);
    vga_puts_color("      LeonardOS Sysinfo             ", THEME_TITLE);
    vga_puts_color(" ║\n", THEME_BORDER);
    vga_puts_color("  ╚═════════════════════════════════════╝\n", THEME_BORDER);
    vga_puts("\n");

    vga_puts_color("  OS              ", THEME_LABEL);
    vga_puts_color(": ", THEME_DIM);
    vga_puts_color("LeonardOS v1.0.0\n", THEME_VALUE);

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
