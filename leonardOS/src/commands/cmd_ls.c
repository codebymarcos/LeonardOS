// LeonardOS - Comando: ls
// Lista conteúdo de um diretório via VFS
//
// Uso:
//   ls        → lista raiz (/)
//   ls /etc   → lista /etc

#include "cmd_ls.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../fs/vfs.h"

void cmd_ls(const char *args) {
    // Path padrão: raiz
    const char *path = "/";
    if (args && args[0] != '\0') {
        path = args;
    }

    vfs_node_t *dir = vfs_open(path);
    if (!dir) {
        vga_puts_color("ls: caminho nao encontrado: ", THEME_ERROR);
        vga_puts_color(path, THEME_WARNING);
        vga_putchar('\n');
        return;
    }

    if (!(dir->type & VFS_DIRECTORY)) {
        // É um arquivo — mostra apenas o nome e tamanho
        vga_puts_color(dir->name, THEME_INFO);
        vga_puts_color("  ", THEME_DEFAULT);
        vga_putint(dir->size);
        vga_puts_color(" bytes\n", THEME_DIM);
        return;
    }

    // Lista entradas do diretório
    vga_puts_color("Conteudo de ", THEME_LABEL);
    vga_puts_color(path, THEME_VALUE);
    vga_puts_color(":\n\n", THEME_LABEL);

    uint32_t index = 0;
    vfs_node_t *entry;
    int count = 0;

    while ((entry = vfs_readdir(dir, index)) != NULL) {
        vga_puts_color("  ", THEME_DEFAULT);

        if (entry->type & VFS_DIRECTORY) {
            vga_puts_color("[DIR]  ", THEME_INFO);
            vga_puts_color(entry->name, THEME_INFO);
        } else {
            vga_puts_color("[ARQ]  ", THEME_DIM);
            vga_puts_color(entry->name, THEME_DEFAULT);
            vga_puts_color("  ", THEME_DEFAULT);
            vga_putint(entry->size);
            vga_puts_color("B", THEME_DIM);
        }

        vga_putchar('\n');
        index++;
        count++;
    }

    if (count == 0) {
        vga_puts_color("  (vazio)\n", THEME_DIM);
    }

    vga_putchar('\n');
}
