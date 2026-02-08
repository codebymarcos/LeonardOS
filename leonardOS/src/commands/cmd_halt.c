// LeonardOS - Comando: halt
// Desliga o kernel

#include "cmd_halt.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"

void cmd_halt(const char *args) {
    (void)args;
    vga_puts_color("Desligando...\n", THEME_WARNING);
    asm volatile("cli");
    asm volatile("hlt");
    while (1);
}
