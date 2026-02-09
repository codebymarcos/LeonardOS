// LeonardOS - Comando: cat
// Exibe conteúdo de um arquivo via VFS
//
// Uso:
//   cat /etc/hostname     → caminho absoluto
//   cat hostname           → relativo ao pwd
//   cat ../etc/version     → relativo com ..

#include "cmd_cat.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../fs/vfs.h"
#include "../shell/shell.h"

void cmd_cat(const char *args) {
    if (!args || args[0] == '\0') {
        vga_puts_color("Uso: cat <caminho>\n", THEME_DIM);
        return;
    }

    // Pula espaços
    while (*args == ' ') args++;
    if (*args == '\0') {
        vga_puts_color("Uso: cat <caminho>\n", THEME_DIM);
        return;
    }

    // Resolve via VFS (absoluto ou relativo)
    vfs_node_t *node = vfs_resolve(args, current_dir, NULL, 0);
    if (!node) {
        vga_puts_color("cat: nao encontrado: ", THEME_ERROR);
        vga_puts_color(args, THEME_WARNING);
        vga_putchar('\n');
        return;
    }

    if (node->type & VFS_DIRECTORY) {
        vga_puts_color("cat: e um diretorio: ", THEME_ERROR);
        vga_puts_color(args, THEME_WARNING);
        vga_putchar('\n');
        return;
    }

    if (node->size == 0) {
        vga_puts_color("(arquivo vazio)\n", THEME_DIM);
        return;
    }

    // Lê e exibe conteúdo
    uint8_t buf[512];
    uint32_t offset = 0;

    while (offset < node->size) {
        uint32_t chunk = node->size - offset;
        if (chunk > sizeof(buf) - 1) chunk = sizeof(buf) - 1;

        uint32_t bytes = vfs_read(node, offset, chunk, buf);
        if (bytes == 0) break;

        for (uint32_t i = 0; i < bytes; i++) {
            vga_putchar((char)buf[i]);
        }

        offset += bytes;
    }

    vga_putchar('\n');
}
