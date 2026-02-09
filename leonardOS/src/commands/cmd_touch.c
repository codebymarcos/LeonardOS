// LeonardOS - Comando: touch
// Cria um arquivo vazio no filesystem
//
// Uso:
//   touch arquivo.txt         → cria no diretório atual
//   touch /tmp/novo.txt       → caminho absoluto
//   touch ../etc/config       → relativo com ..

#include "cmd_touch.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../common/string.h"
#include "../fs/vfs.h"
#include "../fs/ramfs.h"
#include "../shell/shell.h"

void cmd_touch(const char *args) {
    if (!args || args[0] == '\0') {
        vga_puts_color("Uso: touch <caminho>\n", THEME_DIM);
        return;
    }

    // Pula espaços
    while (*args == ' ') args++;
    if (*args == '\0') {
        vga_puts_color("Uso: touch <caminho>\n", THEME_DIM);
        return;
    }

    // Constrói path absoluto canônico
    char full_path[256];
    if (!vfs_build_path(current_path, args, full_path, 256)) {
        vga_puts_color("touch: caminho invalido\n", THEME_ERROR);
        return;
    }

    // Verifica se já existe
    vfs_node_t *existing = vfs_open(full_path);
    if (existing) {
        // Arquivo já existe — não faz nada (como Unix touch)
        return;
    }

    // Separa diretório pai e nome do arquivo
    int path_len = kstrlen(full_path);
    int last_slash = -1;
    for (int i = 0; i < path_len; i++) {
        if (full_path[i] == '/') last_slash = i;
    }

    char parent_path[256];
    char file_name[64];

    if (last_slash <= 0) {
        kstrcpy(parent_path, "/", 256);
        kstrcpy(file_name, full_path + 1, 64);
    } else {
        int di = 0;
        for (int i = 0; i < last_slash && di < 255; i++) {
            parent_path[di++] = full_path[i];
        }
        parent_path[di] = '\0';

        kstrcpy(file_name, full_path + last_slash + 1, 64);
    }

    if (file_name[0] == '\0') {
        vga_puts_color("touch: nome invalido\n", THEME_ERROR);
        return;
    }

    // Resolve pai
    vfs_node_t *parent = vfs_open(parent_path);
    if (!parent || !(parent->type & VFS_DIRECTORY)) {
        vga_puts_color("touch: diretorio nao encontrado: ", THEME_ERROR);
        vga_puts_color(parent_path, THEME_WARNING);
        vga_putchar('\n');
        return;
    }

    // Cria arquivo
    vfs_node_t *created = ramfs_create_file(parent, file_name);
    if (!created) {
        vga_puts_color("touch: falha ao criar arquivo\n", THEME_ERROR);
        return;
    }

    vga_puts_color("Criado: ", THEME_DIM);
    vga_puts_color(full_path, THEME_INFO);
    vga_putchar('\n');
}
