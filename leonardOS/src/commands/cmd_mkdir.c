// LeonardOS - Comando: mkdir
// Cria um diretório no filesystem
//
// Uso:
//   mkdir meudir         → cria no diretório atual
//   mkdir /tmp/subdir    → caminho absoluto
//   mkdir ../novo        → relativo com ..

#include "cmd_mkdir.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../common/string.h"
#include "../fs/vfs.h"
#include "../fs/ramfs.h"
#include "../fs/leonfs.h"
#include "../shell/shell.h"

void cmd_mkdir(const char *args) {
    if (!args || args[0] == '\0') {
        vga_puts_color("Uso: mkdir <caminho>\n", THEME_DIM);
        return;
    }

    // Pula espaços
    while (*args == ' ') args++;
    if (*args == '\0') {
        vga_puts_color("Uso: mkdir <caminho>\n", THEME_DIM);
        return;
    }

    // Constrói path absoluto canônico
    char full_path[256];
    if (!vfs_build_path(current_path, args, full_path, 256)) {
        vga_puts_color("mkdir: caminho invalido\n", THEME_ERROR);
        return;
    }

    // Separa diretório pai e nome do novo diretório
    int path_len = kstrlen(full_path);
    int last_slash = -1;
    for (int i = 0; i < path_len; i++) {
        if (full_path[i] == '/') last_slash = i;
    }

    char parent_path[256];
    char dir_name[64];

    if (last_slash <= 0) {
        // Criar na raiz
        kstrcpy(parent_path, "/", 256);
        kstrcpy(dir_name, full_path + 1, 64);
    } else {
        // Copia parent_path (até last_slash)
        int di = 0;
        for (int i = 0; i < last_slash && di < 255; i++) {
            parent_path[di++] = full_path[i];
        }
        parent_path[di] = '\0';

        // Nome é tudo após last_slash
        kstrcpy(dir_name, full_path + last_slash + 1, 64);
    }

    if (dir_name[0] == '\0') {
        vga_puts_color("mkdir: nome invalido\n", THEME_ERROR);
        return;
    }

    // Resolve pai
    vfs_node_t *parent = vfs_open(parent_path);
    if (!parent || !(parent->type & VFS_DIRECTORY)) {
        vga_puts_color("mkdir: pai nao encontrado: ", THEME_ERROR);
        vga_puts_color(parent_path, THEME_WARNING);
        vga_putchar('\n');
        return;
    }

    // Verifica se já existe
    vfs_node_t *existing = vfs_finddir(parent, dir_name);
    if (existing) {
        vga_puts_color("mkdir: ja existe: ", THEME_WARNING);
        vga_puts_color(full_path, THEME_INFO);
        vga_putchar('\n');
        return;
    }

    // Cria no FS correto
    vfs_node_t *created;
    if (leonfs_is_node(parent)) {
        created = leonfs_create_dir(parent, dir_name);
    } else {
        created = ramfs_create_dir(parent, dir_name);
    }
    if (!created) {
        vga_puts_color("mkdir: falha ao criar diretorio\n", THEME_ERROR);
        return;
    }

    vga_puts_color("Criado: ", THEME_DIM);
    vga_puts_color(full_path, THEME_INFO);
    vga_putchar('/');
    vga_putchar('\n');
}
