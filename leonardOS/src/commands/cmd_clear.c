// LeonardOS - Comando: clear
// Limpa a tela

#include "cmd_clear.h"
#include "../drivers/vga/vga.h"

void cmd_clear(const char *args) {
    (void)args;
    vga_clear();
}
