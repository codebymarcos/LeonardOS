// LeonardOS - Comando: rm
// Remove arquivo ou diretório vazio do filesystem
//
// Uso:
//   rm arquivo.txt         → remove no diretório atual
//   rm /tmp/test.txt       → caminho absoluto
//   rm ../tmp/test.txt     → relativo com ..
//   rm -r /tmp/subdir      → remove diretório (mesmo com filhos)
//
// Diretórios sem -r só são removidos se estiverem vazios.
// Não permite remover "/" nem o diretório atual.

#include "cmd_rm.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../common/string.h"
#include "../fs/vfs.h"
#include "../fs/ramfs.h"
#include "../shell/shell.h"

// Remove recursivo: remove todos os filhos e depois o próprio nó
static bool rm_recursive(vfs_node_t *parent, vfs_node_t *node) {
    if (!parent || !node) return false;

    // Se é diretório, remove filhos primeiro
    if (node->type & VFS_DIRECTORY) {
        ramfs_data_t *rd = (ramfs_data_t *)node->fs_data;
        if (rd) {
            // Remove filhos de trás pra frente (child_count diminui a cada remoção)
            while (rd->child_count > 0) {
                vfs_node_t *child = rd->children[0];
                if (!rm_recursive(node, child)) return false;
            }
        }
    }

    // Agora o nó está vazio (ou é arquivo), remove do pai
    return ramfs_remove(parent, node->name);
}

void cmd_rm(const char *args) {
    if (!args || args[0] == '\0') {
        vga_puts_color("Uso: rm [-r] <caminho>\n", THEME_DIM);
        return;
    }

    // Pula espaços
    while (*args == ' ') args++;
    if (*args == '\0') {
        vga_puts_color("Uso: rm [-r] <caminho>\n", THEME_DIM);
        return;
    }

    // Verifica flag -r (recursivo)
    bool recursive = false;
    if (args[0] == '-' && args[1] == 'r' && (args[2] == ' ' || args[2] == '\0')) {
        recursive = true;
        args += 2;
        while (*args == ' ') args++;
        if (*args == '\0') {
            vga_puts_color("rm: caminho ausente\n", THEME_ERROR);
            return;
        }
    }

    // Constrói path absoluto canônico
    char full_path[256];
    if (!vfs_build_path(current_path, args, full_path, 256)) {
        vga_puts_color("rm: caminho invalido\n", THEME_ERROR);
        return;
    }

    // Não permite remover "/"
    if (full_path[0] == '/' && full_path[1] == '\0') {
        vga_puts_color("rm: nao pode remover '/'\n", THEME_ERROR);
        return;
    }

    // Não permite remover o diretório atual
    if (kstrcmp(full_path, current_path) == 0) {
        vga_puts_color("rm: nao pode remover diretorio atual\n", THEME_ERROR);
        return;
    }

    // Resolve o alvo
    vfs_node_t *target = vfs_open(full_path);
    if (!target) {
        vga_puts_color("rm: nao encontrado: ", THEME_ERROR);
        vga_puts_color(args, THEME_WARNING);
        vga_putchar('\n');
        return;
    }

    // Encontra o pai — pega tudo antes do último '/'
    char parent_path[256];
    char target_name[64];
    int path_len = kstrlen(full_path);
    int last_slash = -1;
    for (int i = 0; i < path_len; i++) {
        if (full_path[i] == '/') last_slash = i;
    }

    if (last_slash <= 0) {
        kstrcpy(parent_path, "/", 256);
        kstrcpy(target_name, full_path + 1, 64);
    } else {
        int di = 0;
        for (int i = 0; i < last_slash && di < 255; i++) {
            parent_path[di++] = full_path[i];
        }
        parent_path[di] = '\0';
        kstrcpy(target_name, full_path + last_slash + 1, 64);
    }

    vfs_node_t *parent = vfs_open(parent_path);
    if (!parent) {
        vga_puts_color("rm: pai nao encontrado\n", THEME_ERROR);
        return;
    }

    // Diretório com filhos precisa de -r
    if (target->type & VFS_DIRECTORY) {
        ramfs_data_t *rd = (ramfs_data_t *)target->fs_data;
        if (rd && rd->child_count > 0 && !recursive) {
            vga_puts_color("rm: diretorio nao vazio (use -r): ", THEME_ERROR);
            vga_puts_color(args, THEME_WARNING);
            vga_putchar('\n');
            return;
        }
    }

    // Remove
    bool ok;
    if (recursive && (target->type & VFS_DIRECTORY)) {
        ok = rm_recursive(parent, target);
    } else {
        ok = ramfs_remove(parent, target_name);
    }

    if (!ok) {
        vga_puts_color("rm: falha ao remover: ", THEME_ERROR);
        vga_puts_color(args, THEME_WARNING);
        vga_putchar('\n');
        return;
    }

    vga_puts_color("Removido: ", THEME_DIM);
    vga_puts_color(full_path, THEME_INFO);
    vga_putchar('\n');
}
