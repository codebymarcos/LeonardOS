// LeonardOS - Comando: help
// Exibe a lista de comandos disponíveis

#include "cmd_help.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"

void cmd_help(const char *args) {
    (void)args;

    const command_t *cmds = commands_get_all();
    int count = commands_get_count();

    vga_puts_color("\n", THEME_DEFAULT);
    vga_puts_color("     _.--._  _.--._\n", THEME_BANNER);
    vga_puts_color("  ,-=.-\":;:;:;\\'\\':;:;:;\"-._\n", THEME_BANNER);
    vga_puts_color("  \\\\\\:;:;:;:;:;\\:;:;:;:;:;\\\n", THEME_BANNER);
    vga_puts_color("   \\\\\\:;:;:;:;:;\\:;:;:;:;:;\\\n", THEME_BANNER);
    vga_puts_color("    \\\\\\:;:;:;:;:;\\:;:;:;:;:;\\\n", THEME_BANNER);
    vga_puts_color("     \\\\\\:;:;:;:;:;\\:;::;:;:;:\\\n", THEME_BANNER);
    vga_puts_color("      \\\\\\;:;::;:;:;\\:;:;:;::;:\\\n", THEME_BANNER);
    vga_puts_color("       \\\\\\;;:;:_:--:\\:_:--:_;:;\\", THEME_BANNER);
    vga_puts_color("    -leonardOS\n", THEME_TITLE);
    vga_puts_color("        \\\\\\_.-\"      :      \"-._\\\n", THEME_BANNER);
    vga_puts_color("         \\`_..--\"\"--. ;.--\"\"--.._ =>\n", THEME_BANNER);
    vga_puts_color("          \"\n\n", THEME_BANNER);

    vga_puts_color("  Comandos disponiveis:\n", THEME_DEFAULT);

    for (int i = 0; i < count; i++) {
        vga_puts_color("    ", THEME_DEFAULT);
        vga_puts_color(cmds[i].name, THEME_INFO);

        // Padding para alinhar descrições
        int name_len = 0;
        const char *p = cmds[i].name;
        while (*p++) name_len++;
        for (int pad = name_len; pad < 10; pad++) {
            vga_putchar(' ');
        }

        vga_puts_color("- ", THEME_DIM);
        vga_puts_color(cmds[i].description, THEME_DEFAULT);
        vga_putchar('\n');
    }
    vga_putchar('\n');
}
