// LeonardOS - Comando: tree
// Exibe a árvore de diretórios recursivamente
//
// Uso:
//   tree              → mostra a partir do diretório atual
//   tree /            → mostra a partir da raiz
//   tree /mnt         → mostra a partir de /mnt

#include "cmd_tree.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../common/string.h"
#include "../fs/vfs.h"
#include "../shell/shell.h"

// Contadores globais para o tree
static int tree_dirs;
static int tree_files;

// Profundidade máxima para evitar stack overflow (stack = 8KB)
#define TREE_MAX_DEPTH 8

static void tree_recurse(vfs_node_t *node, int depth, int is_last[], int level) {
    if (depth >= TREE_MAX_DEPTH) {
        // Prefix
        for (int i = 0; i < level; i++) {
            if (is_last[i])
                vga_puts_color("    ", THEME_DIM);
            else
                vga_puts_color("│   ", THEME_DIM);
        }
        vga_puts_color("... (profundidade maxima)\n", THEME_WARNING);
        return;
    }

    // Lista todos os filhos
    uint32_t count = 0;
    while (vfs_readdir(node, count) != NULL) count++;

    for (uint32_t i = 0; i < count; i++) {
        vfs_node_t *child = vfs_readdir(node, i);
        if (!child) break;

        int last = (i == count - 1);

        // Desenha prefix
        for (int j = 0; j < level; j++) {
            if (is_last[j])
                vga_puts_color("    ", THEME_DIM);
            else
                vga_puts_color("│   ", THEME_DIM);
        }

        // Conector
        if (last)
            vga_puts_color("└── ", THEME_DIM);
        else
            vga_puts_color("├── ", THEME_DIM);

        // Nome com cor
        if (child->type & VFS_DIRECTORY) {
            vga_puts_color(child->name, THEME_DIR);
            vga_putchar('\n');
            tree_dirs++;

            // Recurse
            is_last[level] = last;
            tree_recurse(child, depth + 1, is_last, level + 1);
        } else {
            vga_puts_color(child->name, THEME_FILE);
            // Tamanho
            if (child->size > 0) {
                vga_puts_color(" (", THEME_DIM);
                vga_putint(child->size);
                vga_puts_color("B)", THEME_DIM);
            }
            vga_putchar('\n');
            tree_files++;
        }
    }
}

void cmd_tree(const char *args) {
    vfs_node_t *start;
    const char *label;

    if (!args || args[0] == '\0') {
        start = current_dir;
        label = current_path;
    } else {
        // Pula espaços
        while (*args == ' ') args++;
        if (*args == '\0') {
            start = current_dir;
            label = current_path;
        } else {
            char clean[256];
            kstrcpy(clean, args, 256);
            int len = kstrlen(clean);
            while (len > 0 && clean[len - 1] == ' ') len--;
            clean[len] = '\0';

            char resolved[256];
            start = vfs_resolve(clean, current_dir, resolved, 256);
            if (!start) {
                vga_puts_color("tree: nao encontrado: ", THEME_ERROR);
                vga_puts_color(clean, THEME_WARNING);
                vga_putchar('\n');
                return;
            }
            if (!(start->type & VFS_DIRECTORY)) {
                vga_puts_color("tree: nao e diretorio: ", THEME_ERROR);
                vga_puts_color(clean, THEME_WARNING);
                vga_putchar('\n');
                return;
            }
            label = clean;
        }
    }

    // Reset contadores
    tree_dirs = 0;
    tree_files = 0;

    // Raiz da árvore
    vga_puts_color(label, THEME_DIR);
    vga_putchar('\n');

    // Recursão
    int is_last[TREE_MAX_DEPTH];
    for (int i = 0; i < TREE_MAX_DEPTH; i++) is_last[i] = 0;

    tree_recurse(start, 0, is_last, 0);

    // Resumo
    vga_putchar('\n');
    vga_putint(tree_dirs);
    vga_puts_color(" diretorios, ", THEME_DIM);
    vga_putint(tree_files);
    vga_puts_color(" arquivos\n", THEME_DIM);
}
