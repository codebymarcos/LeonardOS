// LeonardOS - Shell
// Interface interativa com sistema modular de comandos

#include "shell.h"
#include "../drivers/vga/vga.h"
#include "../drivers/keyboard/keyboard.h"
#include "../common/colors.h"
#include "../commands/commands.h"

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
            if (!commands_execute(cmd_buf)) {
                // Extrai apenas o nome do comando para a mensagem de erro
                const char *p = cmd_buf;
                while (*p == ' ') p++;
                vga_puts_color("Comando desconhecido: ", THEME_ERROR);
                vga_puts_color(p, THEME_WARNING);
                vga_puts("\n");
            }
        }
    }
}
