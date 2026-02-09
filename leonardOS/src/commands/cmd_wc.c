// LeonardOS - Comando: wc
// Conta linhas, palavras e bytes de um arquivo ou de stdin (pipe)
//
// Uso:
//   wc arquivo.txt          → linhas, palavras, bytes do arquivo
//   cat arquivo.txt | wc    → conta da entrada via pipe

#include "cmd_wc.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../common/string.h"
#include "../fs/vfs.h"
#include "../shell/shell.h"

static void wc_count(const char *data, int len) {
    int lines = 0, words = 0, bytes = len;
    int in_word = 0;

    for (int i = 0; i < len; i++) {
        if (data[i] == '\n') lines++;
        if (data[i] == ' ' || data[i] == '\n' || data[i] == '\t') {
            in_word = 0;
        } else {
            if (!in_word) { words++; in_word = 1; }
        }
    }

    vga_puts("  ");
    vga_putint(lines);
    vga_puts("  ");
    vga_putint(words);
    vga_puts("  ");
    vga_putint(bytes);
}

void cmd_wc(const char *args) {
    if (!args || args[0] == '\0') {
        vga_puts_color("wc: uso: wc <arquivo> ou via pipe\n", THEME_ERROR);
        return;
    }

    // Tenta abrir como arquivo primeiro
    vfs_node_t *node = vfs_resolve(args, current_dir, NULL, 0);
    if (node && !(node->type & VFS_DIRECTORY)) {
        // É um arquivo
        static uint8_t buf[4096];
        uint32_t bytes = vfs_read(node, 0, node->size < 4096 ? node->size : 4096, buf);
        wc_count((const char *)buf, (int)bytes);
        vga_puts("  ");
        vga_puts_color(args, THEME_FILE);
        vga_putchar('\n');
        return;
    }

    // Senão, trata args como texto piped (entrada direta)
    int len = kstrlen(args);
    wc_count(args, len);
    vga_putchar('\n');
}
