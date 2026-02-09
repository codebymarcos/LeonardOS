// LeonardOS - Comando: head
// Exibe as primeiras N linhas de um arquivo ou entrada
//
// Uso:
//   head arquivo.txt         → primeiras 10 linhas
//   head -n 5 arquivo.txt    → primeiras 5 linhas
//   cat arquivo | head -n 3  → via pipe

#include "cmd_head.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../common/string.h"
#include "../fs/vfs.h"
#include "../shell/shell.h"

static void print_head(const char *data, int len, int max_lines) {
    int lines = 0;
    for (int i = 0; i < len && lines < max_lines; i++) {
        vga_putchar(data[i]);
        if (data[i] == '\n') lines++;
    }
    if (len > 0 && data[len - 1] != '\n') vga_putchar('\n');
}

void cmd_head(const char *args) {
    if (!args || args[0] == '\0') {
        vga_puts_color("head: uso: head [-n N] <arquivo>\n", THEME_ERROR);
        return;
    }

    int max_lines = 10;
    const char *p = args;

    // Parse -n N
    while (*p == ' ') p++;
    if (*p == '-' && *(p + 1) == 'n') {
        p += 2;
        while (*p == ' ') p++;
        max_lines = 0;
        while (*p >= '0' && *p <= '9') {
            max_lines = max_lines * 10 + (*p - '0');
            p++;
        }
        while (*p == ' ') p++;
        if (max_lines <= 0) max_lines = 10;
    }

    // Tenta abrir como arquivo
    vfs_node_t *node = vfs_resolve(p, current_dir, NULL, 0);
    if (node && !(node->type & VFS_DIRECTORY)) {
        static uint8_t buf[4096];
        uint32_t bytes = vfs_read(node, 0, node->size < 4096 ? node->size : 4096, buf);
        print_head((const char *)buf, (int)bytes, max_lines);
        return;
    }

    // Senão, trata como texto piped
    print_head(p, kstrlen(p), max_lines);
}
