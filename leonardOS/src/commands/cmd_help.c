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
    vga_puts_color("             a          a       \n", THEME_BANNER);
    vga_puts_color("             aaa        aaa     \n", THEME_BANNER);
    vga_puts_color("            aaaaaaaaaaaaaaaa    \n", THEME_BANNER);
    vga_puts_color("           aaaaaaaaaaaaaaaaaa   \n", THEME_BANNER);
    vga_puts_color("          aaaaafaaaaaaafaaaaaa  \n", THEME_BANNER);
    vga_puts_color("          aaaaaaaaaaaaaaaaaaaa  \n", THEME_BANNER);
    vga_puts_color("           aaaaaaaaaaaaaaaaaa   \n", THEME_BANNER);
    vga_puts_color("            aaaaaaa  aaaaaaa    \n", THEME_BANNER);
    vga_puts_color("             aaaaaaaaaaaaaa     \n", THEME_BANNER);
    vga_puts_color("  a         aaaaaaaaaaaaaaaa    \n", THEME_BANNER);
    vga_puts_color(" aaa       aaaaaaaaaaaaaaaaaa   \n", THEME_BANNER);
    vga_puts_color(" aaa      aaaaaaaaaaaaaaaaaaaa  \n", THEME_BANNER);
    vga_puts_color(" aaa     aaaaaaaaaaaaaaaaaaaaaa \n", THEME_BANNER);
    vga_puts_color(" aaa    aaaaaaaaaaaaaaaaaaaaaaaa\n", THEME_BANNER);
    vga_puts_color("  aaa   aaaaaaaaaaaaaaaaaaaaaaaa\n", THEME_BANNER);
    vga_puts_color("  aaa   aaaaaaaaaaaaaaaaaaaaaaaa\n", THEME_BANNER);
    vga_puts_color("  aaa    aaaaaaaaaaaaaaaaaaaaaa \n", THEME_BANNER);
    vga_puts_color("   aaa    aaaaaaaaaaaaaaaaaaaa  \n", THEME_BANNER);
    vga_puts_color("    aaaaaaaaaaaaaaaaaaaaaaaaaa  \n", THEME_BANNER);
    vga_puts_color("     aaaaaaaaaaaaaaaaaaaaaaaaa  \n", THEME_BANNER);
    vga_puts_color("                 leonardOS\n\n", THEME_TITLE);

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
