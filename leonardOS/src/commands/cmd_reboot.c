// LeonardOS - Comando: reboot
// Reinicia o sistema via controlador de teclado PS/2 (CPU reset line)

#include "cmd_reboot.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../common/io.h"

void cmd_reboot(const char *args) {
    (void)args;
    vga_puts_color("Reiniciando...\n", THEME_WARNING);

    // Desabilita interrupções
    asm volatile("cli");

    // Método 1: Pulse CPU reset via controlador de teclado 8042
    // Espera o buffer de entrada do 8042 esvaziar
    uint8_t status;
    do {
        status = inb(0x64);
    } while (status & 0x02);

    // Envia comando 0xFE (CPU reset) para porta 0x64
    outb(0x64, 0xFE);

    // Se não funcionou, halt
    asm volatile("hlt");
    while (1);
}
