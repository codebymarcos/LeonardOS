// LeonardOS - Comando: source
// Executa um script .sh no contexto do shell atual
//
// Uso:
//   source script.sh     → executa script.sh
//   source /tmp/setup.sh → caminho absoluto

#include "cmd_source.h"
#include "../shell/script.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"

void cmd_source(const char *args) {
    if (!args || args[0] == '\0') {
        vga_puts_color("source: uso: source <script.sh>\n", THEME_ERROR);
        return;
    }

    // Remove espaços
    while (*args == ' ') args++;

    script_execute_file(args);
}
