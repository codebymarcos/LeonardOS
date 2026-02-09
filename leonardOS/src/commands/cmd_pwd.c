// LeonardOS - Comando: pwd
// Exibe o diretório atual

#include "cmd_pwd.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../shell/shell.h"

void cmd_pwd(const char *args) {
    (void)args;  // Não usa argumentos

    if (!current_dir) {
        vga_puts_color("Erro: diretório atual inválido\n", THEME_ERROR);
        return;
    }

    vga_puts_color(current_path, THEME_INFO);
    vga_putchar('\n');
}
