// LeonardOS - Comando: halt
// Desliga o kernel

#include "cmd_halt.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"

void cmd_halt(const char *args) {
    (void)args;
    vga_puts_color("Desligando...\n", THEME_WARNING);
    vga_puts_color("(Sistema parado. Feche o QEMU ou pressione Ctrl+C)\n", THEME_DIM);
    
    // Desabilita interrupções
    asm volatile("cli");
    
    // Loop infinito com HLT (CPU aguarda interrupção, economiza energia)
    while (1) {
        asm volatile("hlt");
    }
}
