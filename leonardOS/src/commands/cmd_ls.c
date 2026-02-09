// LeonardOS - Comando: ls
// Lista conteúdo de um diretório via VFS
//
// Uso:
//   ls          → lista diretório atual (pwd)
//   ls /etc     → lista /etc (absoluto)
//   ls tmp      → lista tmp (relativo ao pwd)
//   ls ..       → lista diretório pai
//   ls arquivo  → mostra info de arquivo

#include "cmd_ls.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../common/string.h"
#include "../fs/vfs.h"
#include "../shell/shell.h"

void cmd_ls(const char *args) {
    vfs_node_t *dir = NULL;
    char display_path[256];

    if (!args || args[0] == '\0') {
        // Sem argumento: usa diretório atual
        dir = current_dir;
        kstrcpy(display_path, current_path, 256);
    } else {
        // Pula espaços
        while (*args == ' ') args++;

        // Resolve path (absoluto ou relativo)
        dir = vfs_resolve(args, current_dir, display_path, 256);
    }

    if (!dir) {
        vga_puts_color("ls: caminho nao encontrado: ", THEME_ERROR);
        vga_puts_color(args ? args : "(null)", THEME_WARNING);
        vga_putchar('\n');
        return;
    }

    if (!(dir->type & VFS_DIRECTORY)) {
        // É um arquivo — mostra info detalhada
        vga_puts_color("  ", THEME_DEFAULT);
        vga_puts_color(dir->name, THEME_INFO);
        vga_puts_color("  ", THEME_DEFAULT);
        vga_putint(dir->size);
        vga_puts_color(" bytes", THEME_DIM);
        vga_puts_color("  [ARQ]\n", THEME_DIM);
        return;
    }

    // Cabeçalho
    vga_puts_color("Conteudo de ", THEME_LABEL);
    vga_puts_color(display_path, THEME_VALUE);
    vga_puts_color(":\n\n", THEME_LABEL);

    // Lista entradas
    uint32_t index = 0;
    vfs_node_t *entry;
    int dirs = 0;
    int files = 0;

    while ((entry = vfs_readdir(dir, index)) != NULL) {
        vga_puts_color("  ", THEME_DEFAULT);

        if (entry->type & VFS_DIRECTORY) {
            vga_puts_color("[DIR]  ", THEME_INFO);
            vga_puts_color(entry->name, THEME_INFO);
            vga_putchar('/');
            dirs++;
        } else {
            vga_puts_color("[ARQ]  ", THEME_DIM);
            vga_puts_color(entry->name, THEME_DEFAULT);

            // Alinha tamanho
            int nlen = kstrlen(entry->name);
            for (int pad = nlen; pad < 16; pad++) {
                vga_putchar(' ');
            }

            vga_putint(entry->size);
            vga_puts_color("B", THEME_DIM);
            files++;
        }

        vga_putchar('\n');
        index++;
    }

    if (dirs + files == 0) {
        vga_puts_color("  (vazio)\n", THEME_DIM);
    }

    // Resumo
    vga_putchar('\n');
    vga_puts_color("  ", THEME_DIM);
    vga_putint(dirs);
    vga_puts_color(" dir, ", THEME_DIM);
    vga_putint(files);
    vga_puts_color(" arq\n", THEME_DIM);
}
